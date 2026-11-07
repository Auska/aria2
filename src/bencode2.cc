/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2010 Tatsuhiro Tsujikawa
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
#include "bencode2.h"

#include <span>
#include <string>

#include "fmt.h"
#include "DlAbortEx.h"
#include "error_code.h"
#include "BufferedFile.h"
#include "util.h"
#include "ValueBaseBencodeParser.h"

namespace aria2 {

namespace bencode2 {

std::unique_ptr<ValueBase> decode(std::span<const std::byte> data)
{
  size_t end;
  return decode(data, end);
}

std::unique_ptr<ValueBase> decode(const std::string& data)
{
  size_t end;
  return decode(std::as_bytes(std::span{data.data(), data.size()}), end);
}

std::unique_ptr<ValueBase> decode(std::span<const std::byte> data, size_t& end)
{
  ssize_t error;
  bittorrent::ValueBaseBencodeParser parser;
  // The parser chain still takes a raw pointer and a length; they stay out
  // of this refactor's scope.
  auto res = parser.parseFinal(data, error);
  if (error < 0) {
    throw DL_ABORT_EX2(
        fmt("Bencode decoding failed: error={}", static_cast<int>(error)),
        error_code::BENCODE_PARSE_ERROR);
  }
  end = error;
  return res;
}

std::string encode(const ValueBase* vlb)
{
  class BencodeValueBaseVisitor : public ValueBaseVisitor {
  private:
    std::string out_;

  public:
    virtual void visit(const String& string) override
    {
      const std::string& s = string.s();
      out_ += std::to_string(s.size());
      out_ += ':';
      out_ += s;
    }

    virtual void visit(const Integer& integer) override
    {
      out_ += 'i';
      out_ += std::to_string(integer.i());
      out_ += 'e';
    }

    virtual void visit(const Bool& v) override {}
    virtual void visit(const Null& v) override {}

    virtual void visit(const List& list) override
    {
      out_ += 'l';
      for (const auto& e : list) {
        e->accept(*this);
      }
      out_ += 'e';
    }

    virtual void visit(const Dict& dict) override
    {
      out_ += 'd';
      for (const auto& e : dict) {
        auto& key = e.first;
        out_ += std::to_string(key.size());
        out_ += ':';
        out_ += key;
        e.second->accept(*this);
      }
      out_ += 'e';
    }

    std::string getResult() const { return out_; }
  };
  BencodeValueBaseVisitor visitor;
  vlb->accept(visitor);
  return visitor.getResult();
}

} // namespace bencode2

} // namespace aria2
