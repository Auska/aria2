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
#ifndef D_GENERIC_PARSER_H
#define D_GENERIC_PARSER_H

#include <array>
#include <span>
#include "common.h"
#include "a2io.h"
#include "util.h"

namespace aria2 {

template <typename Parser, typename ParserStateMachine,
          bool allowEmptyName = false>
class GenericParser {
public:
  GenericParser() : parser_{&psm_}
  {
    psm_.setAllowEmptyMemberName(allowEmptyName);
  }

  ~GenericParser() noexcept = default;

  using ResultType = typename ParserStateMachine::ResultType;
  using ParserStateMachineType = ParserStateMachine;

  // Parses |data| and returns the number of bytes processed. On error,
  // one of the negative error codes is returned.
  ssize_t parseUpdate(std::span<const std::byte> data)
  {
    return parser_.parseUpdate(data);
  }

  // Parses |data| and returns the result. The |consumed| argument
  // receives the number of bytes processed (>= 0) on success or one of
  // the negative error codes on failure. This function also resets the
  // underlying parser and makes it ready to reuse.
  ResultType parseFinal(std::span<const std::byte> data, ssize_t& consumed)
  {
    ResultType res;
    consumed = parser_.parseFinal(data);
    if (consumed < 0) {
      res = ParserStateMachine::noResult();
    }
    else {
      res = psm_.getResult();
    }
    parser_.reset();
    return res;
  }

private:
  ParserStateMachine psm_;
  Parser parser_;
};

template <typename Parser>
typename Parser::ResultType parseFile(Parser& parser,
                                      const std::string& filename)
{
  int fd;
  // TODO Overrode a2open(const char*,..) and a2open(const std::wstring&,..)
  while ((fd = a2open(utf8ToWChar(filename).c_str(), O_BINARY | O_RDONLY,
                      OPEN_MODE)) == -1 &&
         errno == EINTR)
    ;
  if (fd == -1) {
    return Parser::ParserStateMachineType::noResult();
  }
  auto fdclose = defer(fd, close);
  std::array<char, 4_k> buf;
  ssize_t nread;
  ssize_t nproc;
  while ((nread = read(fd, buf.data(), buf.size())) > 0) {
    nproc = parser.parseUpdate(
        std::as_bytes(std::span{buf.data(), static_cast<size_t>(nread)}));
    if (nproc < 0) {
      break;
    }
  }
  return parser.parseFinal(std::span<const std::byte>{}, nproc);
}

} // namespace aria2

#endif // D_GENERIC_PARSER_H
