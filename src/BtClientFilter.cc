/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2025 Tatsuhiro Tsujikawa
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
#include "BtClientFilter.h"

#include <cstring>
#include <vector>

#include "prefs.h"
#include "util.h"
#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "bittorrent_helper.h"

namespace aria2 {

std::vector<std::string> BtClientFilter::getExcludedClientIds(
    const std::shared_ptr<DownloadContext>& downloadContext)
{
  const std::string& excludeClientIds = 
      downloadContext->getOwnerRequestGroup()->getOption()->get(PREF_BT_EXCLUDE_CLIENT_IDS);
  
  std::vector<std::string> excludedIds;
  if (!excludeClientIds.empty()) {
    util::split(excludeClientIds.begin(), excludeClientIds.end(), 
                std::back_inserter(excludedIds), ',', true);
  }
  
  return excludedIds;
}

std::vector<std::string> BtClientFilter::getIncludedClientIds(
    const std::shared_ptr<DownloadContext>& downloadContext)
{
  const std::string& includeClientIds = 
      downloadContext->getOwnerRequestGroup()->getOption()->get(PREF_BT_INCLUDE_CLIENT_IDS);
  
  std::vector<std::string> includedIds;
  if (!includeClientIds.empty()) {
    util::split(includeClientIds.begin(), includeClientIds.end(), 
                std::back_inserter(includedIds), ',', true);
  }
  
  return includedIds;
}

bool BtClientFilter::isPeerExcluded(const unsigned char* peerId,
                                    const std::shared_ptr<DownloadContext>& downloadContext)
{
  std::string_view peerStr(reinterpret_cast<const char*>(peerId), 8);
  std::vector<std::string> excludedIds = getExcludedClientIds(downloadContext);

  for (const std::string& excludedId : excludedIds) {
    if (peerStr.length() >= excludedId.length() &&
        peerStr.substr(0, excludedId.length()) == excludedId) {
      return true;  // Peer is excluded
    }
  }

  return false;  // Peer is not excluded
}

bool BtClientFilter::isPeerIncluded(const unsigned char* peerId,
                                    const std::shared_ptr<DownloadContext>& downloadContext)
{
  std::string_view peerStr(reinterpret_cast<const char*>(peerId), 8);
  std::vector<std::string> includedIds = getIncludedClientIds(downloadContext);

  // If no include list is specified, all peers are considered included
  if (includedIds.empty()) {
    return true;
  }

  for (const std::string& includedId : includedIds) {
    if (peerStr.length() >= includedId.length() &&
        peerStr.substr(0, includedId.length()) == includedId) {
      return true;  // Peer is included
    }
  }

  return false;  // Peer is not included
}

std::string BtClientFilter::getClientIdsMode(const std::shared_ptr<DownloadContext>& downloadContext)
{
  return downloadContext->getOwnerRequestGroup()->getOption()->get(PREF_BT_CLIENT_IDS_MODE);
}

} // namespace aria2