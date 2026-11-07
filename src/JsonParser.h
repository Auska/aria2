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
#ifndef D_JSON_PARSER_H
#define D_JSON_PARSER_H

#include "common.h"

#include <span>
#include <stack>

#include "StructParserStateMachine.h"

namespace aria2 {

namespace json {

enum class JsonError {
  UNEXPECTED_CHAR_BEFORE_VAL = -1,
  UNEXPECTED_CHAR_BEFORE_OBJ_KEY = -2,
  UNEXPECTED_CHAR_BEFORE_OBJ_VAL = -3,
  UNEXPECTED_CHAR_BEFORE_OBJ_SEP = -4,
  INVALID_UNICODE_POINT = -5,
  INVALID_NUMBER = -6,
  NUMBER_OUT_OF_RANGE = -7,
  UNEXPECTED_CHAR_BEFORE_ARRAY_SEP = -8,
  UNEXPECTED_LITERAL = -9,
  PREMATURE_DATA = -10,
  STRUCTURE_TOO_DEEP = -11
};

class JsonParser {
public:
  JsonParser(StructParserStateMachine* psm);
  ~JsonParser();
  // Parses |size| bytes of data |data| and returns the number of
  // bytes processed. On error, one of the negative error codes is
  // returned.
  ssize_t parseUpdate(std::span<const std::byte> data);
  // Parses |size| bytes of data |data| and returns the number of
  // bytes processed. On error, one of the negative error codes is
  // returned. Call this function to signal the parser that this is
  // the last piece of data. This function does NOT reset the internal
  // state.
  ssize_t parseFinal(std::span<const std::byte> data);
  // Resets the internal state of the parser and makes it ready for
  // reuse.
  void reset();

private:
  int pushState(int state);
  int stateTop() const;
  int popState();
  void runBeginCallback(StructElementType elementType);
  void runEndCallback(StructElementType elementType);
  void runCharactersCallback(std::span<const std::byte> data);
  void runNumberCallback(int64_t number, int frac, int exp);
  void runBoolCallback(bool bval);

  void onStringEnd();
  void onNumberEnd();
  void onObjectEnd();
  void onArrayEnd();
  void onValueEnd();
  void onBoolEnd();
  void onNullEnd();

  void consumeUnicode(char c);
  int consumeLowSurrogate(char c);

  StructParserStateMachine* psm_;
  std::stack<int> stateStack_;
  int currentState_;
  // Unicode codepoint
  uint16_t codepoint_;
  // For low-surrogate codepoint
  uint16_t codepoint2_;
  int numberSign_;
  int64_t number_;
  int frac_;
  int expSign_;
  int exp_;
  size_t numConsumed_;
  int lastError_;
};

} // namespace json

} // namespace aria2

#endif // D_JSON_PARSER_H
