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
#include "BencodeParser.h"

#include <cassert>

#include "StructParserStateMachine.h"
#include "util.h"

namespace aria2 {

namespace bittorrent {

namespace {
enum {
  BENCODE_FINISH,
  BENCODE_ERROR,
  BENCODE_INITIAL,
  BENCODE_VALUE,
  BENCODE_DICT_KEY,
  BENCODE_DICT_VAL,
  BENCODE_LIST,
  BENCODE_STRING_LEN,
  BENCODE_STRING,
  BENCODE_NUMBER_SIGN,
  BENCODE_NUMBER,
  BENCODE_FLOAT_NUMBER_IGNORE,
};
} // namespace

BencodeParser::BencodeParser(StructParserStateMachine* psm)
    : psm_(psm),
      currentState_(BENCODE_INITIAL),
      strLength_(0),
      numberSign_(1),
      number_(0),
      numConsumed_(0),
      lastError_(0)
{
  stateStack_.push(BENCODE_FINISH);
}

BencodeParser::~BencodeParser() = default;

ssize_t BencodeParser::parseUpdate(std::span<const std::byte> data)
{
  const char* d = reinterpret_cast<const char*>(data.data());
  size_t size = data.size();
  size_t i;
  if (currentState_ == BENCODE_FINISH) {
    return 0;
  }
  else if (currentState_ == BENCODE_ERROR) {
    return lastError_;
  }
  for (i = 0; i < size && currentState_ != BENCODE_FINISH; ++i) {
    char c = d[i];
    switch (currentState_) {
    case BENCODE_LIST:
      if (c == 'e') {
        onListEnd();
        break;
      }
      else {
        int rv = pushState(currentState_);
        if (rv < 0) {
          return rv;
        }
        currentState_ = BENCODE_VALUE;
        runBeginCallback(StructElementType::ARRAY_DATA);
      }
    // Fall through
    case BENCODE_INITIAL:
    case BENCODE_VALUE:
      switch (c) {
      case 'd': {
        currentState_ = BENCODE_DICT_KEY;
        runBeginCallback(StructElementType::DICT);
        break;
      }
      case 'l':
        currentState_ = BENCODE_LIST;
        runBeginCallback(StructElementType::ARRAY);
        break;
      case 'i':
        number_ = 0;
        numberSign_ = 1;
        numConsumed_ = 0;
        currentState_ = BENCODE_NUMBER_SIGN;
        runBeginCallback(StructElementType::NUMBER);
        break;
      default:
        if (util::isDigit(c)) {
          strLength_ = c - '0';
          numConsumed_ = 1;
          currentState_ = BENCODE_STRING_LEN;
          runBeginCallback(StructElementType::STRING);
          break;
        }
        else {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::UNEXPECTED_CHAR_BEFORE_VAL);
        }
      }
      break;
    case BENCODE_DICT_KEY: {
      if (c == 'e') {
        onDictEnd();
        break;
      }
      int rv = pushState(currentState_);
      if (rv < 0) {
        return rv;
      }
      strLength_ = 0;
      numConsumed_ = 0;
      runBeginCallback(StructElementType::DICT_KEY);
      currentState_ = BENCODE_STRING_LEN;
      // Fall through
    }
    case BENCODE_STRING_LEN: {
      size_t j;
      for (j = i; j < size && in(d[j], '0', '9'); ++j) {
        if ((INT64_MAX - (d[j] - '0')) / 10 < strLength_) {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::STRING_LENGTH_OUT_OF_RANGE);
        }
        strLength_ *= 10;
        strLength_ += d[j] - '0';
      }
      numConsumed_ += j - i;
      if (j != size) {
        if (d[j] != ':' || numConsumed_ == 0) {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::INVALID_STRING_LENGTH);
        }
        i = j;
        currentState_ = BENCODE_STRING;
        if (strLength_ == 0) {
          runCharactersCallback(std::span<const std::byte>{});
          onStringEnd();
        }
      }
      else {
        i = j - 1;
      }
      break;
    }
    case BENCODE_STRING: {
      size_t nread = std::min(static_cast<int64_t>(size - i), strLength_);
      runCharactersCallback(data.subspan(i, nread));
      strLength_ -= nread;
      i += nread - 1;
      if (strLength_ == 0) {
        onStringEnd();
      }
      break;
    }
    case BENCODE_NUMBER_SIGN: {
      switch (c) {
      case '+':
        numberSign_ = 1;
        currentState_ = BENCODE_NUMBER;
        break;
      case '-':
        numberSign_ = -1;
        currentState_ = BENCODE_NUMBER;
        break;
      default:
        if (util::isDigit(c)) {
          number_ = c - '0';
          numConsumed_ = 1;
          currentState_ = BENCODE_NUMBER;
        }
      }
      break;
    }
    case BENCODE_NUMBER: {
      size_t j;
      for (j = i; j < size && in(d[j], '0', '9'); ++j) {
        if ((INT64_MAX - (d[j] - '0')) / 10 < number_) {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::NUMBER_OUT_OF_RANGE);
        }
        number_ *= 10;
        number_ += d[j] - '0';
      }
      numConsumed_ += j - i;
      if (j != size) {
        if (numConsumed_ == 0) {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::INVALID_NUMBER);
        }

        auto c = d[j];
        if (util::isDigit(c) || c == '.' || c == 'E' || c == '+' || c == '-') {
          // some torrent generator adds floating point number in
          // scientific notation (e.g., -1.134E+3) in integer field.
          // In this case, just skip these bytes until we find 'e'.
          number_ = 0;
          numConsumed_ = 0;
          currentState_ = BENCODE_FLOAT_NUMBER_IGNORE;

          i = j;

          break;
        }

