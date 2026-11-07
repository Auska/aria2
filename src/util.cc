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
#include "util.h"

#include <span>

#ifdef HAVE_SIGNAL_H
#  include <signal.h>
#endif // HAVE_SIGNAL_H

#include <sys/types.h>
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif // HAVE_PWD_H

#include <array>
#include <cerrno>
#include <cassert>
#include <charconv>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <algorithm>
#include <ranges>
#include <fstream>
#include <iomanip>

#include "SimpleRandomizer.h"
#include "File.h"
#include "Randomizer.h"
#include "a2netcompat.h"
#include "BitfieldMan.h"
#include "DefaultDiskWriter.h"
#include "FatalException.h"
#include "FileEntry.h"
#include "A2STR.h"
#include "array_fun.h"
#include "bitfield.h"
#include "DownloadHandlerConstants.h"
#include "RequestGroup.h"
#include "LogFactory.h"
#include "Logger.h"
#include "Option.h"
#include "DownloadContext.h"
#include "BufferedFile.h"
#include "SocketCore.h"
#include "Lock.h"

#include "MessageDigest.h"
#include "message_digest_helper.h"

// For libc6 which doesn't define ULLONG_MAX properly because of broken limits.h
#ifndef ULLONG_MAX
#  define ULLONG_MAX 18446744073709551615ULL
#endif // ULLONG_MAX

