/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2013 Nils Maier
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

#include "GZipFile.h"

#include <algorithm>
#include <limits>

#include "a2io.h"
#include "util.h"

namespace aria2 {

GZipFile::GZipFile(const char* filename, const char* mode) : fp_(nullptr)
{
  auto fp = strcmp(DEV_STDIN, filename) == 0 ? stdin : a2fopen(filename, mode);
  if (fp) {
    int fd = dup(fileno(fp));
    if (fd != -1) {
      fp_ = gzdopen(fd, mode);
      if (fp_) {
// fp_ retains fd and gzclose() will close fd as well.
#if HAVE_GZBUFFER
        gzbuffer(fp_, 1 << 17);
#endif
#if HAVE_GZSETPARAMS
        gzsetparams(fp_, 2, Z_DEFAULT_STRATEGY);
#endif
      }
      else {
        ::close(fd);
      }
    }
    fclose(fp);
  }
}

GZipFile::~GZipFile() { close(); }

int GZipFile::onClose()
{
  int rv = 0;
  if (fp_) {
    rv = gzclose(fp_);
    fp_ = nullptr;
  }
  return rv;
}

bool GZipFile::onSupportsColor() { return false; }

bool GZipFile::isError() const
{
  int rv = 0;
  const char* e = gzerror(fp_, &rv);
  return (e != nullptr && *e != 0) || rv != 0;
}

bool GZipFile::isEOF() const { return gzeof(fp_); }

bool GZipFile::isOpen() const { return fp_; }

size_t GZipFile::onRead(std::span<std::byte> buf)
{
  char* data = reinterpret_cast<char*>(buf.data());
  size_t count = buf.size();
  size_t read = 0;
  while (count) {
    size_t len = std::min(
        count, static_cast<size_t>(std::numeric_limits<unsigned>::max()));
    int rv = gzread(fp_, data, len);
    if (rv <= 0) {
      break;
    }
    count -= rv;
    read += rv;
    data += rv;
  }
  return read;
}

size_t GZipFile::onWrite(std::span<const std::byte> buf)
{
  const char* data = reinterpret_cast<const char*>(buf.data());
  size_t count = buf.size();
  size_t written = 0;
  while (count) {
    size_t len = std::min(
        count, static_cast<size_t>(std::numeric_limits<unsigned>::max()));
    int rv = gzwrite(fp_, data, len);
    if (rv <= 0) {
      break;
    }
    count -= rv;
    written += rv;
    data += rv;
  }
  return written;
}

char* GZipFile::onGets(char* s, int size) { return gzgets(fp_, s, size); }

int GZipFile::onFlush() { return gzflush(fp_, 0); }

} // namespace aria2
