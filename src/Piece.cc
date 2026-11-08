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
#include "Piece.h"

#include <array>
#include <cassert>

#include "util.h"
#include "BitfieldMan.h"
#include "A2STR.h"
#include "util.h"
#include "a2functional.h"
#include "WrDiskCache.h"
#include "WrDiskCacheEntry.h"
#include "LogFactory.h"
#include "fmt.h"
#include "DiskAdaptor.h"
#include "MessageDigest.h"

namespace aria2 {

Piece::Piece() : index_(0), length_(0), nextBegin_(0), usedBySegment_(false) {}

Piece::Piece(size_t index, int64_t length, int32_t blockLength)
    : bitfield_(make_unique<BitfieldMan>(blockLength, length)),
      index_(index),
      length_(length),
      nextBegin_(0),
      usedBySegment_(false)
{
}

Piece::~Piece() = default;

void Piece::completeBlock(size_t blockIndex)
{
  bitfield_->setBit(blockIndex);
  bitfield_->unsetUseBit(blockIndex);
}

void Piece::clearAllBlock(WrDiskCache* diskCache)
{
  bitfield_->clearAllBit();
  bitfield_->clearAllUseBit();
  if (diskCache && wrCache_) {
    clearWrCache(diskCache);
  }
}

void Piece::setAllBlock() { bitfield_->setAllBit(); }

bool Piece::pieceComplete() const { return bitfield_->isAllBitSet(); }

size_t Piece::countBlock() const { return bitfield_->countBlock(); }

int32_t Piece::getBlockLength(size_t index) const
{
  return bitfield_->getBlockLength(index);
}

int32_t Piece::getBlockLength() const { return bitfield_->getBlockLength(); }

const unsigned char* Piece::getBitfield() const
{
  return bitfield_->getBitfield();
}

size_t Piece::getBitfieldLength() const
{
  return bitfield_->getBitfieldLength();
}

bool Piece::isBlockUsed(size_t index) const
{
  return bitfield_->isUseBitSet(index);
}

void Piece::cancelBlock(size_t blockIndex)
{
  bitfield_->unsetUseBit(blockIndex);
}

size_t Piece::countCompleteBlock() const
{
  return bitfield_->countBlock() - bitfield_->countMissingBlock();
}

size_t Piece::countMissingBlock() const
{
  return bitfield_->countMissingBlock();
}

bool Piece::hasBlock(size_t blockIndex) const
{
  return bitfield_->isBitSet(blockIndex);
}

bool Piece::getMissingUnusedBlockIndex(size_t& index) const
{
  auto indexOpt = bitfield_->getFirstMissingUnusedIndex();
  if (!indexOpt) {
    return false;
  }
  index = *indexOpt;
  bitfield_->setUseBit(index);
  return true;
}

size_t Piece::getMissingUnusedBlockIndex(std::vector<size_t>& indexes,
                                         size_t n) const
{
  size_t num = bitfield_->getFirstNMissingUnusedIndex(indexes, n);
  if (num) {
    for (std::vector<size_t>::const_iterator i = indexes.end() - num,
                                             eoi = indexes.end();
         i != eoi; ++i) {
      bitfield_->setUseBit(*i);
    }
  }
  return num;
}

bool Piece::getFirstMissingBlockIndexWithoutLock(size_t& index) const
{
  auto indexOpt = bitfield_->getFirstMissingIndex();
  if (!indexOpt) {
    return false;
  }
  index = *indexOpt;
  return true;
}

bool Piece::getAllMissingBlockIndexes(unsigned char* misbitfield,
                                      size_t mislen) const
{
  return bitfield_->getAllMissingIndexes(misbitfield, mislen);
}

std::string Piece::toString() const
{
  return fmt("piece: index={}, length={}",
             static_cast<unsigned long>(index_), length_);
}

void Piece::reconfigure(int64_t length)
{
  length_ = length;
  // TODO currently, this function is only called from
  // GrowSegment::updateWrittenLength().  If we use default block
  // length (16K), and length_ gets large (e.g., 4GB), creating
  // BitfieldMan for each call is very expensive.  Therefore, we use
  // maximum block length for now to reduce the overhead.  Ideally, we
  // check the code thoroughly and remove bitfield_ if we can.
  bitfield_ =
      make_unique<BitfieldMan>(std::numeric_limits<int32_t>::max(), length_);
}

void Piece::setBitfield(const unsigned char* bitfield, size_t len)
{
  bitfield_->setBitfield(bitfield, len);
}

int64_t Piece::getCompletedLength() { return bitfield_->getCompletedLength(); }

void Piece::setHashType(const std::string& hashType) { hashType_ = hashType; }

bool Piece::updateHash(int64_t begin, const unsigned char* data,
                       size_t dataLength)
{
  if (hashType_.empty()) {
    return false;
  }
  if (begin == nextBegin_ &&
      nextBegin_ + static_cast<int64_t>(dataLength) <= length_) {
    if (!mdctx_) {
      mdctx_ = MessageDigest::create(hashType_);
    }
    mdctx_->update(std::as_bytes(std::span{data, dataLength}));
    nextBegin_ += dataLength;
    return true;
  }
  else {
    return false;
  }
}

bool Piece::isHashCalculated() const { return mdctx_ && nextBegin_ == length_; }

std::string Piece::getDigest()
{
  if (!mdctx_) {
    return A2STR::NIL;
  }
  else {
    std::string hash = mdctx_->digest();
    destroyHashContext();
    return hash;
  }
}

namespace {
void updateHashWithRead(MessageDigest* mdctx,
                        const std::shared_ptr<DiskAdaptor>& adaptor,
                        int64_t offset, size_t len)
{
  std::array<std::byte, 4_k> buf;
  ldiv_t res = ldiv(len, buf.size());
  for (int j = 0; j < res.quot; ++j) {
    ssize_t nread = adaptor->readData(buf, offset);
    if (static_cast<size_t>(nread) != buf.size()) {
      throw DL_ABORT_EX(fmt(EX_FILE_READ, "n/a", "data is too short"));
    }
    mdctx->update(std::span{buf}.first(static_cast<size_t>(nread)));
    offset += nread;
  }
  if (res.rem) {
    ssize_t nread = adaptor->readData(std::span{buf}.first(res.rem), offset);
    if (nread != res.rem) {
      throw DL_ABORT_EX(fmt(EX_FILE_READ, "n/a", "data is too short"));
    }
    mdctx->update(std::span{buf}.first(static_cast<size_t>(nread)));
  }
}
} // namespace

std::string
Piece::getDigestWithWrCache(size_t pieceLength,
                            const std::shared_ptr<DiskAdaptor>& adaptor)
{
  auto mdctx = MessageDigest::create(hashType_);
  int64_t start = static_cast<int64_t>(index_) * pieceLength;
  int64_t goff = start;
  if (wrCache_) {
    const WrDiskCacheEntry::DataCellSet& dataSet = wrCache_->getDataSet();
    for (auto& d : dataSet) {
      if (goff < d->goff) {
        updateHashWithRead(mdctx.get(), adaptor, goff, d->goff - goff);
      }
      mdctx->update(std::as_bytes(std::span{d->data.get() + d->offset, d->len}));
      goff = d->goff + d->len;
    }
    updateHashWithRead(mdctx.get(), adaptor, goff, start + length_ - goff);
  }
  else {
    updateHashWithRead(mdctx.get(), adaptor, goff, length_);
  }
  return mdctx->digest();
}

void Piece::destroyHashContext()
{
  mdctx_.reset();
  nextBegin_ = 0;
}

bool Piece::usedBy(cuid_t cuid) const
{
  return std::ranges::find(users_, cuid) != users_.end();
}

void Piece::addUser(cuid_t cuid)
{
  if (std::ranges::find(users_, cuid) == users_.end()) {
    users_.push_back(cuid);
  }
}

void Piece::removeUser(cuid_t cuid)
{
  std::erase(users_, cuid);
}

void Piece::initWrCache(WrDiskCache* diskCache,
                        const std::shared_ptr<DiskAdaptor>& diskAdaptor)
{
  if (!diskCache) {
    return;
  }
  assert(!wrCache_);
  wrCache_ = make_unique<WrDiskCacheEntry>(diskAdaptor);
  bool rv = diskCache->add(wrCache_.get());
  assert(rv);
}

void Piece::flushWrCache(WrDiskCache* diskCache)
{
  if (!diskCache) {
    return;
  }
  assert(wrCache_);
  ssize_t size = static_cast<ssize_t>(wrCache_->getSize());
  diskCache->update(wrCache_.get(), -size);
  wrCache_->writeToDisk();
}

void Piece::clearWrCache(WrDiskCache* diskCache)
{
  if (!diskCache) {
    return;
  }
  assert(wrCache_);
  ssize_t size = static_cast<ssize_t>(wrCache_->getSize());
  diskCache->update(wrCache_.get(), -size);
  wrCache_->clear();
}

void Piece::updateWrCache(WrDiskCache* diskCache,
                          std::unique_ptr<unsigned char[]> data, size_t offset,
                          size_t len, size_t capacity, int64_t goff)
{
  if (!diskCache) {
    return;
  }
  assert(wrCache_);
  A2_LOG_DEBUG(fmt("updateWrCache entry={:p}",
                   static_cast<const void*>(wrCache_.get())));
  // shortcut: DataCellSet/DerefLess/cacheData all take raw DataCell*, so a
  // unique_ptr would ripple into WrDiskCacheEntry's public API and 6 tests.
  auto cell = new WrDiskCacheEntry::DataCell();
  cell->goff = goff;
  cell->data = std::move(data);
  cell->offset = offset;
  cell->len = len;
  cell->capacity = capacity;
  bool rv;
  rv = wrCache_->cacheData(cell);
  assert(rv);
  rv = diskCache->update(wrCache_.get(), len);
  assert(rv);
}

size_t Piece::appendWrCache(WrDiskCache* diskCache, int64_t goff,
                            std::span<const std::byte> data)
{
  if (!diskCache) {
    return 0;
  }
  assert(wrCache_);
  size_t delta = wrCache_->append(goff, data);
  bool rv;
  if (delta > 0) {
    rv = diskCache->update(wrCache_.get(), delta);
    assert(rv);
  }
  return delta;
}

void Piece::releaseWrCache(WrDiskCache* diskCache)
{
  if (diskCache && wrCache_) {
    diskCache->remove(wrCache_.get());
    wrCache_.reset();
  }
}

} // namespace aria2