namespace aria2 {

namespace util {

const char DEFAULT_STRIP_CHARSET[] = "\r\n\t ";

std::string strip(const std::string& str, const char* chars)
{
  std::pair<std::string::const_iterator, std::string::const_iterator> p =
      stripIter(str.begin(), str.end(), chars);
  return std::string(p.first, p.second);
}

std::string itos(int64_t value, bool comma)
{
  bool flag = false;
  std::string str;
  if (value < 0) {
    if (value == INT64_MIN) {
      if (comma) {
        str = "-9,223,372,036,854,775,808";
      }
      else {
        str = "-9223372036854775808";
      }
      return str;
    }
    flag = true;
    value = -value;
  }
  str = uitos(value, comma);
  if (flag) {
    str.insert(str.begin(), '-');
  }
  return str;
}

int64_t difftv(struct timeval tv1, struct timeval tv2)
{
  if ((tv1.tv_sec < tv2.tv_sec) ||
      ((tv1.tv_sec == tv2.tv_sec) && (tv1.tv_usec < tv2.tv_usec))) {
    return 0;
  }
  return ((int64_t)(tv1.tv_sec - tv2.tv_sec) * 1000000 + tv1.tv_usec -
          tv2.tv_usec);
}

int32_t difftvsec(struct timeval tv1, struct timeval tv2)
{
  if (tv1.tv_sec < tv2.tv_sec) {
    return 0;
  }
  return tv1.tv_sec - tv2.tv_sec;
}

std::string replace(const std::string& target, const std::string& oldstr,
                    const std::string& newstr)
{
  if (target.empty() || oldstr.empty()) {
    return target;
  }
  std::string result;
  std::string::size_type p = 0;
  std::string::size_type np = target.find(oldstr);
  while (np != std::string::npos) {
    result.append(target.begin() + p, target.begin() + np);
    result += newstr;
    p = np + oldstr.size();
    np = target.find(oldstr, p);
  }
  result.append(target.begin() + p, target.end());
  return result;
}

bool isAlpha(const char c)
{
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

bool isDigit(const char c) { return '0' <= c && c <= '9'; }

bool isHexDigit(const char c)
{
  return isDigit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

bool isHexDigit(const std::string& s)
{
  for (const auto& c : s) {
    if (!isHexDigit(c)) {
      return false;
    }
  }
  return true;
}

bool inRFC3986ReservedChars(const char c)
{
  static const char reserved[] = {':', '/',  '?', '#', '[', ']', '@', '!', '$',
                                  '&', '\'', '(', ')', '*', '+', ',', ';', '='};
  return std::ranges::find(reserved, c) != std::end(reserved);
}

bool inRFC3986UnreservedChars(const char c)
{
  static const char unreserved[] = {'-', '.', '_', '~'};
  return isAlpha(c) || isDigit(c) ||
         std::ranges::find(unreserved, c) != std::end(unreserved);
}

bool inRFC2978MIMECharset(const char c)
{
  static const char chars[] = {'!', '#', '$', '%', '&', '\'', '+',
                               '-', '^', '_', '`', '{', '}',  '~'};
  return isAlpha(c) || isDigit(c) || std::ranges::find(chars, c) != std::end(chars);
}

bool inRFC2616HttpToken(const char c)
{
  static const char chars[] = {'!', '#', '$', '%', '&', '\'', '*', '+',
                               '-', '.', '^', '_', '`', '|',  '~'};
  return isAlpha(c) || isDigit(c) || std::ranges::find(chars, c) != std::end(chars);
}

bool inRFC5987AttrChar(const char c)
{
  return inRFC2616HttpToken(c) && c != '*' && c != '\'' && c != '%';
}

// Returns nonzero if |c| is in ISO/IEC 8859-1 character set.
bool isIso8859p1(unsigned char c)
{
  return (0x20u <= c && c <= 0x7eu) || 0xa0u <= c;
}

bool isLws(const char c) { return c == ' ' || c == '\t'; }
bool isCRLF(const char c) { return c == '\r' || c == '\n'; }

namespace {

bool isUtf8Tail(unsigned char ch) { return in(ch, 0x80u, 0xbfu); }

bool inPercentEncodeMini(const unsigned char c)
{
  return c > 0x20 && c < 0x7fu &&
         // Chromium escapes following characters. Firefox4 escapes more.
         c != '"' && c != '<' && c != '>';
}

} // namespace

bool isUtf8(const std::string& str)
{
  for (std::string::const_iterator s = str.begin(), eos = str.end(); s != eos;
       ++s) {
    unsigned char firstChar = *s;
    // See ABNF in http://tools.ietf.org/search/rfc3629#section-4
    if (in(firstChar, 0x20u, 0x7eu) || firstChar == 0x08u || // \b
        firstChar == 0x09u ||                                // \t
        firstChar == 0x0au ||                                // \n
        firstChar == 0x0cu ||                                // \f
        firstChar == 0x0du                                   // \r
    ) {
      // UTF8-1 (without ctrl chars)
    }
    else if (in(firstChar, 0xc2u, 0xdfu)) {
      // UTF8-2
      if (++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (0xe0u == firstChar) {
      // UTF8-3
      if (++s == eos || !in(static_cast<unsigned char>(*s), 0xa0u, 0xbfu) ||
          ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (in(firstChar, 0xe1u, 0xecu) || in(firstChar, 0xeeu, 0xefu)) {
      // UTF8-3
      if (++s == eos || !isUtf8Tail(*s) || ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (0xedu == firstChar) {
      // UTF8-3
      if (++s == eos || !in(static_cast<unsigned char>(*s), 0x80u, 0x9fu) ||
          ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (0xf0u == firstChar) {
      // UTF8-4
      if (++s == eos || !in(static_cast<unsigned char>(*s), 0x90u, 0xbfu) ||
          ++s == eos || !isUtf8Tail(*s) || ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (in(firstChar, 0xf1u, 0xf3u)) {
      // UTF8-4
      if (++s == eos || !isUtf8Tail(*s) || ++s == eos || !isUtf8Tail(*s) ||
          ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else if (0xf4u == firstChar) {
      // UTF8-4
      if (++s == eos || !in(static_cast<unsigned char>(*s), 0x80u, 0x8fu) ||
          ++s == eos || !isUtf8Tail(*s) || ++s == eos || !isUtf8Tail(*s)) {
        return false;
      }
    }
    else {
      return false;
    }
  }
  return true;
}

std::string percentEncode(std::span<const std::byte> target)
{
  std::string dest;
  for (auto b : target) {
    auto c = static_cast<unsigned char>(b);
    if (inRFC3986UnreservedChars(c)) {
      dest += static_cast<char>(c);
    }
    else {
      dest.append(fmt("%{:02X}", c));
    }
  }
  return dest;
}

std::string percentEncode(const std::string& target)
{
  if (std::find_if_not(target.begin(), target.end(),
                       inRFC3986UnreservedChars) == target.end()) {
    return target;
  }
  return percentEncode(
      std::as_bytes(std::span{target.data(), target.size()}));
}

std::string percentEncodeMini(const std::string& src)
{
  if (std::find_if_not(src.begin(), src.end(), inPercentEncodeMini) ==
      src.end()) {
    return src;
  }
  std::string result;
  for (auto c : src) {
    if (!inPercentEncodeMini(c)) {
      result += fmt("%{:02X}", static_cast<unsigned char>(c));
    }
    else {
      result += c;
    }
  }
  return result;
}

std::string torrentPercentEncode(std::span<const std::byte> target)
{
  std::string dest;
  for (auto b : target) {
    auto c = static_cast<unsigned char>(b);
    if (isAlpha(c) || isDigit(c)) {
      dest += static_cast<char>(c);
    }
    else {
      dest.append(fmt("%{:02X}", c));
    }
  }
  return dest;
}

std::string torrentPercentEncode(const std::string& target)
{
  return torrentPercentEncode(
      std::as_bytes(std::span{target.data(), target.size()}));
}

std::string percentDecode(std::string::const_iterator first,
                          std::string::const_iterator last)
{
  std::string result;
  for (; first != last; ++first) {
    if (*first == '%') {
      if (first + 1 != last && first + 2 != last && isHexDigit(*(first + 1)) &&
          isHexDigit(*(first + 2))) {
        result +=
            hexCharToUInt(*(first + 1)) * 16 + hexCharToUInt(*(first + 2));
        first += 2;
      }
      else {
        result += *first;
      }
    }
    else {
      result += *first;
    }
  }
  return result;
}

std::string toHex(std::span<const std::byte> src)
{
  std::string out(src.size() * 2, '\0');
  std::string::iterator o = out.begin();
  for (auto b : src) {
    auto c = static_cast<unsigned char>(b);
    *o = (c >> 4);
    *(o + 1) = c & 0x0fu;
    for (int j = 0; j < 2; ++j) {
      if (*o < 10) {
        *o += '0';
      }
      else {
        *o += 'a' - 10;
      }
      ++o;
    }
  }
  return out;
}

std::string toHex(const std::string& src)
{
  return toHex(std::as_bytes(std::span{src.data(), src.size()}));
}

unsigned int hexCharToUInt(unsigned char ch)
{
  if ('a' <= ch && ch <= 'f') {
    ch -= 'a';
    ch += 10;
  }
  else if ('A' <= ch && ch <= 'F') {
    ch -= 'A';
    ch += 10;
  }
  else if ('0' <= ch && ch <= '9') {
    ch -= '0';
  }
  else {
    ch = 255;
  }
  return ch;
}

std::string secfmt(time_t sec)
{
  time_t tsec = sec;
  std::string str;
  if (sec >= 3600) {
    str = fmt("{}h", static_cast<int64_t>(sec / 3600));
    sec %= 3600;
  }
  if (sec >= 60) {
    str += fmt("{}m", static_cast<int>(sec / 60));
    sec %= 60;
  }
  if (sec || tsec == 0) {
    str += fmt("{}s", static_cast<int>(sec));
  }
  return str;
}

namespace {
// Strips leading and trailing spaces, matching what the printf family does
// with its leading whitespace and what the old strto* callers tolerated.
std::string_view trimSpaces(std::string_view s)
{
  while (!s.empty() && isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  while (!s.empty() && isspace(static_cast<unsigned char>(s.back()))) {
    s.remove_suffix(1);
  }
  return s;
}

// std::from_chars only takes base 10 for the strictly-standard overloads, so
// the |base| argument is handled here with a manual digit loop for the rare
// non-decimal callers.
template <typename T> bool parseIntegral(T& res, std::string_view sv, int base)
{
  auto t = trimSpaces(sv);
  if (t.empty()) {
    return false;
  }
  if (base == 10) {
    auto r = std::from_chars(t.data(), t.data() + t.size(), res);
    return r.ec == std::errc() && r.ptr == t.data() + t.size();
  }
  // Non-decimal bases are not used by any caller today; keep the previous
  // semantics with a slow but obviously correct path.
  std::string buf(t);
  char* endptr;
  errno = 0;
  if constexpr (std::is_signed_v<T>) {
    auto v = std::strtoll(buf.c_str(), &endptr, base);
    if (errno == ERANGE) {
      return false;
    }
    res = static_cast<T>(v);
  }
  else {
    auto v = std::strtoull(buf.c_str(), &endptr, base);
    if (errno == ERANGE) {
      return false;
    }
    res = static_cast<T>(v);
  }
  return endptr == buf.c_str() + buf.size();
}
} // namespace

bool parseIntNoThrow(int32_t& res, const std::string& s, int base)
{
  int32_t t;
  if (parseIntegral(t, s, base)) {
    res = t;
    return true;
  }
  return false;
}

bool parseUIntNoThrow(uint32_t& res, const std::string& s, int base)
{
  uint32_t t;
  if (!parseIntegral(t, s, base) || t > static_cast<uint32_t>(INT32_MAX)) {
    return false;
  }
  res = t;
  return true;
}

bool parseLLIntNoThrow(int64_t& res, const std::string& s, int base)
{
  int64_t t;
  if (parseIntegral(t, s, base)) {
    res = t;
    return true;
  }
  return false;
}

bool parseDoubleNoThrow(double& res, const std::string& s)
{
  auto t = trimSpaces(s);
  if (t.empty()) {
    return false;
  }
  double d = 0;
  auto r = std::from_chars(t.data(), t.data() + t.size(), d);
  if (r.ec == std::errc::result_out_of_range) {
    return false;
  }
  // Anything but the whole (trimmed) input is a malformed number.
  if (r.ec != std::errc() || r.ptr != t.data() + t.size()) {
    return false;
  }
  res = d;
  return true;
}

SegList<int> parseIntSegments(const std::string& src)
{
  SegList<int> sgl;
  for (std::string::const_iterator i = src.begin(), eoi = src.end();
       i != eoi;) {
    std::string::const_iterator j = std::find(i, eoi, ',');
    if (j == i) {
      ++i;
      continue;
    }
    std::string::const_iterator p = std::find(i, j, '-');
    if (p == j) {
      int a;
      if (parseIntNoThrow(a, std::string(i, j))) {
        sgl.add(a, a + 1);
      }
      else {
        throw DL_ABORT_EX(fmt("Bad range {}", std::string(i, j).c_str()));
      }
    }
    else if (p == i || p + 1 == j) {
      throw DL_ABORT_EX(fmt(MSG_INCOMPLETE_RANGE, std::string(i, j).c_str()));
    }
    else {
      int a, b;
      if (parseIntNoThrow(a, std::string(i, p)) &&
          parseIntNoThrow(b, (std::string(p + 1, j)))) {
        sgl.add(a, b + 1);
      }
      else {
        throw DL_ABORT_EX(fmt("Bad range {}", std::string(i, j).c_str()));
      }
    }
    if (j == eoi) {
      break;
    }
    i = j + 1;
  }
  return sgl;
}

namespace {
void computeHeadPieces(
    std::vector<size_t>& indexes,
    const std::vector<std::shared_ptr<FileEntry>>& fileEntries,
    size_t pieceLength, int64_t head)
{
  if (head == 0) {
    return;
  }
  for (const auto& fi : fileEntries) {
    if (fi->getLength() == 0) {
      continue;
    }
    const size_t lastIndex =
        (fi->getOffset() + std::min(head, fi->getLength()) - 1) / pieceLength;
    for (size_t idx = fi->getOffset() / pieceLength; idx <= lastIndex; ++idx) {
      indexes.push_back(idx);
    }
  }
}
} // namespace

namespace {
void computeTailPieces(
    std::vector<size_t>& indexes,
    const std::vector<std::shared_ptr<FileEntry>>& fileEntries,
    size_t pieceLength, int64_t tail)
{
  if (tail == 0) {
    return;
  }
  for (const auto& fi : fileEntries) {
    if (fi->getLength() == 0) {
      continue;
    }
    int64_t endOffset = fi->getLastOffset();
    size_t fromIndex =
        (endOffset - 1 - (std::min(tail, fi->getLength()) - 1)) / pieceLength;
    const size_t toIndex = (endOffset - 1) / pieceLength;
    while (fromIndex <= toIndex) {
      indexes.push_back(fromIndex++);
    }
  }
}
} // namespace

void parsePrioritizePieceRange(
    std::vector<size_t>& result, const std::string& src,
    const std::vector<std::shared_ptr<FileEntry>>& fileEntries,
    size_t pieceLength, int64_t defaultSize)
{
  std::vector<size_t> indexes;
  std::vector<Scip> parts;
  splitIter(src.begin(), src.end(), std::back_inserter(parts), ',', true);
  for (const auto& i : parts) {
    if (util::streq(i.first, i.second, "head")) {
      computeHeadPieces(indexes, fileEntries, pieceLength, defaultSize);
    }
    else if (util::startsWith(i.first, i.second, "head=")) {
      std::string sizestr(i.first + 5, i.second);
      computeHeadPieces(indexes, fileEntries, pieceLength,
                        std::max((int64_t)0, getRealSize(sizestr)));
    }
    else if (util::streq(i.first, i.second, "tail")) {
      computeTailPieces(indexes, fileEntries, pieceLength, defaultSize);
    }
    else if (util::startsWith(i.first, i.second, "tail=")) {
      std::string sizestr(i.first + 5, i.second);
      computeTailPieces(indexes, fileEntries, pieceLength,
                        std::max((int64_t)0, getRealSize(sizestr)));
    }
    else {
      throw DL_ABORT_EX(
          fmt("Unrecognized token {}", std::string(i.first, i.second).c_str()));
    }
  }
  std::sort(indexes.begin(), indexes.end());
  indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
  result.insert(result.end(), indexes.begin(), indexes.end());
}

// Converts ISO/IEC 8859-1 string to UTF-8 string.  If there is a
// character not in ISO/IEC 8859-1, returns empty string.
std::string iso8859p1ToUtf8(std::span<const std::byte> src)
{
  std::string dest;
  for (auto b : src) {
    auto c = static_cast<unsigned char>(b);
    if (0xa0u <= c) {
      if (c <= 0xbfu) {
        dest += 0xc2u;
      }
      else {
        dest += 0xc3u;
      }
      dest += c & (~0x40u);
    }
    else if (0x80u <= c && c <= 0x9fu) {
      return "";
    }
    else {
      dest += static_cast<char>(c);
    }
  }
  return dest;
}

std::string iso8859p1ToUtf8(const std::string& src)
{
  return iso8859p1ToUtf8(std::as_bytes(std::span{src.data(), src.size()}));
}

/* Start of utf8 dfa */
/* Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 *
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static const uint8_t utf8d[] = {
    /*
     * The first part of the table maps bytes to character classes that
     * to reduce the size of the transition table and create bitmasks.
     */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    9,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    7,
    8,
    8,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    10,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    4,
    3,
    3,
    11,
    6,
    6,
    6,
    5,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,

    /*
     * The second part is a transition table that maps a combination
     * of a state of the automaton and a character class to a state.
     */
    0,
    12,
    24,
    36,
    60,
    96,
    84,
    12,
    12,
    12,
    48,
    72,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    0,
    12,
    12,
    12,
    12,
    12,
    0,
    12,
    0,
    12,
    12,
    12,
    24,
    12,
    12,
    12,
    12,
    12,
    24,
    12,
    24,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    24,
    12,
    12,
    12,
    12,
    12,
    24,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    24,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    36,
    12,
    36,
    12,
    12,
    12,
    36,
    12,
    12,
    12,
    12,
    12,
    36,
    12,
    36,
    12,
    12,
    12,
    36,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
    12,
};

static uint32_t utf8dfa(uint32_t* state, uint32_t* codep, uint32_t byte)
{
  uint32_t type = utf8d[byte];

  *codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6)
                                   : (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

/* End of utf8 dfa */

typedef enum {
  CD_BEFORE_DISPOSITION_TYPE,
  CD_AFTER_DISPOSITION_TYPE,
  CD_DISPOSITION_TYPE,
  CD_BEFORE_DISPOSITION_PARM_NAME,
  CD_AFTER_DISPOSITION_PARM_NAME,
  CD_DISPOSITION_PARM_NAME,
  CD_BEFORE_VALUE,
  CD_AFTER_VALUE,
  CD_QUOTED_STRING,
  CD_TOKEN,
  CD_BEFORE_EXT_VALUE,
  CD_CHARSET,
  CD_LANGUAGE,
  CD_VALUE_CHARS,
  CD_VALUE_CHARS_PCT_ENCODED1,
  CD_VALUE_CHARS_PCT_ENCODED2
} content_disposition_parse_state;

typedef enum {
  CD_FILENAME_FOUND = 1,
  CD_EXT_FILENAME_FOUND = 1 << 1
} content_disposition_parse_flag;

typedef enum {
  CD_ENC_UNKNOWN,
  CD_ENC_UTF8,
  CD_ENC_ISO_8859_1
} content_disposition_charset;

ssize_t parse_content_disposition(std::span<std::byte> dest,
                                  std::span<const std::byte>* charsetp,
                                  std::span<const std::byte> in,
                                  bool defaultUTF8)
{
  const size_t destlen = dest.size();
  const char* const in_base = reinterpret_cast<const char*>(in.data());
  const char *p = in_base, *eop = in_base + in.size(),
             *mark_first = nullptr, *mark_last = nullptr;
  int state = CD_BEFORE_DISPOSITION_TYPE;
  int in_file_parm = 0;
  int flags = 0;
  int quoted_seen = 0;
  int charset = 0;
  /* To suppress warnings */
  char* const dest_base = reinterpret_cast<char*>(dest.data());
  char* dp = dest_base;
  size_t dlen = destlen;
  uint32_t dfa_state = UTF8_ACCEPT;
  uint32_t dfa_code = 0;
  uint8_t pctval = 0;

  *charsetp = std::span<const std::byte>();

  for (; p != eop; ++p) {
    switch (state) {
    case CD_BEFORE_DISPOSITION_TYPE:
      if (inRFC2616HttpToken(*p)) {
        state = CD_DISPOSITION_TYPE;
      }
      else if (!isLws(*p)) {
        return -1;
      }
      break;
    case CD_AFTER_DISPOSITION_TYPE:
    case CD_DISPOSITION_TYPE:
      if (*p == ';') {
        state = CD_BEFORE_DISPOSITION_PARM_NAME;
      }
      else if (isLws(*p)) {
        state = CD_AFTER_DISPOSITION_TYPE;
      }
      else if (state == CD_AFTER_DISPOSITION_TYPE || !inRFC2616HttpToken(*p)) {
        return -1;
      }
      break;
    case CD_BEFORE_DISPOSITION_PARM_NAME:
      if (inRFC2616HttpToken(*p)) {
        mark_first = p;
        state = CD_DISPOSITION_PARM_NAME;
      }
      else if (!isLws(*p)) {
        return -1;
      }
      break;
    case CD_AFTER_DISPOSITION_PARM_NAME:
    case CD_DISPOSITION_PARM_NAME:
      if (*p == '=') {
        if (state == CD_DISPOSITION_PARM_NAME) {
          mark_last = p;
        }
        in_file_parm = 0;
        if (strieq(mark_first, mark_last, "filename*")) {
          if ((flags & CD_EXT_FILENAME_FOUND) == 0) {
            in_file_parm = 1;
          }
          else {
            return -1;
          }
          state = CD_BEFORE_EXT_VALUE;
        }
        else if (strieq(mark_first, mark_last, "filename")) {
          if (flags & CD_FILENAME_FOUND) {
            return -1;
          }
          if ((flags & CD_EXT_FILENAME_FOUND) == 0) {
            in_file_parm = 1;
          }
          state = CD_BEFORE_VALUE;
        }
        else {
          /* ext-token must be characters in token, followed by "*" */
          if (mark_first != mark_last - 1 && *(mark_last - 1) == '*') {
            state = CD_BEFORE_EXT_VALUE;
          }
          else {
            state = CD_BEFORE_VALUE;
          }
        }
        if (in_file_parm) {
          dp = dest_base;
          dlen = destlen;
        }
      }
      else if (isLws(*p)) {
        mark_last = p;
        state = CD_AFTER_DISPOSITION_PARM_NAME;
      }
      else if (state == CD_AFTER_DISPOSITION_PARM_NAME ||
               !inRFC2616HttpToken(*p)) {
        return -1;
      }
      break;
    case CD_BEFORE_VALUE:
      if (*p == '"') {
        quoted_seen = 0;
        state = CD_QUOTED_STRING;
        if (defaultUTF8) {
          dfa_state = UTF8_ACCEPT;
          dfa_code = 0;
        }
      }
      else if (inRFC2616HttpToken(*p)) {
        if (in_file_parm) {
          if (dlen == 0) {
            return -1;
          }
          else {
            *dp++ = *p;
            --dlen;
          }
        }
        state = CD_TOKEN;
      }
      else if (!isLws(*p)) {
        return -1;
      }
      break;
    case CD_AFTER_VALUE:
      if (*p == ';') {
        state = CD_BEFORE_DISPOSITION_PARM_NAME;
      }
      else if (!isLws(*p)) {
        return -1;
      }
      break;
    case CD_QUOTED_STRING:
      if (*p == '\\' && quoted_seen == 0) {
        quoted_seen = 1;
      }
      else if (*p == '"' && quoted_seen == 0) {
        if (defaultUTF8 && dfa_state != UTF8_ACCEPT) {
          return -1;
        }
        if (in_file_parm) {
          flags |= CD_FILENAME_FOUND;
        }
        state = CD_AFTER_VALUE;
      }
      else {
        /* TEXT which is OCTET except CTLs, but including LWS. Accept
           ISO-8859-1 chars, or UTF-8 if defaultUTF8 is set */
        quoted_seen = 0;
        if (defaultUTF8) {
          if (utf8dfa(&dfa_state, &dfa_code, (unsigned char)*p) ==
              UTF8_REJECT) {
            return -1;
          }
        }
        else if (!isIso8859p1(*p)) {
          return -1;
        }
        if (in_file_parm) {
          if (dlen == 0) {
            return -1;
          }
          else {
            *dp++ = *p;
            --dlen;
          }
        }
      }
      break;
    case CD_TOKEN:
      if (inRFC2616HttpToken(*p)) {
        if (in_file_parm) {
          if (dlen == 0) {
            return -1;
          }
          else {
            *dp++ = *p;
            --dlen;
          }
        }
      }
      else if (*p == ';') {
        if (in_file_parm) {
          flags |= CD_FILENAME_FOUND;
        }
        state = CD_BEFORE_DISPOSITION_PARM_NAME;
      }
      else if (isLws(*p)) {
        if (in_file_parm) {
          flags |= CD_FILENAME_FOUND;
        }
        state = CD_AFTER_VALUE;
      }
      else {
        return -1;
      }
      break;
    case CD_BEFORE_EXT_VALUE:
      if (*p == '\'') {
        /* Empty charset is not allowed */
        return -1;
      }
      else if (inRFC2978MIMECharset(*p)) {
        mark_first = p;
        state = CD_CHARSET;
      }
      else if (!isLws(*p)) {
        return -1;
      }
      break;
    case CD_CHARSET:
      if (*p == '\'') {
        mark_last = p;
        *charsetp = in.subspan(mark_first - in_base, mark_last - mark_first);
        if (strieq(mark_first, mark_last, "utf-8")) {
          charset = CD_ENC_UTF8;
          dfa_state = UTF8_ACCEPT;
          dfa_code = 0;
        }
        else if (strieq(mark_first, mark_last, "iso-8859-1")) {
          charset = CD_ENC_ISO_8859_1;
        }
        else {
          charset = CD_ENC_UNKNOWN;
        }
        state = CD_LANGUAGE;
      }
      else if (!inRFC2978MIMECharset(*p)) {
        return -1;
      }
      break;
    case CD_LANGUAGE:
      if (*p == '\'') {
        if (in_file_parm) {
          dp = dest_base;
          dlen = destlen;
        }
        state = CD_VALUE_CHARS;
      }
      else if (*p != '-' && !isAlpha(*p) && !isDigit(*p)) {
        return -1;
      }
      break;
    case CD_VALUE_CHARS:
      if (inRFC5987AttrChar(*p)) {
        if (charset == CD_ENC_UTF8) {
          if (utf8dfa(&dfa_state, &dfa_code, static_cast<unsigned char>(*p)) ==
              UTF8_REJECT) {
            return -1;
          }
        }
        if (in_file_parm) {
          if (dlen == 0) {
            return -1;
          }
          else {
            *dp++ = *p;
            --dlen;
          }
        }
      }
      else if (*p == '%') {
        if (in_file_parm) {
          if (dlen == 0) {
            return -1;
          }
        }
        pctval = 0;
        state = CD_VALUE_CHARS_PCT_ENCODED1;
      }
      else if (*p == ';' || isLws(*p)) {
        if (charset == CD_ENC_UTF8 && dfa_state != UTF8_ACCEPT) {
          return -1;
        }
        if (in_file_parm) {
          flags |= CD_EXT_FILENAME_FOUND;
        }
        if (*p == ';') {
          state = CD_BEFORE_DISPOSITION_PARM_NAME;
        }
        else {
          state = CD_AFTER_VALUE;
        }
      }
      else if (!inRFC5987AttrChar(*p)) {
        return -1;
      }
      break;
    case CD_VALUE_CHARS_PCT_ENCODED1:
      if (isHexDigit(*p)) {
        pctval |= hexCharToUInt(*p) << 4;
        state = CD_VALUE_CHARS_PCT_ENCODED2;
      }
      else {
        return -1;
      }
      break;
    case CD_VALUE_CHARS_PCT_ENCODED2:
      if (isHexDigit(*p)) {
        pctval |= hexCharToUInt(*p);
        if (charset == CD_ENC_UTF8) {
          if (utf8dfa(&dfa_state, &dfa_code, pctval) == UTF8_REJECT) {
            return -1;
          }
        }
        else if (charset == CD_ENC_ISO_8859_1) {
          if (!isIso8859p1(pctval)) {
            return -1;
          }
        }
        if (in_file_parm) {
          *dp++ = pctval;
          --dlen;
        }
        state = CD_VALUE_CHARS;
      }
      else {
        return -1;
      }
      break;
    }
  }
  switch (state) {
  case CD_BEFORE_DISPOSITION_TYPE:
  case CD_AFTER_DISPOSITION_TYPE:
  case CD_DISPOSITION_TYPE:
  case CD_AFTER_VALUE:
  case CD_TOKEN:
    return destlen - dlen;
  case CD_VALUE_CHARS:
    if (charset == CD_ENC_UTF8 && dfa_state != UTF8_ACCEPT) {
      return -1;
    }
    return destlen - dlen;
  default:
    return -1;
  }
}

std::string getContentDispositionFilename(const std::string& header,
                                          bool defaultUTF8)
{
  std::array<char, 1_k> cdval;
  std::span<const std::byte> charset;
  ssize_t rv = parse_content_disposition(
      std::as_writable_bytes(std::span{cdval.data(), cdval.size()}), &charset,
      std::as_bytes(std::span{header.data(), header.size()}), defaultUTF8);
  if (rv == -1) {
    return "";
  }

  std::string res;
  if ((!charset.empty() &&
       strieq(reinterpret_cast<const char*>(charset.data()),
              reinterpret_cast<const char*>(charset.data()) + charset.size(),
              "iso-8859-1")) ||
      (charset.empty() && !defaultUTF8)) {
    res = iso8859p1ToUtf8(
        std::as_bytes(std::span{cdval.data(), static_cast<size_t>(rv)}));
  }
  else {
    res.assign(cdval.data(), rv);
  }
  if (!detectDirTraversal(res) &&
      res.find_first_of("/\\") == std::string::npos) {
    return res;
  }
  return "";
}

std::string toUpper(std::string src)
{
  uppercase(src);
  return src;
}

std::string toLower(std::string src)
{
  lowercase(src);
  return src;
}

void uppercase(std::string& s)
{
  std::transform(s.begin(), s.end(), s.begin(), toUpperChar);
}

void lowercase(std::string& s)
{
  std::transform(s.begin(), s.end(), s.begin(), toLowerChar);
}

char toUpperChar(char c)
{
  if ('a' <= c && c <= 'z') {
    c += 'A' - 'a';
  }
  return c;
}

char toLowerChar(char c)
{
  if ('A' <= c && c <= 'Z') {
    c += 'a' - 'A';
  }
  return c;
}

bool isNumericHost(const std::string& name)
{
  struct addrinfo hints;
  struct addrinfo* res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;
  if (getaddrinfo(name.c_str(), nullptr, &hints, &res)) {
    return false;
  }
  freeaddrinfo(res);
  return true;
}

void setGlobalSignalHandler(int sig, sigset_t* mask, signal_handler_t handler,
                            int flags)
{
#ifdef HAVE_SIGACTION
  struct sigaction sigact;
  sigact.sa_handler = handler;
  sigact.sa_flags = flags;
  sigact.sa_mask = *mask;
  if (sigaction(sig, &sigact, nullptr) == -1) {
    auto errNum = errno;
    A2_LOG_ERROR(fmt("sigaction() failed for signal {}: {}", sig,
                     safeStrerror(errNum).c_str()));
  }
#else
  if (signal(sig, handler) == SIG_ERR) {
    auto errNum = errno;
    A2_LOG_ERROR(fmt("signal() failed for signal {}: {}", sig,
                     safeStrerror(errNum).c_str()));
  }
#endif // HAVE_SIGACTION
}

std::string getHomeDir()
{
  const char* p = getenv("HOME");
  if (p) {
    return p;
  }
#ifdef HAVE_PWD_H
  auto pw = getpwuid(geteuid());
  if (pw && pw->pw_dir) {
    return pw->pw_dir;
  }
#endif // HAVE_PWD_H
  return A2STR::NIL;
}

std::string getXDGDir(const std::string& environmentVariable,
                      const std::string& fallbackDirectory)
{
  std::string filename;
  const char* p = getenv(environmentVariable.c_str());
  if (p && p[0] == '/') {
    filename = p;
  }
  else {
    filename = fallbackDirectory;
  }
  return filename;
}

std::string getConfigFile()
{
  std::string filename = getHomeDir() + "/.aria2/aria2.conf";
  if (!File(filename).exists()) {
    filename = getXDGDir("XDG_CONFIG_HOME", getHomeDir() + "/.config") +
               "/aria2/aria2.conf";
  }
  return filename;
}

std::string getDHTFile(bool ipv6)
{
  std::string filename =
      getHomeDir() + (ipv6 ? "/.aria2/dht6.dat" : "/.aria2/dht.dat");
  if (!File(filename).exists()) {
    filename = getXDGDir("XDG_CACHE_HOME", getHomeDir() + "/.cache") +
               (ipv6 ? "/aria2/dht6.dat" : "/aria2/dht.dat");
  }
  return filename;
}

int64_t getRealSize(const std::string& sizeWithUnit)
{
  std::string::size_type p = sizeWithUnit.find_first_of("KMkm");
  std::string size;
  int32_t mult = 1;
  if (p == std::string::npos) {
    size = sizeWithUnit;
  }
  else {
    switch (sizeWithUnit[p]) {
    case 'K':
    case 'k':
      mult = 1_k;
      break;
    case 'M':
    case 'm':
      mult = 1_m;
      break;
    }
    size.assign(sizeWithUnit.begin(), sizeWithUnit.begin() + p);
  }
  int64_t v;
  if (!parseLLIntNoThrow(v, size) || v < 0) {
    throw DL_ABORT_EX(
        fmt("Bad or negative value detected: {}", sizeWithUnit.c_str()));
  }
  if (INT64_MAX / mult < v) {
    throw DL_ABORT_EX(
        fmt(MSG_STRING_INTEGER_CONVERSION_FAILURE, "overflow/underflow"));
  }
  return v * mult;
}

std::string abbrevSize(int64_t size)
{
  static const char* UNITS[] = {"", "Ki", "Mi", "Gi"};
  int64_t t = size;
  size_t uidx = 0;
  int r = 0;
  while (t >= static_cast<int64_t>(1_k) &&
         uidx + 1 < sizeof(UNITS) / sizeof(UNITS[0])) {
    lldiv_t d = lldiv(t, 1_k);
    t = d.quot;
    r = d.rem;
    ++uidx;
  }
  if (uidx + 1 < sizeof(UNITS) / sizeof(UNITS[0]) && t >= 922) {
    ++uidx;
    r = t;
    t = 0;
  }
  std::string res;
  res += itos(t, true);
  if (t < 10 && uidx > 0) {
    res += ".";
    res += itos(r * 10 / 1_k);
  }
  res += UNITS[uidx];
  return res;
}

void sleep(long seconds)
{
#if HAVE_SLEEP
  ::sleep(seconds);
#elif defined(HAVE_USLEEP)
  ::usleep(seconds * 1000000);
#else
#  error no sleep function is available (nanosleep?)
#endif
}

void usleep(long microseconds)
{
#ifdef HAVE_USLEEP
  ::usleep(microseconds);
#else
#  error no usleep function is available (nanosleep?)
#endif
}

void mkdirs(const std::string& dirpath)
{
  File dir(dirpath);
  if (!dir.mkdirs()) {
    int errNum = errno;
    if (!dir.isDir()) {
      throw DL_ABORT_EX3(
          errNum,
          fmt(EX_MAKE_DIR, dir.getPath().c_str(), safeStrerror(errNum).c_str()),
          error_code::DIR_CREATE_ERROR);
    }
  }
}

void convertBitfield(BitfieldMan* dest, const BitfieldMan* src)
{
  size_t numBlock = dest->countBlock();
  for (size_t index = 0; index < numBlock; ++index) {
    if (src->isBitSetOffsetRange((int64_t)index * dest->getBlockLength(),
                                 dest->getBlockLength())) {
      dest->setBit(index);
    }
  }
}

std::string toString(const std::shared_ptr<BinaryStream>& binaryStream)
{
  std::string result;
  std::array<char, 2048> data;
  while (true) {
    int32_t dataLength = binaryStream->readData(
        reinterpret_cast<unsigned char*>(data.data()), data.size(),
        static_cast<int64_t>(result.size()));
    result.append(data.data(), dataLength);
    if (dataLength == 0) {
      break;
    }
  }
  return result;
}

#ifdef HAVE_POSIX_MEMALIGN
/**
 * In linux 2.6, alignment and size should be a multiple of 512.
 */
void* allocateAlignedMemory(size_t alignment, size_t size)
{
  void* buffer;
  int res;
  if ((res = posix_memalign(&buffer, alignment, size)) != 0) {
    throw FATAL_EXCEPTION(
        fmt("Error in posix_memalign: {}", util::safeStrerror(res).c_str()));
  }
  return buffer;
}
#endif // HAVE_POSIX_MEMALIGN

Endpoint getNumericNameInfo(const struct sockaddr* sockaddr, socklen_t len)
{
  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  int s = getnameinfo(sockaddr, len, host, NI_MAXHOST, service, NI_MAXSERV,
                      NI_NUMERICHOST | NI_NUMERICSERV);
  if (s != 0) {
    throw DL_ABORT_EX(
        fmt("Failed to get hostname and port. cause: {}", gai_strerror(s)));
  }
  return {host, sockaddr->sa_family,
          static_cast<uint16_t>(strtoul(service, nullptr, 10))};
}

std::string htmlEscape(const std::string& src)
{
  std::string dest;
  dest.reserve(src.size());
  auto j = std::begin(src);
  for (auto i = std::begin(src); i != std::end(src); ++i) {
    char ch = *i;
    const char* repl;
    if (ch == '<') {
      repl = "&lt;";
    }
    else if (ch == '>') {
      repl = "&gt;";
    }
    else if (ch == '&') {
      repl = "&amp;";
    }
    else if (ch == '\'') {
      repl = "&#39;";
    }
    else if (ch == '"') {
      repl = "&quot;";
    }
    else {
      continue;
    }
    dest.append(j, i);
    j = i + 1;
    dest += repl;
  }
  dest.append(j, std::end(src));
  return dest;
}

std::pair<size_t, std::string> parseIndexPath(const std::string& line)
{
  auto p = divide(std::begin(line), std::end(line), '=');
  uint32_t index;
  if (!parseUIntNoThrow(index, std::string(p.first.first, p.first.second))) {
    throw DL_ABORT_EX("Bad path index");
  }
  if (p.second.first == p.second.second) {
    throw DL_ABORT_EX(fmt("Path with index={} is empty.", index));
  }
  return std::make_pair(index, std::string(p.second.first, p.second.second));
}

std::vector<std::pair<size_t, std::string>> createIndexPaths(std::istream& i)
{
  std::vector<std::pair<size_t, std::string>> indexPaths;
  std::string line;
  while (getline(i, line)) {
    indexPaths.push_back(parseIndexPath(line));
  }
  return indexPaths;
}

void generateRandomData(std::span<std::byte> data)
{
  const auto& rd = SimpleRandomizer::getInstance();
  return rd->getRandomBytes(
      reinterpret_cast<unsigned char*>(data.data()), data.size());
}

bool saveAs(const std::string& filename, const std::string& data,
            bool overwrite)
{
  if (!overwrite && File(filename).exists()) {
    return false;
  }
  std::string tempFilename = filename;
  tempFilename += "__temp";
  {
    BufferedFile fp(tempFilename.c_str(), BufferedFile::WRITE);
    if (!fp) {
      return false;
    }
    if (fp.write(data.data(), data.size()) != data.size()) {
      return false;
    }
    if (fp.close() == EOF) {
      return false;
    }
  }
  return File(tempFilename).renameTo(filename);
}

std::string applyDir(const std::string& dir, const std::string& relPath)
{
  std::string s;
  if (dir.empty()) {
    s = "./";
    s += relPath;
  }
  else {
    s = dir;
    if (dir == "/") {
      s += relPath;
    }
    else {
      s += "/";
      s += relPath;
    }
  }
  return s;
}

std::string fixTaintedBasename(const std::string& src)
{
  return escapePath(replace(src, "/", "%2F"));
}

void generateRandomKey(unsigned char* key)
{
  unsigned char bytes[40];
  generateRandomData(
      std::as_writable_bytes(std::span{bytes, sizeof(bytes)}));
  message_digest::digest(key, 20, MessageDigest::sha1().get(),
                         std::as_bytes(std::span{bytes}));
}

// Returns true is given numeric ipv4addr is in Private Address Space.
//
// From Section.3 RFC1918
// 10.0.0.0        -   10.255.255.255  (10/8 prefix)
// 172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
// 192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
bool inPrivateAddress(const std::string& ipv4addr)
{
  if (ipv4addr.starts_with("10.") || ipv4addr.starts_with("192.168.")) {
    return true;
  }
  if (ipv4addr.starts_with("172.")) {
    for (int i = 16; i <= 31; ++i) {
      std::string t(fmt("{}.", i));
      if (util::startsWith(ipv4addr.begin() + 4, ipv4addr.end(), t.begin(),
                           t.end())) {
        return true;
      }
    }
  }
  return false;
}

bool detectDirTraversal(const std::string& s)
{
  if (s.empty()) {
    return false;
  }
  for (auto c : s) {
    unsigned char ch = c;
    if (in(ch, 0x00u, 0x1fu) || ch == 0x7fu) {
      return true;
    }
  }
  return s == "." || s == ".." || s[0] == '/' || s.starts_with("./") ||
         s.starts_with("../") || s.find("/../") != std::string::npos ||
         s.find("/./") != std::string::npos || s[s.size() - 1] == '/' ||
         s.ends_with("/.") || s.ends_with("/..");
}

std::string escapePath(const std::string& s)
{
  // We don't escape '/' because we use it as a path separator.
  std::string d;
  for (auto cc : s) {
    unsigned char c = cc;
    if (in(c, 0x00u, 0x1fu) || c == 0x7fu) {
      d += fmt("%{:02X}", c);
    }
    else {
      d += c;
    }
  }
  return d;
}

bool inSameCidrBlock(const std::string& ip1, const std::string& ip2,
                     size_t bits)
{
  unsigned char s1[16], s2[16];
  size_t len1, len2;
  if ((len1 = net::getBinAddr(s1, ip1)) == 0 ||
      (len2 = net::getBinAddr(s2, ip2)) == 0 || len1 != len2) {
    return false;
  }
  if (bits == 0) {
    return true;
  }
  if (bits > 8 * len1) {
    bits = 8 * len1;
  }
  int last = (bits - 1) / 8;
  for (int i = 0; i < last; ++i) {
    if (s1[i] != s2[i]) {
      return false;
    }
  }
  unsigned char mask = bitfield::lastByteMask(bits);
  return (s1[last] & mask) == (s2[last] & mask);
}

namespace {

void executeHook(const std::string& command, a2_gid_t gid, size_t numFiles,
                 const std::string& firstFilename)
{
  const std::string gidStr = GroupId::toHex(gid);
  const std::string numFilesStr = util::uitos(numFiles);
  A2_LOG_INFO(fmt("Executing user command: {} {} {} {}", command.c_str(),
                  gidStr.c_str(), numFilesStr.c_str(), firstFilename.c_str()));
  pid_t cpid = fork();
  if (cpid == 0) {
    // child!
    execlp(command.c_str(), command.c_str(), gidStr.c_str(),
           numFilesStr.c_str(), firstFilename.c_str(),
           reinterpret_cast<char*>(0));
    perror(("Could not execute user command: " + command).c_str());
    _exit(EXIT_FAILURE);
    return;
  }

  if (cpid == -1) {
    A2_LOG_ERROR("fork() failed. Cannot execute user command.");
  }
  return;
}
} // namespace

void executeHookByOptName(const std::shared_ptr<RequestGroup>& group,
                          const Option* option, PrefPtr pref)
{
  executeHookByOptName(group.get(), option, pref);
}

void executeHookByOptName(const RequestGroup* group, const Option* option,
                          PrefPtr pref)
{
  const std::string& cmd = option->get(pref);
  if (!cmd.empty()) {
    const std::shared_ptr<DownloadContext> dctx = group->getDownloadContext();
    std::string firstFilename;
    size_t numFiles = 0;
    if (!group->inMemoryDownload()) {
      std::shared_ptr<FileEntry> file = dctx->getFirstRequestedFileEntry();
      if (file) {
        firstFilename = file->getPath();
      }
      numFiles = dctx->countRequestedFileEntry();
    }
    executeHook(cmd, group->getGID(), numFiles, firstFilename);
  }
}

std::string createSafePath(const std::string& dir, const std::string& filename)
{
  return util::applyDir(dir,
                        util::isUtf8(filename)
                            ? util::fixTaintedBasename(filename)
                            : util::escapePath(util::percentEncode(filename)));
}

std::string createSafePath(const std::string& filename)
{
  return util::isUtf8(filename)
             ? util::fixTaintedBasename(filename)
             : util::escapePath(util::percentEncode(filename));
}

std::string encodeNonUtf8(const std::string& s)
{
  return util::isUtf8(s) ? s : util::percentEncode(s);
}

std::string makeString(const char* str)
{
  if (!str) {
    return A2STR::NIL;
  }
  return str;
}

std::string safeStrerror(int errNum) { return makeString(strerror(errNum)); }

bool noProxyDomainMatch(const std::string& hostname, const std::string& domain)
{
  if (!domain.empty() && domain[0] == '.' && !util::isNumericHost(hostname)) {
    return hostname.ends_with(domain);
  }
  return hostname == domain;
}

bool tlsHostnameMatch(const std::string& pattern, const std::string& hostname)
{
  std::string::const_iterator ptWildcard =
      std::find(pattern.begin(), pattern.end(), '*');
  if (ptWildcard == pattern.end()) {
    return strieq(pattern.begin(), pattern.end(), hostname.begin(),
                  hostname.end());
  }
  std::string::const_iterator ptLeftLabelEnd =
      std::find(pattern.begin(), pattern.end(), '.');
  bool wildcardEnabled = true;
  // Do case-insensitive match. At least 2 dots are required to enable
  // wildcard match. Also wildcard must be in the left-most label.
  // Don't attempt to match a presented identifier where the wildcard
  // character is embedded within an A-label.
  if (ptLeftLabelEnd == pattern.end() ||
      std::find(ptLeftLabelEnd + 1, pattern.end(), '.') == pattern.end() ||
      ptLeftLabelEnd < ptWildcard || istartsWith(pattern, "xn--")) {
    wildcardEnabled = false;
  }
  if (!wildcardEnabled) {
    return strieq(pattern.begin(), pattern.end(), hostname.begin(),
                  hostname.end());
  }
  std::string::const_iterator hnLeftLabelEnd =
      std::find(hostname.begin(), hostname.end(), '.');
  if (!strieq(ptLeftLabelEnd, pattern.end(), hnLeftLabelEnd, hostname.end())) {
    return false;
  }
  // Perform wildcard match. Here '*' must match at least one
  // character.
  if (hnLeftLabelEnd - hostname.begin() < ptLeftLabelEnd - pattern.begin()) {
    return false;
  }
  return istartsWith(hostname.begin(), hnLeftLabelEnd, pattern.begin(),
                     ptWildcard) &&
         iendsWith(hostname.begin(), hnLeftLabelEnd, ptWildcard + 1,
                   ptLeftLabelEnd);
}

bool strieq(const std::string& a, const char* b)
{
  return strieq(a.begin(), a.end(), b);
}

bool strieq(const std::string& a, const std::string& b)
{
  return strieq(a.begin(), a.end(), b.begin(), b.end());
}

bool istartsWith(const std::string& a, const char* b)
{
  return istartsWith(a.begin(), a.end(), b);
}

bool istartsWith(const std::string& a, const std::string& b)
{
  return istartsWith(std::begin(a), std::end(a), std::begin(b), std::end(b));
}

bool iendsWith(const std::string& a, const char* b)
{
  return iendsWith(a.begin(), a.end(), b, b + strlen(b));
}

bool iendsWith(const std::string& a, const std::string& b)
{
  return iendsWith(a.begin(), a.end(), b.begin(), b.end());
}

bool strless(const char* a, const char* b) { return strcmp(a, b) < 0; }

#ifdef ENABLE_SSL
TLSVersion toTLSVersion(const std::string& ver)
{
  if (ver == A2_V_TLS11) {
    return TLS_PROTO_TLS11;
  }
  if (ver == A2_V_TLS12) {
    return TLS_PROTO_TLS12;
  }
  if (ver == A2_V_TLS13) {
    return TLS_PROTO_TLS13;
  }
  return TLS_PROTO_TLS12;
}
#endif // ENABLE_SSL

void make_fd_cloexec(int fd)
{
  int flags;

  // TODO from linux man page, fcntl() with F_GETFD or F_SETFD does
  // not return -1 with errno == EINTR.  Historically, aria2 code base
  // checks this case.  Probably, it is not needed.
  while ((flags = fcntl(fd, F_GETFD)) == -1 && errno == EINTR)
    ;
  if (flags == -1) {
    return;
  }

  while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1 && errno == EINTR)
    ;
}

} // namespace util

} // namespace aria2