        if (c != 'e') {
          currentState_ = BENCODE_ERROR;
          return lastError_ = static_cast<int>(BencodeError::INVALID_NUMBER);
        }

        i = j;

        onNumberEnd();
      }
      else {
        i = j - 1;
      }
      break;
    }
    case BENCODE_FLOAT_NUMBER_IGNORE: {
      auto c = d[i];
      if (util::isDigit(c) || c == '.' || c == 'E' || c == '+' || c == '-') {
        continue;
      }

      if (c != 'e') {
        currentState_ = BENCODE_ERROR;
        return lastError_ = static_cast<int>(BencodeError::INVALID_FLOAT_NUMBER);
      }

      onNumberEnd();
      break;
    }
    }
  }
  return i;
}

ssize_t BencodeParser::parseFinal(std::span<const std::byte> data)
{
  ssize_t rv;
  rv = parseUpdate(data);
  if (rv >= 0) {
    if (currentState_ != BENCODE_FINISH && currentState_ != BENCODE_INITIAL) {
      rv = static_cast<int>(BencodeError::PREMATURE_DATA);
    }
  }
  return rv;
}

void BencodeParser::reset()
{
  psm_->reset();
  currentState_ = BENCODE_INITIAL;
  lastError_ = 0;
  while (!stateStack_.empty()) {
    stateStack_.pop();
  }
  stateStack_.push(BENCODE_FINISH);
}

void BencodeParser::onStringEnd()
{
  runEndCallback(stateTop() == BENCODE_DICT_KEY ? StructElementType::DICT_KEY
                                                : StructElementType::STRING);
  onValueEnd();
}

void BencodeParser::onNumberEnd()
{
  runNumberCallback(numberSign_ * number_);
  runEndCallback(StructElementType::NUMBER);
  onValueEnd();
}

void BencodeParser::onDictEnd()
{
  runEndCallback(StructElementType::DICT);
  onValueEnd();
}

void BencodeParser::onListEnd()
{
  runEndCallback(StructElementType::ARRAY);
  onValueEnd();
}

void BencodeParser::onValueEnd()
{
  switch (stateTop()) {
  case BENCODE_DICT_KEY:
    popState();
    pushState(BENCODE_DICT_VAL);
    currentState_ = BENCODE_VALUE;
    runBeginCallback(StructElementType::DICT_DATA);
    break;
  case BENCODE_DICT_VAL:
    runEndCallback(StructElementType::DICT_DATA);
    popState();
    currentState_ = BENCODE_DICT_KEY;
    break;
  case BENCODE_LIST:
    runEndCallback(StructElementType::ARRAY_DATA);
    popState();
    currentState_ = BENCODE_LIST;
    break;
  default:
    assert(stateTop() == BENCODE_FINISH);
    currentState_ = stateTop();
    break;
  }
}

int BencodeParser::pushState(int state)
{
  if (stateStack_.size() >= 50) {
    return static_cast<int>(BencodeError::STRUCTURE_TOO_DEEP);
  }
  else {
    stateStack_.push(state);
    return 0;
  }
}

int BencodeParser::stateTop() const { return stateStack_.top(); }

int BencodeParser::popState()
{
  int state = stateStack_.top();
  stateStack_.pop();
  return state;
}

void BencodeParser::runBeginCallback(StructElementType elementType)
{
  // switch(elementType) {
  // case StructElementType::DICT:
  //   std::cout << "object start" << std::endl;
  //   break;
  // case StructElementType::DICT_KEY:
  //   std::cout << "object key start" << std::endl;
  //   break;
  // case StructElementType::DICT_DATA:
  //   std::cout << "object data start" << std::endl;
  //   break;
  // case StructElementType::ARRAY:
  //   std::cout << "array start" << std::endl;
  //   break;
  // case StructElementType::ARRAY_DATA:
  //   std::cout << "array data start" << std::endl;
  //   break;
  // case StructElementType::STRING:
  //   std::cout << "string start" << std::endl;
  //   break;
  // case StructElementType::NUMBER:
  //   std::cout << "number start" << std::endl;
  //   break;
  // case StructElementType::BOOL:
  //   std::cout << "bool start" << std::endl;
  //   break;
  // case StructElementType::NULL_VALUE:
  //   std::cout << "null start" << std::endl;
  //   break;
  // default:
  //   break;
  // };
  psm_->beginElement(elementType);
}

void BencodeParser::runEndCallback(StructElementType elementType)
{
  // switch(elementType) {
  // case StructElementType::DICT:
  //   std::cout << "object end" << std::endl;
  //   break;
  // case StructElementType::DICT_KEY:
  //   std::cout << "object key end" << std::endl;
  //   break;
  // case StructElementType::DICT_DATA:
  //   std::cout << "object data end" << std::endl;
  //   break;
  // case StructElementType::ARRAY:
  //   std::cout << "array end" << std::endl;
  //   break;
  // case StructElementType::ARRAY_DATA:
  //   std::cout << "array data end" << std::endl;
  //   break;
  // case StructElementType::STRING:
  //   std::cout << "string end" << std::endl;
  //   break;
  // case StructElementType::NUMBER:
  //   std::cout << "number end" << std::endl;
  //   break;
  // case StructElementType::BOOL:
  //   std::cout << "bool end" << std::endl;
  //   break;
  // case StructElementType::NULL_VALUE:
  //   std::cout << "null end" << std::endl;
  //   break;
  // default:
  //   break;
  // };
  psm_->endElement(elementType);
}

void BencodeParser::runCharactersCallback(std::span<const std::byte> data)
{
  psm_->charactersCallback(data);
}

void BencodeParser::runNumberCallback(int64_t number)
{
  psm_->numberCallback(number, 0, 0);
}

} // namespace bittorrent

} // namespace aria2
