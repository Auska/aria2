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
#include "DHTUnknownMessage.h"
#include <string_view>

#include <cstring>
#include <cstdlib>

#include "DHTNode.h"
#include "util.h"
#include "a2functional.h"

namespace aria2 {

const std::string DHTUnknownMessage::E("e");

const std::string DHTUnknownMessage::UNKNOWN("unknown");

DHTUnknownMessage::DHTUnknownMessage(const std::shared_ptr<DHTNode>& localNode,
                                     std::span<const unsigned char> data,
                                     std::string_view ipaddr, uint16_t port)
    : DHTMessage(localNode, std::shared_ptr<DHTNode>()),
      data_(data.begin(), data.end()),
      ipaddr_(ipaddr),
      port_(port)
{
}

DHTUnknownMessage::~DHTUnknownMessage() = default;

void DHTUnknownMessage::doReceivedAction() {}

bool DHTUnknownMessage::send() { return true; }

bool DHTUnknownMessage::isReply() const { return false; }

const std::string& DHTUnknownMessage::getMessageType() const { return UNKNOWN; }

std::string DHTUnknownMessage::toString() const
{
  auto sample = std::span{data_}.first(std::min<size_t>(8, data_.size()));
  return fmt("dht unknown Remote:{}({}) length={}, first 8 bytes(hex)={}",
             ipaddr_, port_, static_cast<unsigned long>(data_.size()),
             util::toHex(std::as_bytes(sample)));
}

} // namespace aria2
