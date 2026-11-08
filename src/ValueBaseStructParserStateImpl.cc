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
#include "ValueBaseStructParserStateImpl.h"

#include <cassert>

#include "ValueBaseStructParserStateMachine.h"
#include "ValueBase.h"

namespace aria2 {

void ValueValueBaseStructParserState::beginElement(
    ValueBaseStructParserStateMachine* psm, StructElementType elementType)
{
  switch (elementType) {
  case StructElementType::DICT:
    psm->setCurrentFrameValue(Dict::g());
    psm->pushDictState();
    break;
  case StructElementType::ARRAY:
    psm->setCurrentFrameValue(List::g());
    psm->pushArrayState();
    break;
  case StructElementType::STRING:
    psm->pushStringState();
    break;
  case StructElementType::NUMBER:
    psm->pushNumberState();
    break;
  case StructElementType::BOOL:
    psm->pushBoolState();
    break;
  case StructElementType::NULL_VALUE:
    psm->pushNullState();
    break;
  default:
    // Not reachable
    assert(0);
  }
}

void DictValueBaseStructParserState::beginElement(
    ValueBaseStructParserStateMachine* psm, StructElementType elementType)
{
  switch (elementType) {
  case StructElementType::DICT_KEY:
    psm->pushFrame();
    psm->pushDictKeyState();
    break;
  case StructElementType::DICT_DATA:
    psm->pushDictDataState();
    break;
  default:
    // Not reachable
    assert(0);
  }
}

void DictKeyValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->setCurrentFrameName(psm->getCharacters());
}

void DictDataValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->popDictFrame();
}

void ArrayValueBaseStructParserState::beginElement(
    ValueBaseStructParserStateMachine* psm, StructElementType elementType)
{
  assert(elementType == StructElementType::ARRAY_DATA);
  psm->pushFrame();
  psm->pushArrayDataState();
}

void ArrayDataValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->popArrayFrame();
}

void StringValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->setCurrentFrameValue(String::g(psm->getCharacters()));
}

void NumberValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  // TODO Ignore frac and exp
  psm->setCurrentFrameValue(Integer::g(psm->getNumber().number));
}

void BoolValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->setCurrentFrameValue(psm->getBool() ? Bool::gTrue() : Bool::gFalse());
}

void NullValueBaseStructParserState::endElement(
    ValueBaseStructParserStateMachine* psm, StructElementType)
{
  psm->setCurrentFrameValue(Null::g());
}

} // namespace aria2
