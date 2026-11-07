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
#include "JsonParser.h"

#include <cassert>

#include "StructParserStateMachine.h"
#include "util.h"

namespace aria2 {

namespace json {

namespace {
enum {
  JSON_FINISH,
  JSON_ERROR,
  JSON_VALUE,
  JSON_OBJECT_KEY,
  JSON_OBJECT_VAL,
  JSON_OBJECT_SEP,
  JSON_ARRAY,
  JSON_ARRAY_SEP,
  JSON_STRING,
  JSON_STRING_ESCAPE,
  JSON_STRING_UNICODE,
  JSON_STRING_LOW_SURROGATE_ESCAPE,
  JSON_STRING_LOW_SURROGATE_U,
  JSON_STRING_LOW_SURROGATE,
  JSON_NUMBER,
  JSON_NUMBER_FRAC,
  JSON_NUMBER_EXP_SIGN,
  JSON_NUMBER_EXP,
  JSON_TRUE,
  JSON_FALSE,
  JSON_NULL
};
} // namespace

namespace {
const char JSON_TRUE_STR[] = "true";
const char JSON_FALSE_STR[] = "false";
const char JSON_NULL_STR[] = "null";
} // namespace

JsonParser::JsonParser(StructParserStateMachine* psm)
    : psm_(psm),
      currentState_(JSON_VALUE),
      codepoint_(0),
      codepoint2_(0),
      numberSign_(1),
      number_(0),
      frac_(0),
      expSign_(1),
      exp_(0),
      numConsumed_(0),
      lastError_(0)
{
  stateStack_.push(JSON_FINISH);
}

JsonParser::~JsonParser() = default;

namespace {
bool isSpace(char c) { return util::isLws(c) || util::isCRLF(c); }
} // namespace

ssize_t JsonParser::parseUpdate(std::span<const std::byte> dataIn)
{
  const char* data = reinterpret_cast<const char*>(dataIn.data());
  size_t size = dataIn.size();
  size_t i;
  if (currentState_ == JSON_FINISH) {
    return 0;
  }
  else if (currentState_ == JSON_ERROR) {
    return lastError_;
  }
  for (i = 0; i < size && currentState_ != JSON_FINISH; ++i) {
    char c = data[i];
    switch (currentState_) {
    case JSON_ARRAY:
      if (c == ']') {
        onArrayEnd();
        break;
      }
      else if (isSpace(c)) {
        break;
      }
      else {
        int rv = pushState(currentState_);
        if (rv < 0) {
          return rv;
        }
        currentState_ = JSON_VALUE;
        runBeginCallback(StructElementType::ARRAY_DATA);
      }
    // Fall through
    case JSON_VALUE:
      switch (c) {
      case '{':
        currentState_ = JSON_OBJECT_KEY;
        runBeginCallback(StructElementType::DICT);
        break;
      case '[':
        currentState_ = JSON_ARRAY;
        runBeginCallback(StructElementType::ARRAY);
        break;
      case '"':
        currentState_ = JSON_STRING;
        runBeginCallback(StructElementType::STRING);
        break;
      case '-':
        number_ = 0;
        numberSign_ = -1;
        numConsumed_ = 0;
        currentState_ = JSON_NUMBER;
        runBeginCallback(StructElementType::NUMBER);
        break;
      case 't':
        numConsumed_ = 1;
        currentState_ = JSON_TRUE;
        runBeginCallback(StructElementType::BOOL);
        break;
      case 'f':
        numConsumed_ = 1;
        currentState_ = JSON_FALSE;
        runBeginCallback(StructElementType::BOOL);
        break;
      case 'n':
        numConsumed_ = 1;
        currentState_ = JSON_NULL;
        runBeginCallback(StructElementType::NULL_VALUE);
        break;
      default:
        if (util::isDigit(c)) {
          number_ = c - '0';
          numberSign_ = 1;
          numConsumed_ = 1;
          currentState_ = JSON_NUMBER;
          runBeginCallback(StructElementType::NUMBER);
        }
        else if (!isSpace(c)) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::UNEXPECTED_CHAR_BEFORE_VAL);
        }
      }
      break;
    case JSON_TRUE:
      if (JSON_TRUE_STR[numConsumed_] == c) {
        ++numConsumed_;
        if (numConsumed_ == sizeof(JSON_TRUE_STR) - 1) {
          runBoolCallback(true);
          onBoolEnd();
        }
      }
      else {
        currentState_ = JSON_ERROR;
        return lastError_ = static_cast<int>(JsonError::UNEXPECTED_LITERAL);
      }
      break;
    case JSON_FALSE:
      if (JSON_FALSE_STR[numConsumed_] == c) {
        ++numConsumed_;
        if (numConsumed_ == sizeof(JSON_FALSE_STR) - 1) {
          runBoolCallback(false);
          onBoolEnd();
        }
      }
      else {
        currentState_ = JSON_ERROR;
        return lastError_ = static_cast<int>(JsonError::UNEXPECTED_LITERAL);
      }
      break;
    case JSON_NULL:
      if (JSON_NULL_STR[numConsumed_] == c) {
        ++numConsumed_;
        if (numConsumed_ == sizeof(JSON_NULL_STR) - 1) {
          onNullEnd();
        }
      }
      else {
        currentState_ = JSON_ERROR;
        return lastError_ = static_cast<int>(JsonError::UNEXPECTED_LITERAL);
      }
      break;
    case JSON_OBJECT_KEY:
      switch (c) {
      case '"': {
        int rv = pushState(currentState_);
        if (rv < 0) {
          return rv;
        }
        currentState_ = JSON_STRING;
        runBeginCallback(StructElementType::DICT_KEY);
        break;
      }
      case '}':
        onObjectEnd();
        break;
      default:
        if (!isSpace(c)) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::UNEXPECTED_CHAR_BEFORE_OBJ_KEY);
        }
      }
      break;

