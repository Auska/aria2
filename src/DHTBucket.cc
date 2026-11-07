/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
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
#include "DHTBucket.h"

#include <cstring>
#include <cassert>
#include <algorithm>

#include "DHTNode.h"
#include "LogFactory.h"
#include "Logger.h"
#include "util.h"
#include "DHTConstants.h"
#include "a2functional.h"
#include "bittorrent_helper.h"
#include "bitfield.h"
#include "wallclock.h"
#include "fmt.h"

namespace aria2 {

DHTBucket::DHTBucket(size_t prefixLength, const DHTID& max, const DHTID& min,
                     const std::shared_ptr<DHTNode>& localNode)
    : prefixLength_(prefixLength),
      min_(min),
      max_(max),
      localNode_(localNode),
      lastUpdated_(global::wallclock())
{
}

DHTBucket::DHTBucket(const std::shared_ptr<DHTNode>& localNode)
    : prefixLength_(0), localNode_(localNode), lastUpdated_(global::wallclock())
{
  max_.fill(0xffu);
  min_.fill(0);
}

DHTBucket::~DHTBucket() = default;

void DHTBucket::getRandomNodeID(DHTID& nodeID) const
{
  if (prefixLength_ == 0) {
    util::generateRandomKey(nodeID.data());
  }
  else {
    size_t lastByteIndex = (prefixLength_ - 1) / 8;
    util::generateRandomKey(nodeID.data());
    std::copy_n(std::begin(min_), lastByteIndex + 1, std::begin(nodeID));
  }
}

bool DHTBucket::isInRange(const std::shared_ptr<DHTNode>& node) const
{
  return isInRange(node->getID(), max_, min_);
}

bool DHTBucket::isInRange(const DHTID& nodeID) const
{
  return isInRange(nodeID, max_, min_);
}

// Returns true if nodeID is in [min, max] (inclusive).
bool DHTBucket::isInRange(const DHTID& nodeID, const DHTID& max,
                          const DHTID& min) const
{
  return !std::lexicographical_compare(std::begin(nodeID), std::end(nodeID),
                                       std::begin(min), std::end(min)) &&
         !std::lexicographical_compare(std::begin(max), std::end(max),
                                       std::begin(nodeID), std::end(nodeID));
}

bool DHTBucket::addNode(const std::shared_ptr<DHTNode>& node)
{
  notifyUpdate();
  auto itr = std::find_if(nodes_.begin(), nodes_.end(), derefEqual(node));
  if (itr == nodes_.end()) {
    if (nodes_.size() < K) {
      nodes_.push_back(node);
      return true;
    }
    else {
      if (nodes_.front()->isBad()) {
        nodes_.erase(nodes_.begin());
        nodes_.push_back(node);
        return true;
      }
      else {
        return false;
      }
    }
  }
  else {
    nodes_.erase(itr);
    nodes_.push_back(node);
    return true;
  }
}

void DHTBucket::cacheNode(const std::shared_ptr<DHTNode>& node)
{
  // cachedNodes_ are sorted by last time seen
  cachedNodes_.push_front(node);
  if (cachedNodes_.size() > CACHE_SIZE) {
    cachedNodes_.resize(CACHE_SIZE, std::shared_ptr<DHTNode>());
  }
}

void DHTBucket::dropNode(const std::shared_ptr<DHTNode>& node)
{
  if (!cachedNodes_.empty()) {
    auto itr = std::find_if(nodes_.begin(), nodes_.end(), derefEqual(node));
    if (itr != nodes_.end()) {
      nodes_.erase(itr);
      nodes_.push_back(cachedNodes_.front());
      cachedNodes_.erase(cachedNodes_.begin());
    }
  }
}

void DHTBucket::moveToHead(const std::shared_ptr<DHTNode>& node)
{
  auto itr = std::find_if(nodes_.begin(), nodes_.end(), derefEqual(node));
  if (itr != nodes_.end()) {
    nodes_.erase(itr);
    nodes_.push_front(node);
  }
}

void DHTBucket::moveToTail(const std::shared_ptr<DHTNode>& node)
{
  auto itr = std::find_if(nodes_.begin(), nodes_.end(), derefEqual(node));
  if (itr != nodes_.end()) {
    nodes_.erase(itr);
    nodes_.push_back(node);
  }
}

bool DHTBucket::splitAllowed() const
{
  return prefixLength_ < DHT_ID_LENGTH * 8 - 1 && isInRange(localNode_);
}

std::unique_ptr<DHTBucket> DHTBucket::split()
{
  assert(splitAllowed());

  DHTID rMax = max_;
  bitfield::flipBit(rMax.data(), rMax.size(), prefixLength_);
  DHTID rMin = min_;

  bitfield::flipBit(min_.data(), min_.size(), prefixLength_);

  ++prefixLength_;
  auto rBucket = make_unique<DHTBucket>(prefixLength_, rMax, rMin, localNode_);

  std::deque<std::shared_ptr<DHTNode>> lNodes;
  for (auto& elem : nodes_) {
    if (rBucket->isInRange(elem)) {
      assert(rBucket->addNode(elem));
    }
    else {
      lNodes.push_back(elem);
    }
  }
  nodes_ = lNodes;
  // TODO create toString() and use it.
  A2_LOG_DEBUG(fmt("New bucket. prefixLength={}, Range:{}-{}",
                   static_cast<unsigned int>(rBucket->getPrefixLength()),
                   util::toHex(std::as_bytes(std::span{rBucket->getMinID()}))
                       ,
                   util::toHex(std::as_bytes(std::span{rBucket->getMaxID()}))
                       ));
  A2_LOG_DEBUG(fmt("Existing bucket. prefixLength={}, Range:{}-{}",
                   static_cast<unsigned int>(prefixLength_),
                   util::toHex(std::as_bytes(std::span{getMinID()})),
                   util::toHex(std::as_bytes(std::span{getMaxID()}))));
  return rBucket;
}

void DHTBucket::getGoodNodes(
    std::vector<std::shared_ptr<DHTNode>>& goodNodes) const
{
  goodNodes.insert(goodNodes.end(), nodes_.begin(), nodes_.end());
  std::erase_if(goodNodes, std::mem_fn(&DHTNode::isBad));
}

std::shared_ptr<DHTNode> DHTBucket::getNode(const DHTID& nodeID,
                                            const std::string& ipaddr,
                                            uint16_t port) const
{
  auto node = std::make_shared<DHTNode>(nodeID);
  node->setIPAddress(ipaddr);
  node->setPort(port);
  auto itr = std::find_if(nodes_.begin(), nodes_.end(), derefEqual(node));
  if (itr == nodes_.end() || (*itr)->getIPAddress() != ipaddr ||
      (*itr)->getPort() != port) {
    return nullptr;
  }
  else {
    return *itr;
  }
}

bool DHTBucket::operator==(const DHTBucket& bucket) const
{
  return max_ == bucket.max_ && min_ == bucket.min_;
}

bool DHTBucket::needsRefresh() const
{
  return nodes_.size() < K || lastUpdated_.difference(global::wallclock()) >=
                                  DHT_BUCKET_REFRESH_INTERVAL;
}

void DHTBucket::notifyUpdate() { lastUpdated_ = global::wallclock(); }

namespace {
class FindQuestionableNode {
public:
  bool operator()(const std::shared_ptr<DHTNode>& node) const
  {
    return node->isQuestionable();
  }
};
} // namespace

bool DHTBucket::containsQuestionableNode() const
{
  return std::find_if(nodes_.begin(), nodes_.end(), FindQuestionableNode()) !=
         nodes_.end();
}

std::shared_ptr<DHTNode> DHTBucket::getLRUQuestionableNode() const
{
  auto i = std::find_if(nodes_.begin(), nodes_.end(), FindQuestionableNode());
  if (i == nodes_.end()) {
    return nullptr;
  }
  else {
    return *i;
  }
}

} // namespace aria2
