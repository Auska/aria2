/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2012 Tatsuhiro Tsujikawa
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
#include "FeatureConfig.h"

#include <cstring>

#ifdef HAVE_ZLIB
#  include <zlib.h>
#endif // HAVE_ZLIB
#ifdef HAVE_SQLITE3
#  include <sqlite3.h>
#endif // HAVE_SQLITE3
#ifdef HAVE_OPENSSL
#  include <openssl/opensslv.h>
#endif // HAVE_OPENSSL
#ifdef HAVE_LIBCARES
#  include <ares.h>
#endif // HAVE_LIBCARES
#ifdef HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif // HAVE_SYS_UTSNAME_H
#include "util.h"

namespace aria2 {

uint16_t getDefaultPort(std::string_view protocol)
{
  if (protocol == "http") {
    return 80;
  }
  else if (protocol == "https") {
    return 443;
  }
  else if (protocol == "ftp") {
    return 21;
  }
  else {
    return 0;
  }
}

std::string featureSummary()
{
  std::string s;
  auto max = static_cast<int>(FeatureType::MAX_FEATURE);
  int first;
  for (first = 0;
       first < max && !strSupportedFeature(static_cast<FeatureType>(first));
       ++first)
    ;
  if (first < max) {
    s += strSupportedFeature(static_cast<FeatureType>(first));
    for (int i = first + 1; i < max; ++i) {
      const char* name = strSupportedFeature(static_cast<FeatureType>(i));
      if (name) {
        s += ", ";
        s += name;
      }
    }
  }
  return s;
}

const char* strSupportedFeature(FeatureType feature)
{
  switch (feature) {
  case (FeatureType::ASYNC_DNS):
#ifdef ENABLE_ASYNC_DNS
    return "Async DNS";
#else  // !ENABLE_ASYNC_DNS
    return nullptr;
#endif // !ENABLE_ASYNC_DNS
    break;

  case (FeatureType::BITTORRENT):
#ifdef ENABLE_BITTORRENT
    return "BitTorrent";
#else  // !ENABLE_BITTORRENT
    return nullptr;
#endif // !ENABLE_BITTORRENT
    break;

  case (FeatureType::FF3_COOKIE):
#ifdef HAVE_SQLITE3
    return "Firefox3 Cookie";
#else  // !HAVE_SQLITE3
    return nullptr;
#endif // !HAVE_SQLITE3
    break;

  case (FeatureType::GZIP):
#ifdef HAVE_ZLIB
    return "GZip";
#else  // !HAVE_ZLIB
    return nullptr;
#endif // !HAVE_ZLIB
    break;

  case (FeatureType::HTTPS):
#ifdef ENABLE_SSL
    return "HTTPS";
#else  // !ENABLE_SSL
    return nullptr;
#endif // !ENABLE_SSL
    break;

  case (FeatureType::MESSAGE_DIGEST):
    return "Message Digest";
    break;

  default:
    return nullptr;
  }
}

std::string usedLibs()
{
  std::string res;
#ifdef HAVE_ZLIB
  res += "zlib/" ZLIB_VERSION " ";
#endif // HAVE_ZLIB
#ifdef HAVE_SQLITE3
  res += "sqlite3/" SQLITE_VERSION " ";
#endif // HAVE_SQLITE3
#ifdef HAVE_OPENSSL
  res += fmt("OpenSSL/{}.{}.{}", OPENSSL_VERSION_NUMBER >> 28,
             (OPENSSL_VERSION_NUMBER >> 20) & 0xff,
             (OPENSSL_VERSION_NUMBER >> 12) & 0xff);
  if ((OPENSSL_VERSION_NUMBER >> 4) & 0xff) {
    res += 'a' + ((OPENSSL_VERSION_NUMBER >> 4) & 0xff) - 1;
  }
  res += " ";
#endif // HAVE_OPENSSL
#ifdef HAVE_LIBCARES
  res += "c-ares/" ARES_VERSION_STR " ";
#endif // HAVE_LIBCARES

  if (!res.empty()) {
    res.erase(res.length() - 1);
  }
  return res;
}

std::string usedCompilerAndPlatform()
{
  std::string rv;
#if defined(__clang_version__)

#  ifdef __apple_build_version__
  rv += "Apple LLVM ";
#  else  // !__apple_build_version__
  rv += "clang ";
#  endif // !__apple_build_version__
  rv += __clang_version__;

#elif defined(__INTEL_COMPILER)

  rv += "Intel ICC ";
  rv += __VERSION__;

#elif defined(__GNUG__)

  rv += "gcc ";
  rv += __VERSION__;

#else // !defined(__GNUG__)

  rv += "Unknown compiler/platform";

#endif // !defined(__GNUG__)

  rv += "\n  built by  ";
  rv += BUILD;
  if (strcmp(BUILD, TARGET)) {
    rv += "\n  targeting ";
    rv += TARGET;
  }
  rv += "\n  on        ";
  rv += __DATE__;
  rv += " ";
  rv += __TIME__;

  return rv;
}

std::string getOperatingSystemInfo()
{
#ifdef HAVE_SYS_UTSNAME_H
  struct utsname name;
  if (!uname(&name)) {
    if (!strstr(name.version, name.sysname) ||
        !strstr(name.version, name.release) ||
        !strstr(name.version, name.machine)) {
      std::string ss;
      ss += name.sysname;
      ss += " ";
      ss += name.release;
      ss += " ";
      ss += name.version;
      ss += " ";
      ss += name.machine;
      return ss;
    }
    return name.version;
  }
#endif // HAVE_SYS_UTSNAME_H
  return "Unknown system";
}

} // namespace aria2
