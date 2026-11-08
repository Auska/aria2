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
#ifndef D_XOR_CLOSER_H
#define D_XOR_CLOSER_H

#include "common.h"

#include <cassert>
#include <cstdlib>
#include <span>
#include <ranges>

namespace aria2 {

// XOR metric comparator: |key ^ key1| < |key ^ key2|. Like the original
// pointer version it returns true when both keys are equal.
class XORCloser {
private:
  std::span<const unsigned char> key_;

public:
  explicit XORCloser(std::span<const unsigned char> key) : key_(key) {}

  bool operator()(std::span<const unsigned char> key1,
                  std::span<const unsigned char> key2) const
  {
    assert(key_.size() == key1.size());
    assert(key_.size() == key2.size());
    for (auto [k, k1, k2] : std::views::zip(key_, key1, key2)) {
      unsigned char c1 = k ^ k1;
      unsigned char c2 = k ^ k2;

      if (c1 < c2) {
        return true;
      }
      else if (c1 > c2) {
        return false;
      }
    }
    return true;
  }
};

} // namespace aria2

#endif // D_XOR_CLOSER_H
