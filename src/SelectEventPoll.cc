/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2009 Tatsuhiro Tsujikawa
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
#include "SelectEventPoll.h"

#include <cstring>
#include <algorithm>
#include <ranges>
#include <numeric>

#include "Command.h"
#include "LogFactory.h"
#include "Logger.h"
#include "a2functional.h"
#include "fmt.h"
#include "util.h"

namespace aria2 {

SelectEventPoll::CommandEvent::CommandEvent(Command* command,
                                            EventPoll::EventType events)
    : command_(command), events_(events)
{
}

void SelectEventPoll::CommandEvent::processEvents(EventPoll::EventType events)
{
  if ((events_ & events) ||
      ((EventPoll::EventType::ERROR | EventPoll::EventType::HUP) & events)) {
    command_->setStatusActive();
  }
  if (EventPoll::EventType::READ & events) {
    command_->readEventReceived();
  }
  if (EventPoll::EventType::WRITE & events) {
    command_->writeEventReceived();
  }
  if (EventPoll::EventType::ERROR & events) {
    command_->errorEventReceived();
  }
  if (EventPoll::EventType::HUP & events) {
    command_->hupEventReceived();
  }
}

SelectEventPoll::SocketEntry::SocketEntry(sock_t socket) : socket_(socket) {}

void SelectEventPoll::SocketEntry::addCommandEvent(Command* command,
                                                  EventPoll::EventType events)
{
  CommandEvent cev(command, events);
  auto i = std::ranges::find(commandEvents_, cev);
  if (i == commandEvents_.end()) {
    commandEvents_.push_back(cev);
  }
  else {
    (*i).addEvents(events);
  }
}
void SelectEventPoll::SocketEntry::removeCommandEvent(Command* command,
                                                      EventPoll::EventType events)
{
  CommandEvent cev(command, events);
  auto i = std::ranges::find(commandEvents_, cev);
  if (i == commandEvents_.end()) {
    // not found
  }
  else {
    (*i).removeEvents(events);
    if ((*i).eventsEmpty()) {
      commandEvents_.erase(i);
    }
  }
}
void SelectEventPoll::SocketEntry::processEvents(EventPoll::EventType events)
{
  using namespace std::placeholders;
  std::for_each(commandEvents_.begin(), commandEvents_.end(),
                std::bind(&CommandEvent::processEvents, _1, events));
}

EventPoll::EventType accumulateEvent(EventPoll::EventType events,
                                     const SelectEventPoll::CommandEvent& event)
{
  return events | event.getEvents();
}

EventPoll::EventType SelectEventPoll::SocketEntry::getEvents()
{
  return std::accumulate(commandEvents_.begin(), commandEvents_.end(),
                         EventPoll::EventType::NONE, accumulateEvent);
}

#ifdef ENABLE_ASYNC_DNS

SelectEventPoll::AsyncNameResolverEntry::AsyncNameResolverEntry(
    const std::shared_ptr<AsyncNameResolver>& nameResolver, Command* command)
    : nameResolver_(nameResolver), command_(command)
{
}

ares_socket_t SelectEventPoll::AsyncNameResolverEntry::getFds(fd_set* rfdsPtr,
                                                              fd_set* wfdsPtr)
{
  return nameResolver_->getFds(rfdsPtr, wfdsPtr);
}

void SelectEventPoll::AsyncNameResolverEntry::process(fd_set* rfdsPtr,
                                                      fd_set* wfdsPtr)
{
  nameResolver_->process(rfdsPtr, wfdsPtr);
  switch (nameResolver_->getStatus()) {
  case AsyncNameResolver::Status::SUCCESS:
  case AsyncNameResolver::Status::ERROR:
    command_->setStatusActive();
    break;
  default:
    break;
  }
}

#endif // ENABLE_ASYNC_DNS

SelectEventPoll::SelectEventPoll() { updateFdSet(); }

SelectEventPoll::~SelectEventPoll() = default;

void SelectEventPoll::poll(const struct timeval& tv)
{
  fd_set rfds;
  fd_set wfds;

  memcpy(&rfds, &rfdset_, sizeof(fd_set));
  memcpy(&wfds, &wfdset_, sizeof(fd_set));

#ifdef ENABLE_ASYNC_DNS

  for (auto& i : nameResolverEntries_) {
    auto& entry = i.second;
    auto fd = entry.getFds(&rfds, &wfds);
    // TODO force error if fd == 0
    if (fdmax_ < fd) {
      fdmax_ = fd;
    }
  }

#endif // ENABLE_ASYNC_DNS
  int retval;
  do {
    struct timeval ttv = tv;
    retval = select(fdmax_ + 1, &rfds, &wfds, nullptr, &ttv);
  } while (retval == -1 && errno == EINTR);
  if (retval > 0) {
    for (auto& i : socketEntries_) {
      auto& e = i.second;
      auto events = EventPoll::EventType::NONE;
      if (FD_ISSET(e.getSocket(), &rfds)) {
        events |= EventPoll::EventType::READ;
      }
      if (FD_ISSET(e.getSocket(), &wfds)) {
        events |= EventPoll::EventType::WRITE;
      }
      e.processEvents(events);
    }
  }
  else if (retval == -1) {
    int errNum = errno;
    A2_LOG_INFO(fmt("select error: {}, fdmax: {}",
                    util::safeStrerror(errNum).c_str(), fdmax_));
  }
#ifdef ENABLE_ASYNC_DNS

  for (auto& i : nameResolverEntries_) {
    i.second.process(&rfds, &wfds);
  }

#endif // ENABLE_ASYNC_DNS
}

void SelectEventPoll::updateFdSet()
{
  FD_ZERO(&rfdset_);
  FD_ZERO(&wfdset_);
  fdmax_ = 0;

  for (auto& i : socketEntries_) {
    auto& e = i.second;
    sock_t fd = e.getSocket();
    if (fd < 0 || FD_SETSIZE <= fd) {
      A2_LOG_WARN("Detected file descriptor >= FD_SETSIZE or < 0. "
                  "Download may slow down or fail.");
      continue;
    }
    auto events = e.getEvents();
    if (events & EventPoll::EventType::READ) {
      FD_SET(fd, &rfdset_);
    }
    if (events & EventPoll::EventType::WRITE) {
      FD_SET(fd, &wfdset_);
    }
    if (fdmax_ < fd) {
      fdmax_ = fd;
    }
  }
}

bool SelectEventPoll::addEvents(sock_t socket, Command* command,
                                EventPoll::EventType events)
{
  auto i = socketEntries_.lower_bound(socket);
  if (i != std::end(socketEntries_) && (*i).first == socket) {
    (*i).second.addCommandEvent(command, events);
  }
  else {
    i = socketEntries_.insert(i, std::make_pair(socket, SocketEntry(socket)));
    (*i).second.addCommandEvent(command, events);
  }
  updateFdSet();
  return true;
}

bool SelectEventPoll::deleteEvents(sock_t socket, Command* command,
                                   EventPoll::EventType events)
{
  auto i = socketEntries_.find(socket);
  if (i == std::end(socketEntries_)) {
    A2_LOG_DEBUG(fmt("Socket {} is not found in SocketEntries.", socket));
    return false;
  }

  auto& socketEntry = (*i).second;
  socketEntry.removeCommandEvent(command, events);
  if (socketEntry.eventEmpty()) {
    socketEntries_.erase(i);
  }
  updateFdSet();
  return true;
}

#ifdef ENABLE_ASYNC_DNS
bool SelectEventPoll::addNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  auto itr = nameResolverEntries_.lower_bound(key);
  if (itr != std::end(nameResolverEntries_) && (*itr).first == key) {
    return false;
  }

  nameResolverEntries_.insert(
      itr, std::make_pair(key, AsyncNameResolverEntry(resolver, command)));

  return true;
}

bool SelectEventPoll::deleteNameResolver(
    const std::shared_ptr<AsyncNameResolver>& resolver, Command* command)
{
  auto key = std::make_pair(resolver.get(), command);
  return nameResolverEntries_.erase(key) == 1;
}
#endif // ENABLE_ASYNC_DNS

} // namespace aria2