    case JSON_OBJECT_VAL:
      switch (c) {
      case ':':
        pushState(currentState_);
        currentState_ = JSON_VALUE;
        runBeginCallback(StructElementType::DICT_DATA);
        break;
      default:
        if (!isSpace(c)) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::UNEXPECTED_CHAR_BEFORE_OBJ_VAL);
        }
      }
      break;
    case JSON_OBJECT_SEP:
      switch (c) {
      case ',':
        currentState_ = JSON_OBJECT_KEY;
        break;
      case '}':
        onObjectEnd();
        break;
      default:
        if (!isSpace(c)) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::UNEXPECTED_CHAR_BEFORE_OBJ_SEP);
        }
      }
      break;
    case JSON_ARRAY_SEP:
      switch (c) {
      case ',':
        pushState(JSON_ARRAY);
        currentState_ = JSON_VALUE;
        runBeginCallback(StructElementType::ARRAY_DATA);
        break;
      case ']':
        onArrayEnd();
        break;
      default:
        if (!isSpace(c)) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::UNEXPECTED_CHAR_BEFORE_ARRAY_SEP);
        }
      }
      break;
    case JSON_STRING:
      switch (c) {
      case '"':
        onStringEnd();
        break;
      case '\\':
        currentState_ = JSON_STRING_ESCAPE;
        break;
      default: {
        size_t j;
        for (j = i; j < size && data[j] != '\\' && data[j] != '"'; ++j)
          ;
        if (j - i >= 1) {
          runCharactersCallback(dataIn.subspan(i, j - i));
        }
        i = j - 1;
        break;
      }
      }
      break;
    case JSON_STRING_ESCAPE:
      switch (c) {
      case 'u':
        codepoint_ = 0;
        numConsumed_ = 0;
        currentState_ = JSON_STRING_UNICODE;
        break;
      default:
        switch (c) {
        case 'b':
          runCharactersCallback(
              std::as_bytes(std::span{"\b", 1}));
          break;
        case 'f':
          runCharactersCallback(
              std::as_bytes(std::span{"\f", 1}));
          break;
        case 'n':
          runCharactersCallback(
              std::as_bytes(std::span{"\n", 1}));
          break;
        case 'r':
          runCharactersCallback(
              std::as_bytes(std::span{"\r", 1}));
          break;
        case 't':
          runCharactersCallback(
              std::as_bytes(std::span{"\t", 1}));
          break;
        default: {
          char temp[1];
          temp[0] = c;
          runCharactersCallback(std::as_bytes(std::span{temp, 1}));
          break;
        }
        }
        currentState_ = JSON_STRING;
      }
      break;
    case JSON_STRING_UNICODE: {
      size_t j;
      for (j = i; j < size && currentState_ == JSON_STRING_UNICODE; ++j) {
        if (util::isHexDigit(data[j])) {
          consumeUnicode(data[j]);
        }
        else {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::INVALID_UNICODE_POINT);
        }
      }
      i = j - 1;
      break;
    }
    case JSON_STRING_LOW_SURROGATE_ESCAPE:
      switch (c) {
      case '\\':
        currentState_ = JSON_STRING_LOW_SURROGATE_U;
        break;
      default:
        currentState_ = JSON_ERROR;
        return lastError_ = static_cast<int>(JsonError::INVALID_UNICODE_POINT);
      }
      break;
    case JSON_STRING_LOW_SURROGATE_U:
      switch (c) {
      case 'u':
        codepoint2_ = 0;
        numConsumed_ = 0;
        currentState_ = JSON_STRING_LOW_SURROGATE;
        break;
      default:
        currentState_ = JSON_ERROR;
        return lastError_ = static_cast<int>(JsonError::INVALID_UNICODE_POINT);
      }
      break;
    case JSON_STRING_LOW_SURROGATE: {
      size_t j;
      for (j = i; j < size && currentState_ == JSON_STRING_LOW_SURROGATE; ++j) {
        if (util::isHexDigit(data[j])) {
          int rv = consumeLowSurrogate(data[j]);
          if (rv != 0) {
            currentState_ = JSON_ERROR;
            lastError_ = rv;
            return rv;
          }
        }
        else {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::INVALID_UNICODE_POINT);
        }
      }
      i = j - 1;
      break;
    }
    case JSON_NUMBER: {
      size_t j;
      for (j = i; j < size && in(data[j], '0', '9'); ++j) {
        if ((INT64_MAX - (data[j] - '0')) / 10 < number_) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::NUMBER_OUT_OF_RANGE);
        }
        number_ *= 10;
        number_ += data[j] - '0';
      }
      numConsumed_ += j - i;
      if (j != size) {
        if (numConsumed_ == 0) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::INVALID_NUMBER);
        }
        switch (data[j]) {
        case '.':
          frac_ = 0;
          numConsumed_ = 0;
          currentState_ = JSON_NUMBER_FRAC;
          i = j;
          break;
        case 'e':
        case 'E':
          expSign_ = 1;
          exp_ = 0;
          numConsumed_ = 0;
          currentState_ = JSON_NUMBER_EXP_SIGN;
          i = j;
          break;
        default:
          onNumberEnd();
          i = j - 1;
        }
      }
      else {
        i = j - 1;
      }
      break;
    }
    case JSON_NUMBER_FRAC: {
      size_t j;
      for (j = i; j < size && in(data[j], '0', '9'); ++j) {
        // Take into account at most 8 digits
        if (frac_ < 100000000) {
          frac_ *= 10;
          frac_ += data[j] - '0';
        }
      }
      numConsumed_ += j - i;
      if (j != size) {
        if (numConsumed_ == 0) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::INVALID_NUMBER);
        }
        switch (data[j]) {
        case 'e':
        case 'E':
          exp_ = 0;
          numConsumed_ = 0;
          currentState_ = JSON_NUMBER_EXP_SIGN;
          i = j;
          break;
        default:
          i = j - 1;
          onNumberEnd();
        }
      }
      else {
        i = j - 1;
      }
      break;
    }
    case JSON_NUMBER_EXP_SIGN:
      switch (c) {
      case '+':
        currentState_ = JSON_NUMBER_EXP;
        break;
      case '-':
        expSign_ = -1;
        currentState_ = JSON_NUMBER_EXP;
        break;
      default:
        break;
      }
      if (currentState_ == JSON_NUMBER_EXP) {
        break;
      }
      else {
        currentState_ = JSON_NUMBER_EXP;
        // Fall through
      }
    case JSON_NUMBER_EXP: {
      size_t j;
      for (j = i; j < size && in(data[j], '0', '9'); ++j) {
        // Take into account at most 8 digits
        exp_ *= 10;
        exp_ += data[j] - '0';
        if (exp_ > 18) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::NUMBER_OUT_OF_RANGE);
        }
      }
      numConsumed_ += j - i;
      if (j != size) {
        if (numConsumed_ == 0) {
          currentState_ = JSON_ERROR;
          return lastError_ = static_cast<int>(JsonError::INVALID_NUMBER);
        }
        switch (data[j]) {
        default:
          onNumberEnd();
        }
      }
      i = j - 1;
      break;
    }
    }
  }
  return i;
}

