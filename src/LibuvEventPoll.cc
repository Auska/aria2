/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2010 Tatsuhiro Tsujikawa
 * Copyright (C) 2013 Tatsuhiro Tsujikawa, Nils Maier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */

#include "LibuvEventPoll.h"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <stdexcept>

#include <uv.h>

#include "Command.h"
#include "LogFactory.h"
#include "Logger.h"
#include "a2functional.h"
#include "fmt.h"
#include "util.h"

namespace {
using namespace aria2;

static void timer_callback(uv_timer_t* handle) { uv_stop(handle->loop); }
} // namespace

namespace aria2 {

LibuvEventPoll::KSocketEntry::KSocketEntry(sock_t s)
    : SocketEntry<KCommandEvent, KADNSEvent>(s)
{
}

inline int accumulateEvent(int events, const LibuvEventPoll::KEvent& event)
{
  return events | event.getEvents();
}

int LibuvEventPoll::KSocketEntry::getEvents() const
{
  int events = 0;
#ifdef ENABLE_ASYNC_DNS
  events =
      std::accumulate(adnsEvents_.begin(), adnsEvents_.end(),
                      std::accumulate(commandEvents_.begin(),
                                      commandEvents_.end(), 0, accumulateEvent),
                      accumulateEvent);
#else  // !ENABLE_ASYNC_DNS
  events = std::accumulate(commandEvents_.begin(), commandEvents_.end(), 0,
                           accumulateEvent);
#endif // !ENABLE_ASYNC_DNS

  return events;
}

LibuvEventPoll::LibuvEventPoll() : loop_(nullptr), timerInited_(false)
{
  loop_ = uv_loop_new();
  if (!loop_) {
    return;
  }
  if (uv_timer_init(loop_, &timer_) < 0) {
    A2_LOG_WARN("uv_timer_init failed");
    return;
  }
  timerInited_ = true;
}

LibuvEventPoll::~LibuvEventPoll() noexcept
{
  for (auto& p : polls_) {
    p.second->close();
  }
  if (loop_) {
    if (timerInited_) {
      uv_timer_stop(&timer_);
      uv_close((uv_handle_t*)&timer_, nullptr);
    }
    // Actually kill the polls, and timers, if any.
    uv_run(loop_, static_cast<uv_run_mode>(UV_RUN_ONCE | UV_RUN_NOWAIT));

    uv_loop_delete(loop_);
    loop_ = nullptr;
  }

  // Need this to free only after the loop is gone.
  polls_.clear();
}

void LibuvEventPoll::poll(const struct timeval& tv)
{
  if (!timerInited_) {
    return;
  }

  const int64_t timeout =
      static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;

  // timeout == 0 will tick once
  int r = uv_timer_start(&timer_, timer_callback,
                         std::max<int64_t>(timeout, 0), 0);
  if (r < 0) {
    A2_LOG_WARN(fmt("uv_timer_start failed: {}", uv_err_name(r)));
    return;
  }

  uv_run(loop_, UV_RUN_DEFAULT);
  uv_timer_stop(&timer_);

#ifdef ENABLE_ASYNC_DNS
  // It turns out that we have to call ares_process_fd before ares's
  // own timeout and ares may create new sockets or closes socket in
  // their API. So we call ares_process_fd for all ares_channel and
  // re-register their sockets.
  for (auto& r : nameResolverEntries_) {
    auto& ent = r.second;
    ent.processTimeout();
    ent.removeSocketEvents(this);
    ent.addSocketEvents(this);
  }
#endif // ENABLE_ASYNC_DNS

  // TODO timeout of name resolver is determined in Command(AbstractCommand,
  // DHTEntryPoint...Command)
}

int LibuvEventPoll::translateEvents(EventPoll::EventType events)
{
  int newEvents = 0;
  if (EventPoll::EventType::READ & events) {
    newEvents |= IEV_READ;
  }
  if (EventPoll::EventType::WRITE & events) {
    newEvents |= IEV_WRITE;
  }
  if (EventPoll::EventType::ERROR & events) {
    newEvents |= IEV_ERROR;
  }
  if (EventPoll::EventType::HUP & events) {
    newEvents |= IEV_HUP;
  }
  return newEvents;
}

void LibuvEventPoll::pollCallback(KPoll* poll, int status, int events)
{
  if (status < 0) {
    switch (status) {
    case UV_EAGAIN:
    case UV_EINTR:
      return;
    case UV_EOF:
    case UV_ECONNABORTED:
    case UV_ECONNREFUSED:
    case UV_ECONNRESET:
    case UV_ENOTCONN:
    case UV_EPIPE:
    case UV_ESHUTDOWN:
      events = IEV_HUP;
      poll->processEvents(events);
      poll->stop();
      uv_stop(loop_);
      return;
    default:
      events = IEV_ERROR;
      poll->processEvents(events);
      poll->stop();
      uv_stop(loop_);
      return;
    }
  }

  // Got something
  poll->processEvents(events);
  uv_stop(loop_);
}

bool LibuvEventPoll::addEvents(sock_t socket,
                               const LibuvEventPoll::KEvent& event)
{
  auto i = socketEntries_.lower_bound(socket);

  if (i != socketEntries_.end() && i->first == socket) {
    auto& socketEntry = i->second;
    event.addSelf(&socketEntry);
    auto poll = polls_.find(socket);
    if (poll == polls_.end()) {
      throw std::logic_error("Invalid socket");
    }
    int r = poll->second->start();
    if (r < 0) {
      A2_LOG_DEBUG(fmt("uv_poll_start failed: {}", uv_err_name(r)));
      return false;
    }
    return true;
  }

  i = socketEntries_.insert(i, std::make_pair(socket, KSocketEntry(socket)));
  auto& socketEntry = i->second;
  event.addSelf(&socketEntry);
  auto poll = new KPoll(this, &socketEntry, socket);
  if (!poll->good()) {
    A2_LOG_DEBUG(fmt("uv_poll_init_socket failed for socket {}", socket));
    delete poll;
    socketEntries_.erase(i);
    return false;
  }
  int r = poll->start();
  if (r < 0) {
    A2_LOG_DEBUG(fmt("uv_poll_start failed: {}", uv_err_name(r)));
    poll->close();
    socketEntries_.erase(i);
    return false;
  }
  polls_[socket] = poll;
  return true;
}

bool LibuvEventPoll::addEvents(sock_t socket, Command* command,
                               EventPoll::EventType events)
{
  int pollEvents = translateEvents(events);
  return addEvents(socket, KCommandEvent(command, pollEvents));
}

#ifdef ENABLE_ASYNC_DNS
bool LibuvEventPoll::addEvents(sock_t socket, Command* command, int events,
                               const std::shared_ptr<AsyncNameResolver>& rs)
{
  return addEvents(socket, KADNSEvent(rs, command, socket, events));
}
#endif // ENABLE_ASYNC_DNS

bool LibuvEventPoll::deleteEvents(sock_t socket,
                                  const LibuvEventPoll::KEvent& event)
{
  auto i = socketEntries_.find(socket);

  if (i == socketEntries_.end()) {
    A2_LOG_DEBUG(fmt("Socket {} is not found in SocketEntries.", socket));
    return false;
  }

  auto& socketEntry = (*i).second;
  event.removeSelf(&socketEntry);

  auto poll = polls_.find(socket);
  if (poll == polls_.end()) {
    return false;
  }

  if (socketEntry.eventEmpty()) {
    poll->second->close();
    polls_.erase(poll);
    socketEntries_.erase(i);
    return true;
  }

  int r = poll->second->start();
  if (r < 0) {
    A2_LOG_DEBUG(fmt("uv_poll_start failed: {}", uv_err_name(r)));
    return false;
  }
  return true;
}

#ifdef ENABLE_ASYNC_DNS
bool LibuvEventPoll::deleteEvents(sock_t socket, Command* command,
                                  const std::shared_ptr<AsyncNameResolver>& rs)
{
  return deleteEvents(socket, KADNSEvent(rs, command, socket, 0));
}
#endif // ENABLE_ASYNC_DNS

bool LibuvEventPoll::deleteEvents(sock_t socket, Command* command,
                                  EventPoll::EventType events)
{
  int pollEvents = translateEvents(events);
  return deleteEvents(socket, KCommandEvent(command, pollEvents));
}

#ifdef ENABLE_ASYNC_DNS
bool LibuvEventPoll::addNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  auto itr = nameResolverEntries_.lower_bound(key);

  if (itr != std::end(nameResolverEntries_) && (*itr).first == key) {
    return false;
  }

  itr = nameResolverEntries_.insert(
      itr, std::make_pair(key, KAsyncNameResolverEntry(resolver, command)));
  (*itr).second.addSocketEvents(this);
  return true;
}

bool LibuvEventPoll::deleteNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  auto itr = nameResolverEntries_.find(key);
  if (itr == std::end(nameResolverEntries_)) {
    return false;
  }

  (*itr).second.removeSocketEvents(this);
  nameResolverEntries_.erase(itr);
  return true;
}
#endif // ENABLE_ASYNC_DNS

} // namespace aria2
