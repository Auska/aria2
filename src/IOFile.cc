/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2011 Tatsuhiro Tsujikawa
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
#include "IOFile.h"

#include <array>
#include <cstring>
#include <ostream>

#include "a2io.h"
#include "util.h"

namespace aria2 {

const char IOFile::READ[] = "rb";
const char IOFile::WRITE[] = "wb";
const char IOFile::APPEND[] = "ab";

IOFile::operator unspecified_bool_type() const
{
  bool ok = isOpen() && !isError();
  return ok ? &IOFile::goodState : nullptr;
}

size_t IOFile::read(std::span<std::byte> data) { return onRead(data); }

size_t IOFile::write(std::span<const std::byte> data) { return onWrite(data); }

size_t IOFile::write(std::string_view str)
{
  return write(std::as_bytes(std::span{str}));
}

char* IOFile::gets(char* s, int size) { return onGets(s, size); }

char* IOFile::getsn(char* s, int size)
{
  char* ptr = gets(s, size);
  if (ptr) {
    int len = strlen(ptr);
    if (ptr[len - 1] == '\n') {
      ptr[len - 1] = '\0';
    }
  }
  return ptr;
}

std::string IOFile::getLine()
{
  std::string res;
  if (eof()) {
    return res;
  }
  std::array<char, 4_k> buf;
  while (gets(buf.data(), buf.size())) {
    size_t len = strlen(buf.data());
    bool lineBreak = false;
    if (buf[len - 1] == '\n') {
      --len;
      lineBreak = true;
    }
    res.append(buf.data(), len);
    if (lineBreak) {
      break;
    }
  }
  return res;
}

int IOFile::close() { return onClose(); }

bool IOFile::eof() { return !isOpen() || isEOF(); }

size_t IOFile::transfer(std::ostream& out)
{
  size_t count = 0;
  std::array<std::byte, 4_k> buf;
  while (1) {
    size_t r = this->read(std::span{buf});
    out.write(reinterpret_cast<const char*>(buf.data()),
              static_cast<std::streamsize>(r));
    count += r;
    if (r < buf.size()) {
      break;
    }
  }
  return count;
}

std::string IOFile::readAll()
{
  std::string out;
  std::array<std::byte, 4_k> buf;
  while (1) {
    size_t r = this->read(std::span{buf});
    out.append(reinterpret_cast<const char*>(buf.data()), r);
    if (r < buf.size()) {
      break;
    }
  }
  return out;
}

int IOFile::flush() { return onFlush(); }

bool IOFile::supportsColor() { return onSupportsColor(); }

} // namespace aria2