ssize_t JsonParser::parseFinal(std::span<const std::byte> data)
{
  ssize_t rv;
  rv = parseUpdate(data);
  if (rv >= 0) {
    if (currentState_ != JSON_FINISH) {
      rv = static_cast<int>(JsonError::PREMATURE_DATA);
    }
  }
  return rv;
}

void JsonParser::reset()
{
  psm_->reset();
  currentState_ = JSON_VALUE;
  lastError_ = 0;
  while (!stateStack_.empty()) {
    stateStack_.pop();
  }
  stateStack_.push(JSON_FINISH);
}

void JsonParser::onStringEnd()
{
  runEndCallback(stateTop() == JSON_OBJECT_KEY ? StructElementType::DICT_KEY
                                               : StructElementType::STRING);
  onValueEnd();
}

void JsonParser::onNumberEnd()
{
  runNumberCallback(numberSign_ * number_, frac_, expSign_ * exp_);
  runEndCallback(StructElementType::NUMBER);
  onValueEnd();
}

void JsonParser::onObjectEnd()
{
  runEndCallback(StructElementType::DICT);
  onValueEnd();
}

void JsonParser::onArrayEnd()
{
  runEndCallback(StructElementType::ARRAY);
  onValueEnd();
}

void JsonParser::onBoolEnd()
{
  runEndCallback(StructElementType::BOOL);
  onValueEnd();
}

