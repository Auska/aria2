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

#include "MessageDigestImpl.h"

#include "crypto_hash.h"

#include "Adler32MessageDigestImpl.h"

namespace {
using namespace aria2;
using namespace crypto;

template <hash::Algorithms algo>
class MessageDigestBase : public MessageDigestImpl {
public:
  MessageDigestBase() : ctx_{hash::create(algo)} {}
  virtual ~MessageDigestBase() = default;

  static size_t length()
  {
    auto ctx = hash::create(algo);
    return ctx->length();
  }

  virtual size_t getDigestLength() const override
  {
    return ctx_->length();
  }

  virtual void reset() override { ctx_->reset(); }

  virtual void update(std::span<const std::byte> data) override
  {
    ctx_->update(data);
  }

  virtual void digest(unsigned char* md) override
  {
    auto rv = ctx_->finalize();
    memcpy(md, rv.data(), rv.length());
  }

private:
  std::unique_ptr<hash::Algorithm> ctx_;
};

using MessageDigestMD5 = MessageDigestBase<hash::Algorithms::MD5>;
using MessageDigestSHA1 = MessageDigestBase<hash::Algorithms::SHA1>;
using MessageDigestSHA224 = MessageDigestBase<hash::Algorithms::SHA224>;
using MessageDigestSHA256 = MessageDigestBase<hash::Algorithms::SHA256>;
using MessageDigestSHA384 = MessageDigestBase<hash::Algorithms::SHA384>;
using MessageDigestSHA512 = MessageDigestBase<hash::Algorithms::SHA512>;

} // namespace

namespace aria2 {

std::unique_ptr<MessageDigestImpl> MessageDigestImpl::sha1()
{
  return make_unique<MessageDigestSHA1>();
}

MessageDigestImpl::hashes_t MessageDigestImpl::hashes = {
    {"sha-1", make_hi<MessageDigestSHA1>()},
    {"sha-224", make_hi<MessageDigestSHA224>()},
    {"sha-256", make_hi<MessageDigestSHA256>()},
    {"sha-384", make_hi<MessageDigestSHA384>()},
    {"sha-512", make_hi<MessageDigestSHA512>()},
    {"md5", make_hi<MessageDigestMD5>()},
    ADLER32_MESSAGE_DIGEST};

} // namespace aria2
