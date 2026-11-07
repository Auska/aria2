/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2013 Tatsuhiro Tsujikawa
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
#include "UDPTrackerClient.h"

#include "UDPTrackerRequest.h"
#include "bittorrent_helper.h"
#include "util.h"
#include "LogFactory.h"
#include "SimpleRandomizer.h"
#include "fmt.h"

namespace aria2 {

UDPTrackerClient::UDPTrackerClient() : numWatchers_(0) {}

namespace {
template <typename InputIterator>
void failRequest(InputIterator first, InputIterator last,
                 UDPTrackerError error)
{
  for (; first != last; ++first) {
    (*first)->state = UDPTrackerState::Complete;
    (*first)->error = error;
  }
}
} // namespace

namespace {
uint32_t generateTransactionId()
{
  uint32_t res;
  SimpleRandomizer::getInstance()->getRandomBytes(
      std::span{reinterpret_cast<std::byte*>(&res), sizeof(res)});
  return res;
}
} // namespace

namespace {
void logInvalidLength(const std::string& remoteAddr, uint16_t remotePort,
                      UDPTrackerAction action, unsigned long expected,
                      unsigned long actual)
{
  A2_LOG_INFO(fmt("UDPT received {} reply from {}:{} invalid length "
                  "expected:{}, actual:{}",
                  getUDPTrackerActionStr(action), remoteAddr.c_str(),
                  remotePort, expected, actual));
}
} // namespace

namespace {
void logInvalidTransaction(const std::string& remoteAddr, uint16_t remotePort,
                           UDPTrackerAction action, uint32_t transactionId)
{
  A2_LOG_INFO(
      fmt("UDPT received {} reply from {}:{} invalid transaction_id={:08x}",
          getUDPTrackerActionStr(action), remoteAddr.c_str(), remotePort,
          transactionId));
}
} // namespace

namespace {
void logTooShortLength(const std::string& remoteAddr, uint16_t remotePort,
                       UDPTrackerAction action, unsigned long minLength,
                       unsigned long actual)
{
  A2_LOG_INFO(fmt("UDPT received {} reply from {}:{} length too short "
                  "min:{}, actual:{}",
                  getUDPTrackerActionStr(action), remoteAddr.c_str(),
                  remotePort, minLength, actual));
}
} // namespace

UDPTrackerClient::~UDPTrackerClient()
{
  // Make all contained requests fail
  auto error = UDPTrackerError::Shutdown;
  failRequest(inflightRequests_.begin(), inflightRequests_.end(), error);
  failRequest(pendingRequests_.begin(), pendingRequests_.end(), error);
  failRequest(connectRequests_.begin(), connectRequests_.end(), error);
}

namespace {
struct CollectAddrPortMatch {
  bool operator()(const std::shared_ptr<UDPTrackerRequest>& req) const
  {
    if (req->remoteAddr == remoteAddr && req->remotePort == remotePort) {
      dest.push_back(req);
      return true;
    }
    else {
      return false;
    }
  }
  std::vector<std::shared_ptr<UDPTrackerRequest>>& dest;
  std::string remoteAddr;
  uint16_t remotePort;
  CollectAddrPortMatch(std::vector<std::shared_ptr<UDPTrackerRequest>>& dest,
                       std::string remoteAddr, uint16_t remotePort)
      : dest(dest), remoteAddr(std::move(remoteAddr)), remotePort(remotePort)
  {
  }
};
} // namespace

int UDPTrackerClient::receiveReply(std::shared_ptr<UDPTrackerRequest>& recvReq,
                                   std::span<const std::byte> data,
                                   const std::string& remoteAddr,
                                   uint16_t remotePort, const Timer& now)
{
  auto dataPtr = reinterpret_cast<const unsigned char*>(data.data());
  auto length = data.size();
  auto action =
      static_cast<UDPTrackerAction>(bittorrent::getIntParam(dataPtr, 0));
  switch (action) {
  case UDPTrackerAction::Connect: {
    if (length != 16) {
      logInvalidLength(remoteAddr, remotePort, action, 16, length);
      return -1;
    }
    auto transactionId = bittorrent::getIntParam(dataPtr, 4);
    std::shared_ptr<UDPTrackerRequest> req =
        findInflightRequest(remoteAddr, remotePort, transactionId, true);
    if (!req) {
      logInvalidTransaction(remoteAddr, remotePort, action, transactionId);
      return -1;
    }

    auto connectionId = bittorrent::getLLIntParam(dataPtr, 8);
    A2_LOG_INFO(
        fmt("UDPT received CONNECT reply from {}:{} transaction_id={:08x},"
            "connection_id={:016x}",
            remoteAddr.c_str(), remotePort, transactionId, connectionId));
    UDPTrackerConnection c(UDPTrackerConnectionState::Connected, connectionId,
                           now);
    connectionIdCache_[std::make_pair(remoteAddr, remotePort)] = c;
    // Now we have connection ID, push requests which are waiting for
    // it.
    std::vector<std::shared_ptr<UDPTrackerRequest>> reqs;
    std::erase_if(connectRequests_,
                  CollectAddrPortMatch(reqs, remoteAddr, remotePort));
    pendingRequests_.insert(pendingRequests_.begin(), reqs.begin(), reqs.end());

    recvReq = std::move(req);

    break;
  }
  case UDPTrackerAction::Announce: {
    if (length < 20) {
      logTooShortLength(remoteAddr, remotePort, action, 20, length);
      return -1;
    }
    auto transactionId = bittorrent::getIntParam(dataPtr, 4);
    std::shared_ptr<UDPTrackerRequest> req =
        findInflightRequest(remoteAddr, remotePort, transactionId, true);
    if (!req) {
      logInvalidTransaction(remoteAddr, remotePort, action, transactionId);
      return -1;
    }
    req->state = UDPTrackerState::Complete;

    req->reply = std::make_shared<UDPTrackerReply>();
    req->reply->action = action;
    req->reply->transactionId = transactionId;
    req->reply->interval = bittorrent::getIntParam(dataPtr, 8);
    req->reply->leechers = bittorrent::getIntParam(dataPtr, 12);
    req->reply->seeders = bittorrent::getIntParam(dataPtr, 16);

    int numPeers = 0;
    for (size_t i = 20; i < length; i += 6) {
      std::pair<std::string, uint16_t> hostport =
          bittorrent::unpackcompact(dataPtr + i, AF_INET);
      if (!hostport.first.empty()) {
        req->reply->peers.push_back(hostport);
        ++numPeers;
      }
    }

    A2_LOG_INFO(
        fmt("UDPT received ANNOUNCE reply from {}:{} transaction_id={:08x},"
            "connection_id=%016{}, event={}, infohash={}, "
            "interval={}, leechers={}, "
            "seeders={}, num_peers={}",
            remoteAddr.c_str(), remotePort, transactionId, req->connectionId,
            getUDPTrackerEventStr(req->event),
            util::toHex(req->infohash).c_str(), req->reply->interval,
            req->reply->leechers, req->reply->seeders, numPeers));

    recvReq = std::move(req);

    break;
  }
  case UDPTrackerAction::Error: {
    if (length < 8) {
      logTooShortLength(remoteAddr, remotePort, action, 8, length);
      return -1;
    }
    auto transactionId = bittorrent::getIntParam(dataPtr, 4);
    std::shared_ptr<UDPTrackerRequest> req =
        findInflightRequest(remoteAddr, remotePort, transactionId, true);
    if (!req) {
      logInvalidTransaction(remoteAddr, remotePort, action, transactionId);
      return -1;
    }
    std::string errorString(dataPtr + 8, dataPtr + length);
    errorString = util::encodeNonUtf8(errorString);

    req->state = UDPTrackerState::Complete;
    req->error = UDPTrackerError::Tracker;

    A2_LOG_INFO(fmt("UDPT received ERROR reply from {}:{} transaction_id={:08x},"
                    "connection_id=%016{}, action={}, error_string={}",
                    remoteAddr.c_str(), remotePort, transactionId,
                    req->connectionId, getUDPTrackerActionStr(action),
                    errorString.c_str()));
    if (req->action == UDPTrackerAction::Connect) {
      failConnect(req->remoteAddr, req->remotePort, UDPTrackerError::Tracker);
    }

    recvReq = std::move(req);

    break;
  }
  case UDPTrackerAction::Scrape:
    A2_LOG_INFO(fmt("unexpected scrape action reply from {}:{}",
                    remoteAddr.c_str(), remotePort));
    return -1;
  default:
    A2_LOG_INFO(
        fmt("unknown action reply from {}:{}", remoteAddr.c_str(), remotePort));
    return -1;
  }
  return 0;
}

ssize_t UDPTrackerClient::createRequest(std::span<std::byte> data,
                                        std::string& remoteAddr,
                                        uint16_t& remotePort, const Timer& now)
{
  if (pendingRequests_.empty()) {
    return -1;
  }
  while (!pendingRequests_.empty()) {
    const std::shared_ptr<UDPTrackerRequest>& req = pendingRequests_.front();
    if (req->action == UDPTrackerAction::Connect) {
      ssize_t rv;
      rv = createUDPTrackerConnect(data, remoteAddr, remotePort, req);
      return rv;
    }
    UDPTrackerConnection* c =
        getConnectionId(req->remoteAddr, req->remotePort, now);
    if (!c) {
      auto creq = std::make_shared<UDPTrackerRequest>();
      creq->action = UDPTrackerAction::Connect;
      creq->remoteAddr = req->remoteAddr;
      creq->remotePort = req->remotePort;
      creq->transactionId = generateTransactionId();
      pendingRequests_.push_front(creq);
      ssize_t rv;
      rv = createUDPTrackerConnect(data, remoteAddr, remotePort, creq);
      return rv;
    }
    if (c->state == UDPTrackerConnectionState::Connecting) {
      connectRequests_.push_back(req);
      pendingRequests_.pop_front();
      continue;
    }
    req->connectionId = c->connectionId;
    req->transactionId = generateTransactionId();
    ssize_t rv;
    rv = createUDPTrackerAnnounce(data, remoteAddr, remotePort, req);
    return rv;
  }
  return -1;
}

void UDPTrackerClient::requestSent(const Timer& now)
{
  if (pendingRequests_.empty()) {
    A2_LOG_WARN("pendingRequests_ is empty");
    return;
  }
  const std::shared_ptr<UDPTrackerRequest>& req = pendingRequests_.front();
  switch (req->action) {
  case UDPTrackerAction::Connect:
    A2_LOG_INFO(fmt("UDPT sent CONNECT to {}:{} transaction_id={:08x}",
                    req->remoteAddr.c_str(), req->remotePort,
                    req->transactionId));
    break;
  case UDPTrackerAction::Announce:
    A2_LOG_INFO(fmt("UDPT sent ANNOUNCE to {}:{} transaction_id={:08x}, "
                    "connection_id=%016{}, event={}, infohash={}",
                    req->remoteAddr.c_str(), req->remotePort,
                    req->transactionId, req->connectionId,
                    getUDPTrackerEventStr(req->event),
                    util::toHex(req->infohash).c_str()));
    break;
  default:
    // unreachable
    assert(0);
  }
  req->dispatched = now;
  if (req->action == UDPTrackerAction::Connect) {
    connectionIdCache_[std::make_pair(req->remoteAddr, req->remotePort)] =
        UDPTrackerConnection();
  }
  inflightRequests_.push_back(req);
  pendingRequests_.pop_front();
}

void UDPTrackerClient::requestFail(UDPTrackerError error)
{
  if (pendingRequests_.empty()) {
    A2_LOG_WARN("pendingRequests_ is empty");
    return;
  }
  const std::shared_ptr<UDPTrackerRequest>& req = pendingRequests_.front();
  switch (req->action) {
  case UDPTrackerAction::Connect:
    A2_LOG_INFO(fmt("UDPT fail CONNECT to {}:{} transaction_id={:08x}",
                    req->remoteAddr.c_str(), req->remotePort,
                    req->transactionId));
    failConnect(req->remoteAddr, req->remotePort, error);
    break;
  case UDPTrackerAction::Announce:
    A2_LOG_INFO(fmt("UDPT fail ANNOUNCE to {}:{} transaction_id={:08x}, "
                    "connection_id=%016{}, event={}, infohash={}",
                    req->remoteAddr.c_str(), req->remotePort,
                    req->transactionId, req->connectionId,
                    getUDPTrackerEventStr(req->event),
                    util::toHex(req->infohash).c_str()));
    break;
  default:
    // unreachable
    assert(0);
  }
  req->state = UDPTrackerState::Complete;
  req->error = error;
  pendingRequests_.pop_front();
}

void UDPTrackerClient::addRequest(const std::shared_ptr<UDPTrackerRequest>& req)
{
  req->state = UDPTrackerState::Pending;
  req->error = UDPTrackerError::Success;
  pendingRequests_.push_back(req);
}

namespace {
struct TimeoutCheck {
  bool operator()(const std::shared_ptr<UDPTrackerRequest>& req) const
  {
    auto t = req->dispatched.difference(now);
    if (req->failCount == 0) {
      if (t >= 5_s) {
        switch (req->action) {
        case UDPTrackerAction::Connect:
          A2_LOG_INFO(fmt("UDPT resend CONNECT to {}:{} transaction_id={:08x}",
                          req->remoteAddr.c_str(), req->remotePort,
                          req->transactionId));
          break;
        case UDPTrackerAction::Announce:
          A2_LOG_INFO(fmt("UDPT resend ANNOUNCE to {}:{} transaction_id={:08x}, "
                          "connection_id=%016{}, event={}, infohash={}",
                          req->remoteAddr.c_str(), req->remotePort,
                          req->transactionId, req->connectionId,
                          getUDPTrackerEventStr(req->event),
                          util::toHex(req->infohash).c_str()));
          break;
        default:
          // unreachable
          assert(0);
        }
        ++req->failCount;
        dest.push_back(req);
        return true;
      }
      else {
        return false;
      }
    }
    else {
      if (t >= 10_s) {
        switch (req->action) {
        case UDPTrackerAction::Connect:
          A2_LOG_INFO(fmt("UDPT timeout CONNECT to {}:{} transaction_id={:08x}",
                          req->remoteAddr.c_str(), req->remotePort,
                          req->transactionId));
          client->failConnect(req->remoteAddr, req->remotePort,
                              UDPTrackerError::Timeout);
          break;
        case UDPTrackerAction::Announce:
          A2_LOG_INFO(fmt("UDPT timeout ANNOUNCE to {}:{} transaction_id={:08x}, "
                          "connection_id=%016{}, event={}, infohash={}",
                          req->remoteAddr.c_str(), req->remotePort,
                          req->transactionId, req->connectionId,
                          getUDPTrackerEventStr(req->event),
                          util::toHex(req->infohash).c_str()));
          break;
        default:
          // unreachable
          assert(0);
        }
        ++req->failCount;
        req->state = UDPTrackerState::Complete;
        req->error = UDPTrackerError::Timeout;
        return true;
      }
      else {
        return false;
      }
    }
  }
  std::vector<std::shared_ptr<UDPTrackerRequest>>& dest;
  UDPTrackerClient* client;
  const Timer& now;
  TimeoutCheck(std::vector<std::shared_ptr<UDPTrackerRequest>>& dest,
               UDPTrackerClient* client, const Timer& now)
      : dest(dest), client(client), now(now)
  {
  }
};
} // namespace

void UDPTrackerClient::handleTimeout(const Timer& now)
{
  std::vector<std::shared_ptr<UDPTrackerRequest>> dest;
  std::erase_if(inflightRequests_, TimeoutCheck(dest, this, now));
  pendingRequests_.insert(pendingRequests_.begin(), dest.begin(), dest.end());
}

std::shared_ptr<UDPTrackerRequest>
UDPTrackerClient::findInflightRequest(const std::string& remoteAddr,
                                      uint16_t remotePort,
                                      uint32_t transactionId, bool remove)
{
  std::shared_ptr<UDPTrackerRequest> res;
  for (auto i = inflightRequests_.begin(), eoi = inflightRequests_.end();
       i != eoi; ++i) {
    if ((*i)->remoteAddr == remoteAddr && (*i)->remotePort == remotePort &&
        (*i)->transactionId == transactionId) {
      res = *i;
      if (remove) {
        inflightRequests_.erase(i);
      }
      break;
    }
  }
  return res;
}

UDPTrackerConnection*
UDPTrackerClient::getConnectionId(const std::string& remoteAddr,
                                  uint16_t remotePort, const Timer& now)
{
  auto i = connectionIdCache_.find(std::make_pair(remoteAddr, remotePort));
  if (i == connectionIdCache_.end()) {
    return nullptr;
  }
  if ((*i).second.state == UDPTrackerConnectionState::Connected &&
      (*i).second.lastUpdated.difference(now) > 1_min) {
    connectionIdCache_.erase(i);
    return nullptr;
  }
  else {
    return &(*i).second;
  }
}

namespace {
struct FailConnectDelete {
  bool operator()(const std::shared_ptr<UDPTrackerRequest>& req) const
  {
    if (req->action == UDPTrackerAction::Announce &&
        req->remoteAddr == remoteAddr && req->remotePort == remotePort) {
      A2_LOG_INFO(
          fmt("Force fail infohash={}", util::toHex(req->infohash).c_str()));
      req->state = UDPTrackerState::Complete;
      req->error = error;
      return true;
    }
    else {
      return false;
    }
  }
  std::string remoteAddr;
  uint16_t remotePort;
  UDPTrackerError error;
  FailConnectDelete(std::string remoteAddr, uint16_t remotePort,
                    UDPTrackerError error)
      : remoteAddr(std::move(remoteAddr)), remotePort(remotePort), error(error)
  {
  }
};
} // namespace

void UDPTrackerClient::failConnect(const std::string& remoteAddr,
                                   uint16_t remotePort, UDPTrackerError error)
{
  connectionIdCache_.erase(std::make_pair(remoteAddr, remotePort));
  // Fail all requests which are waiting for connection ID of the host.
  std::erase_if(connectRequests_,
                FailConnectDelete(remoteAddr, remotePort, error));
  std::erase_if(pendingRequests_,
                FailConnectDelete(remoteAddr, remotePort, error));
}

void UDPTrackerClient::failAll()
{
  auto error = UDPTrackerError::Shutdown;
  failRequest(inflightRequests_.begin(), inflightRequests_.end(), error);
  failRequest(pendingRequests_.begin(), pendingRequests_.end(), error);
  failRequest(connectRequests_.begin(), connectRequests_.end(), error);
}

void UDPTrackerClient::increaseWatchers() { ++numWatchers_; }

void UDPTrackerClient::decreaseWatchers() { --numWatchers_; }

ssize_t createUDPTrackerConnect(std::span<std::byte> data,
                                std::string& remoteAddr, uint16_t& remotePort,
                                const std::shared_ptr<UDPTrackerRequest>& req)
{
  assert(data.size() >= 16);
  auto dataPtr = reinterpret_cast<unsigned char*>(data.data());
  remoteAddr = req->remoteAddr;
  remotePort = req->remotePort;
  bittorrent::setLLIntParam(dataPtr, UDPT_INITIAL_CONNECTION_ID);
  bittorrent::setIntParam(dataPtr + 8, to_underlying(req->action));
  bittorrent::setIntParam(dataPtr + 12, req->transactionId);
  return 16;
}

ssize_t createUDPTrackerAnnounce(std::span<std::byte> data,
                                 std::string& remoteAddr, uint16_t& remotePort,
                                 const std::shared_ptr<UDPTrackerRequest>& req)
{
  assert(data.size() >= 100);
  auto dataPtr = reinterpret_cast<unsigned char*>(data.data());
  remoteAddr = req->remoteAddr;
  remotePort = req->remotePort;
  bittorrent::setLLIntParam(dataPtr, req->connectionId);
  bittorrent::setIntParam(dataPtr + 8, to_underlying(req->action));
  bittorrent::setIntParam(dataPtr + 12, req->transactionId);
  memcpy(dataPtr + 16, req->infohash.c_str(), req->infohash.size());
  memcpy(dataPtr + 36, req->peerId.c_str(), req->peerId.size());
  bittorrent::setLLIntParam(dataPtr + 56, req->downloaded);
  bittorrent::setLLIntParam(dataPtr + 64, req->left);
  bittorrent::setLLIntParam(dataPtr + 72, req->uploaded);
  bittorrent::setIntParam(dataPtr + 80, to_underlying(req->event));
  // ip is already network-byte order
  memcpy(dataPtr + 84, &req->ip, sizeof(req->ip));
  bittorrent::setIntParam(dataPtr + 88, req->key);
  bittorrent::setIntParam(dataPtr + 92, req->numWant);
  bittorrent::setShortIntParam(dataPtr + 96, req->port);
  // extensions is always 0
  bittorrent::setShortIntParam(dataPtr + 98, 0);
  return 100;
}

const char* getUDPTrackerActionStr(UDPTrackerAction action)
{
  switch (action) {
  case UDPTrackerAction::Connect:
    return "CONNECT";
  case UDPTrackerAction::Announce:
    return "ANNOUNCE";
  case UDPTrackerAction::Error:
    return "ERROR";
  default:
    return "(unknown)";
  }
}

const char* getUDPTrackerEventStr(UDPTrackerEvent event)
{
  switch (event) {
  case UDPTrackerEvent::None:
    return "NONE";
  case UDPTrackerEvent::Completed:
    return "COMPLETED";
  case UDPTrackerEvent::Started:
    return "STARTED";
  case UDPTrackerEvent::Stopped:
    return "STOPPED";
  default:
    return "(unknown)";
  }
}

} // namespace aria2