void JsonParser::onNullEnd()
{
  runEndCallback(StructElementType::NULL_VALUE);
  onValueEnd();
}

void JsonParser::onValueEnd()
{
  switch (stateTop()) {
  case JSON_OBJECT_KEY:
    popState();
    currentState_ = JSON_OBJECT_VAL;
    break;
  case JSON_OBJECT_VAL:
    runEndCallback(StructElementType::DICT_DATA);
    popState();
    currentState_ = JSON_OBJECT_SEP;
    break;
  case JSON_ARRAY:
    runEndCallback(StructElementType::ARRAY_DATA);
    popState();
    currentState_ = JSON_ARRAY_SEP;
    break;
  default:
    assert(stateTop() == JSON_FINISH);
    currentState_ = stateTop();
    break;
  }
}

int JsonParser::pushState(int state)
{
  if (stateStack_.size() >= 50) {
    return static_cast<int>(JsonError::STRUCTURE_TOO_DEEP);
  }
  else {
    stateStack_.push(state);
    return 0;
  }
}

int JsonParser::stateTop() const { return stateStack_.top(); }

int JsonParser::popState()
{
  int state = stateStack_.top();
  stateStack_.pop();
  return state;
}

void JsonParser::consumeUnicode(char c)
{
  codepoint_ *= 16;
  codepoint_ += util::hexCharToUInt(c);
  ++numConsumed_;
  if (numConsumed_ == 4) {
    if (in(codepoint_, 0xD800u, 0xDBFFu)) {
      // This is high-surrogate codepoint
      currentState_ = JSON_STRING_LOW_SURROGATE_ESCAPE;
    }
    else {
      char temp[3];
      size_t len;
      if (codepoint_ <= 0x007fu) {
        temp[0] = static_cast<char>(codepoint_);
        len = 1;
      }
      else if (codepoint_ <= 0x07ffu) {
        temp[0] = 0xC0u | (codepoint_ >> 6);
        temp[1] = 0x80u | (codepoint_ & 0x003fu);
        len = 2;
      }
      else {
        temp[0] = 0xE0u | (codepoint_ >> 12);
        temp[1] = 0x80u | ((codepoint_ >> 6) & 0x003Fu);
        temp[2] = 0x80u | (codepoint_ & 0x003Fu);
        len = 3;
      }
      runCharactersCallback(std::as_bytes(std::span{temp, len}));
      currentState_ = JSON_STRING;
    }
  }
}

int JsonParser::consumeLowSurrogate(char c)
{
  codepoint2_ *= 16;
  codepoint2_ += util::hexCharToUInt(c);
  ++numConsumed_;
  if (numConsumed_ == 4) {
    if (!in(codepoint2_, 0xDC00u, 0xDFFFu)) {
      return static_cast<int>(JsonError::INVALID_UNICODE_POINT);
    }
    uint32_t fullcodepoint = 0x010000u;
    fullcodepoint += (codepoint_ & 0x03FFu) << 10;
    fullcodepoint += (codepoint2_ & 0x03FFu);
    char temp[4];
    temp[0] = 0xf0u | (fullcodepoint >> 18);
    temp[1] = 0x80u | ((fullcodepoint >> 12) & 0x003Fu);
    temp[2] = 0x80u | ((fullcodepoint >> 6) & 0x003Fu);
    temp[3] = 0x80u | (fullcodepoint & 0x003Fu);
    runCharactersCallback(std::as_bytes(std::span{temp, sizeof(temp)}));
    currentState_ = JSON_STRING;
  }
  return 0;
}

void JsonParser::runBeginCallback(StructElementType elementType)
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

void JsonParser::runEndCallback(StructElementType elementType)
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

void JsonParser::runCharactersCallback(std::span<const std::byte> data)
{
  psm_->charactersCallback(data);
}

void JsonParser::runNumberCallback(int64_t number, int frac, int exp)
{
  psm_->numberCallback(number, frac, exp);
}

void JsonParser::runBoolCallback(bool bval) { psm_->boolCallback(bval); }

} // namespace json

} // namespace aria2
