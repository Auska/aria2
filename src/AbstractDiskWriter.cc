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
#include "AbstractDiskWriter.h"

#include <unistd.h>
#ifdef HAVE_MMAP
#  include <sys/mman.h>
#endif // HAVE_MMAP
#include <fcntl.h>

#include <cerrno>
#include <cstring>
#include <cassert>

#include "File.h"
#include "util.h"
#include "message.h"
#include "DlAbortEx.h"
#include "a2io.h"
#include "fmt.h"
#include "DownloadFailureException.h"
#include "error_code.h"
#include "LogFactory.h"

namespace aria2 {

AbstractDiskWriter::AbstractDiskWriter(const std::string& filename)
    : filename_(filename),
      fd_(A2_BAD_FD),
      readOnly_(false),
      enableMmap_(false),
      mapaddr_(nullptr),
      maplen_(0)

{
}

AbstractDiskWriter::~AbstractDiskWriter() noexcept { closeFile(); }

namespace {
int fileError() { return errno; }
} // namespace

namespace {
std::string fileStrerror(int errNum) { return util::safeStrerror(errNum); }
} // namespace

void AbstractDiskWriter::openFile(int64_t totalLength)
{
  try {
    openExistingFile(totalLength);
  }
  catch (RecoverableException& e) {
    if (e.getErrNum() == ENOENT) {
      initAndOpenFile(totalLength);
    }
    else {
      throw;
    }
  }
}

void AbstractDiskWriter::closeFile()
{
#ifdef HAVE_MMAP
  if (mapaddr_) {
    int errNum = 0;
    if (munmap(mapaddr_, maplen_) == -1) {
      errNum = errno;
    }
    if (errNum != 0) {
      int errNum = fileError();
      A2_LOG_ERROR(
          fmt("Unmapping file {} failed: {}", filename_, fileStrerror(errNum)));
    }
    else {
      A2_LOG_INFO(fmt("Unmapping file {} succeeded", filename_));
    }
    mapaddr_ = nullptr;
    maplen_ = 0;
  }
#endif // HAVE_MMAP
  if (fd_ != A2_BAD_FD) {
    close(fd_);
    fd_ = A2_BAD_FD;
  }
}

namespace {
int openFileWithFlags(const std::string& filename, int flags,
                      error_code::Value errCode)
{
  int fd;
  while ((fd = a2open(filename.c_str(), flags, OPEN_MODE)) == -1 &&
         errno == EINTR)
    ;
  if (fd < 0) {
    int errNum = errno;
    throw DL_ABORT_EX3(errNum,
                       fmt(EX_FILE_OPEN, filename, util::safeStrerror(errNum)),
                       errCode);
  }
  util::make_fd_cloexec(fd);
  return fd;
}
} // namespace

void AbstractDiskWriter::openExistingFile(int64_t)
{
  int flags = O_BINARY;
  if (readOnly_) {
    flags |= O_RDONLY;
  }
  else {
    flags |= O_RDWR;
  }
  fd_ = openFileWithFlags(filename_, flags, error_code::Value::FILE_OPEN_ERROR);
}

void AbstractDiskWriter::createFile(int addFlags)
{
  assert(!filename_.empty());
  util::mkdirs(File(filename_).getDirname());
  fd_ = openFileWithFlags(filename_,
                          O_CREAT | O_RDWR | O_TRUNC | O_BINARY | addFlags,
                          error_code::Value::FILE_CREATE_ERROR);
}

ssize_t AbstractDiskWriter::writeDataInternal(std::span<const std::byte> data,
                                              int64_t offset)
{
  if (mapaddr_) {
    memcpy(mapaddr_ + offset, data.data(), data.size());
    return data.size();
  }
  else {
    ssize_t writtenLength = 0;
    seek(offset);
    while (static_cast<size_t>(writtenLength) < data.size()) {
      ssize_t ret = 0;
      while ((ret = write(fd_, data.data() + writtenLength,
                          data.size() - writtenLength)) == -1 &&
             errno == EINTR)
        ;
      if (ret == -1) {
        return -1;
      }
      writtenLength += ret;
    }
    return writtenLength;
  }
}

ssize_t AbstractDiskWriter::readDataInternal(std::span<std::byte> data,
                                             int64_t offset)
{
  if (mapaddr_) {
    if (offset >= maplen_) {
      return 0;
    }
    auto readlen = std::min<std::int64_t>(maplen_ - offset, data.size());
    memcpy(data.data(), mapaddr_ + offset, readlen);
    return readlen;
  }
  else {
    seek(offset);
    ssize_t ret = 0;
    while ((ret = read(fd_, data.data(), data.size())) == -1 && errno == EINTR)
      ;
    return ret;
  }
}

void AbstractDiskWriter::seek(int64_t offset)
{
  assert(offset >= 0);
  if (a2lseek(fd_, offset, SEEK_SET) == (a2_off_t)-1) {
    int errNum = fileError();
    throw DL_ABORT_EX2(fmt(EX_FILE_SEEK, filename_, fileStrerror(errNum)),
                       error_code::Value::FILE_IO_ERROR);
  }
}

void AbstractDiskWriter::ensureMmapWrite(size_t len, int64_t offset)
{
#ifdef HAVE_MMAP
  if (enableMmap_) {
    if (mapaddr_) {
      if (static_cast<int64_t>(len + offset) > maplen_) {
        int errNum = 0;
        if (munmap(mapaddr_, maplen_) == -1) {
          errNum = errno;
        }
        if (errNum != 0) {
          A2_LOG_ERROR(fmt("Unmapping file {} failed: {}", filename_,
                           fileStrerror(errNum)));
        }
        mapaddr_ = nullptr;
        maplen_ = 0;
        enableMmap_ = false;
      }
    }
    else {
      int64_t filesize = size();

      if (filesize == 0) {
        // mapping 0 length file is useless.  Also munmap with size ==
        // 0 will fail with EINVAL.
        enableMmap_ = false;
        return;
      }

      if (static_cast<uint64_t>(std::numeric_limits<size_t>::max()) <
          static_cast<uint64_t>(filesize)) {
        // filesize could overflow in 32bit OS with 64bit off_t type
        // the filesize will be truncated if provided as a 32bit size_t
        enableMmap_ = false;
        return;
      }

      int errNum = 0;
      if (static_cast<int64_t>(len + offset) <= filesize) {
        auto pa =
            mmap(nullptr, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

        if (pa == MAP_FAILED) {
          errNum = errno;
        }
        else {
          mapaddr_ = reinterpret_cast<unsigned char*>(pa);
        }
        if (mapaddr_) {
          A2_LOG_DEBUG(fmt("Mapping file {} succeeded, length={}", filename_,
                           static_cast<uint64_t>(filesize)));
          maplen_ = filesize;
        }
        else {
          A2_LOG_WARN(fmt("Mapping file {} failed: {}", filename_,
                          fileStrerror(errNum)));
          enableMmap_ = false;
        }
      }
    }
  }
#endif // HAVE_MMAP
}

namespace {
// Returns true if |errNum| indicates that disk is full.
bool isDiskFullError(int errNum) { return errNum == ENOSPC; }
} // namespace

void AbstractDiskWriter::writeData(std::span<const std::byte> data,
                                   int64_t offset)
{
  ensureMmapWrite(data.size(), offset);
  if (writeDataInternal(data, offset) < 0) {
    int errNum = fileError();
    // If the error indicates disk full situation, throw
    // DownloadFailureException and abort download instantly.
    if (isDiskFullError(errNum)) {
      throw DOWNLOAD_FAILURE_EXCEPTION3(
          errNum, fmt(EX_FILE_WRITE, filename_, fileStrerror(errNum)),
          error_code::Value::NOT_ENOUGH_DISK_SPACE);
    }
    else {
      throw DL_ABORT_EX3(errNum,
                         fmt(EX_FILE_WRITE, filename_, fileStrerror(errNum)),
                         error_code::Value::FILE_IO_ERROR);
    }
  }
}

size_t AbstractDiskWriter::readData(std::span<std::byte> data, int64_t offset)
{
  ssize_t ret;
  if ((ret = readDataInternal(data, offset)) < 0) {
    int errNum = fileError();
    throw DL_ABORT_EX3(errNum,
                       fmt(EX_FILE_READ, filename_, fileStrerror(errNum)),
                       error_code::Value::FILE_IO_ERROR);
  }
  return ret;
}

void AbstractDiskWriter::truncate(int64_t length)
{
  if (fd_ == A2_BAD_FD) {
    throw DL_ABORT_EX("File not yet opened.");
  }
  if (a2ftruncate(fd_, length) == -1) {
    int errNum = fileError();
    throw DL_ABORT_EX2(
        fmt("File truncation failed. cause: {}", fileStrerror(errNum)),
        error_code::Value::FILE_IO_ERROR);
  }
}

void AbstractDiskWriter::allocate(int64_t offset, int64_t length, bool sparse)
{
  if (fd_ == A2_BAD_FD) {
    throw DL_ABORT_EX("File not yet opened.");
  }
  if (sparse) {
    truncate(offset + length);
    return;
  }
#ifdef HAVE_SOME_FALLOCATE
#  if HAVE_FALLOCATE
  // For linux, we use fallocate to detect file system supports
  // fallocate or not.
  int r;
  while ((r = fallocate(fd_, 0, offset, length)) == -1 && errno == EINTR)
    ;
  int errNum = errno;
  if (r == -1) {
    throw DL_ABORT_EX3(
        errNum, fmt("fallocate failed. cause: {}", util::safeStrerror(errNum)),
        isDiskFullError(errNum) ? error_code::Value::NOT_ENOUGH_DISK_SPACE
                                : error_code::Value::FILE_IO_ERROR);
  }
#  elif HAVE_POSIX_FALLOCATE
  int r = posix_fallocate(fd_, offset, length);
  if (r != 0) {
    throw DL_ABORT_EX3(
        r, fmt("posix_fallocate failed. cause: {}", util::safeStrerror(r)),
        isDiskFullError(r) ? error_code::Value::NOT_ENOUGH_DISK_SPACE
                           : error_code::Value::FILE_IO_ERROR);
  }
#  else
#    error "no *_fallocate function available."
#  endif
#endif // HAVE_SOME_FALLOCATE
}

int64_t AbstractDiskWriter::size() { return File(filename_).size(); }

void AbstractDiskWriter::enableReadOnly() { readOnly_ = true; }

void AbstractDiskWriter::disableReadOnly() { readOnly_ = false; }

void AbstractDiskWriter::enableMmap() { enableMmap_ = true; }

void AbstractDiskWriter::dropCache(int64_t len, int64_t offset)
{
#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fd_, offset, len, POSIX_FADV_DONTNEED);
#endif // HAVE_POSIX_FADVISE
}

void AbstractDiskWriter::flushOSBuffers()
{
  if (fd_ == A2_BAD_FD) {
    return;
  }
  fsync(fd_);
}

} // namespace aria2
