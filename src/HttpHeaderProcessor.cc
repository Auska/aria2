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
#include "HttpHeaderProcessor.h"
#include <string_view>

#include <vector>

#include "HttpHeader.h"
#include "message.h"
#include "util.h"
#include "DlRetryEx.h"
#include "DlAbortEx.h"
#include "A2STR.h"
#include "error_code.h"

namespace aria2 {

HttpHeaderProcessor::HttpHeaderProcessor(ParserMode mode)
    : mode_(mode),
      state_(mode == ParserMode::CLIENT_PARSER ? ParseState::PREV_RES_VERSION
                                               : ParseState::PREV_METHOD),
      lastBytesProcessed_(0),
      result_(make_unique<HttpHeader>())
{
}

HttpHeaderProcessor::~HttpHeaderProcessor() noexcept = default;

namespace {
size_t getToken(std::string& buf, const unsigned char* data, size_t length,
                size_t off)
{
  size_t j = off;
  while (j < length && !util::isLws(data[j]) && !util::isCRLF(data[j])) {
    ++j;
  }
  buf.append(&data[off], &data[j]);
  return j - 1;
}
} // namespace

namespace {
size_t getFieldNameToken(std::string& buf, const unsigned char* data,
                         size_t length, size_t off)
{
  size_t j = off;
  while (j < length && data[j] != ':' && !util::isLws(data[j]) &&
         !util::isCRLF(data[j])) {
    ++j;
  }
  buf.append(&data[off], &data[j]);
  return j - 1;
}
} // namespace

namespace {
size_t getText(std::string& buf, const unsigned char* data, size_t length,
               size_t off)
{
  size_t j = off;
  while (j < length && !util::isCRLF(data[j])) {
    ++j;
  }
  buf.append(&data[off], &data[j]);
  return j - 1;
}
} // namespace

namespace {
size_t ignoreText(std::string&, const unsigned char* data, size_t length,
                  size_t off)
{
  size_t j = off;
  while (j < length && !util::isCRLF(data[j])) {
    ++j;
  }
  return j - 1;
}
} // namespace

bool HttpHeaderProcessor::parse(std::span<const std::byte> data)
{
  auto dataPtr = reinterpret_cast<const unsigned char*>(data.data());
  auto length = data.size();
  size_t i;
  lastBytesProcessed_ = 0;
  for (i = 0; i < length; ++i) {
    unsigned char c = dataPtr[i];
    switch (state_) {
    case ParseState::PREV_METHOD:
      if (util::isLws(c) || util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Request-Line: missing method");
      }

      i = getToken(buf_, dataPtr, length, i);
      state_ = ParseState::METHOD;
      break;

    case ParseState::METHOD:
      if (util::isLws(c)) {
        result_->setMethod(buf_);
        buf_.clear();
        state_ = ParseState::PREV_PATH;
        break;
      }

      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Request-Line: missing request-target");
      }

      i = getToken(buf_, dataPtr, length, i);
      break;

    case ParseState::PREV_PATH:
      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Request-Line: missing request-target");
      }

      if (util::isLws(c)) {
        break;
      }

      i = getToken(buf_, dataPtr, length, i);
      state_ = ParseState::PATH;
      break;

    case ParseState::PATH:
      if (util::isLws(c)) {
        result_->setRequestPath(buf_);
        buf_.clear();
        state_ = ParseState::PREV_REQ_VERSION;
        break;
      }

      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Request-Line: missing HTTP-version");
      }

      i = getToken(buf_, dataPtr, length, i);
      break;

    case ParseState::PREV_REQ_VERSION:
      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Request-Line: missing HTTP-version");
      }

      if (util::isLws(c)) {
        break;
      }

      i = getToken(buf_, dataPtr, length, i);
      state_ = ParseState::REQ_VERSION;
      break;

    case ParseState::REQ_VERSION:
      if (util::isCRLF(c)) {
        result_->setVersion(buf_);
        buf_.clear();
        state_ = c == '\n' ? ParseState::PREV_FIELD_NAME : ParseState::PREV_EOL;
        break;
      }

      if (util::isLws(c)) {
        throw DL_ABORT_EX("Bad Request-Line: LWS after HTTP-version");
      }

      i = getToken(buf_, dataPtr, length, i);
      break;

    case ParseState::PREV_RES_VERSION:
      if (util::isLws(c) || util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Status-Line: missing HTTP-version");
      }

      i = getToken(buf_, dataPtr, length, i);
      state_ = ParseState::RES_VERSION;
      break;

    case ParseState::RES_VERSION:
      if (util::isLws(c)) {
        result_->setVersion(buf_);
        buf_.clear();
        state_ = ParseState::PREV_STATUS_CODE;
        break;
      }

      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Status-Line: missing status-code");
      }

      break;

    case ParseState::PREV_STATUS_CODE:
      if (util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad Status-Line: missing status-code");
      }

      if (!util::isLws(c)) {
        state_ = ParseState::STATUS_CODE;
        i = getToken(buf_, dataPtr, length, i);
      }

      break;

    case ParseState::STATUS_CODE:
      if (!util::isLws(c) && !util::isCRLF(c)) {
        i = getToken(buf_, dataPtr, length, i);
        break;
      }

      {
        int statusCode = -1;
        if (buf_.size() == 3 && util::isNumber(buf_.begin(), buf_.end())) {
          statusCode =
              (buf_[0] - '0') * 100 + (buf_[1] - '0') * 10 + (buf_[2] - '0');
        }
        if (statusCode < 100) {
          throw DL_ABORT_EX("Bad status code: bad status-code");
        }
        result_->setStatusCode(statusCode);
        buf_.clear();
      }
      if (c == '\r') {
        state_ = ParseState::PREV_EOL;
        break;
      }

      if (c == '\n') {
        state_ = ParseState::PREV_FIELD_NAME;
        break;
      }

      state_ = ParseState::PREV_REASON_PHRASE;
      break;

    case ParseState::PREV_REASON_PHRASE:
      if (util::isCRLF(c)) {
        // The reason-phrase is completely optional.
        state_ = c == '\n' ? ParseState::PREV_FIELD_NAME : ParseState::PREV_EOL;
        break;
      }

      if (util::isLws(c)) {
        break;
      }

      state_ = ParseState::REASON_PHRASE;
      i = getText(buf_, dataPtr, length, i);
      break;

    case ParseState::REASON_PHRASE:
      if (util::isCRLF(c)) {
        result_->setReasonPhrase(buf_);
        buf_.clear();
        state_ = c == '\n' ? ParseState::PREV_FIELD_NAME : ParseState::PREV_EOL;
        break;
      }

      i = getText(buf_, dataPtr, length, i);
      break;

    case ParseState::PREV_EOL:
      if (c != '\n') {
        throw DL_ABORT_EX("Bad HTTP header: missing LF");
      }

      state_ = ParseState::PREV_FIELD_NAME;
      break;

    case ParseState::PREV_FIELD_NAME:
      if (util::isLws(c)) {
        if (lastFieldName_.empty()) {
          throw DL_ABORT_EX("Bad HTTP header: field name starts with LWS");
        }
        // Evil Multi-line header field
        state_ = ParseState::FIELD_VALUE;
        break;
      }

      if (!lastFieldName_.empty()) {
        if (lastFieldHdKey_) {
          result_->put(*lastFieldHdKey_, util::strip(buf_));
        }
        lastFieldName_.clear();
        lastFieldHdKey_.reset();
        buf_.clear();
      }
      if (c == '\n') {
        state_ = ParseState::HEADERS_COMPLETE;
        break;
      }

      if (c == '\r') {
        state_ = ParseState::PREV_EOH;
        break;
      }

      if (c == ':') {
        throw DL_ABORT_EX("Bad HTTP header: field name starts with ':'");
      }

      state_ = ParseState::FIELD_NAME;
      i = getFieldNameToken(lastFieldName_, dataPtr, length, i);
      break;

    case ParseState::FIELD_NAME:
      if (util::isLws(c) || util::isCRLF(c)) {
        throw DL_ABORT_EX("Bad HTTP header: missing ':'");
      }

      if (c == ':') {
        util::lowercase(lastFieldName_);
        lastFieldHdKey_ = idInterestingHeader(lastFieldName_.c_str());
        state_ = ParseState::PREV_FIELD_VALUE;
        break;
      }

      i = getFieldNameToken(lastFieldName_, dataPtr, length, i);
      break;

    case ParseState::PREV_FIELD_VALUE:
      if (c == '\r') {
        state_ = ParseState::PREV_EOL;
        break;
      }

      if (c == '\n') {
        state_ = ParseState::PREV_FIELD_NAME;
        break;
      }

      if (util::isLws(c)) {
        break;
      }

      state_ = ParseState::FIELD_VALUE;
      if (!lastFieldHdKey_) {
        i = ignoreText(buf_, dataPtr, length, i);
        break;
      }

      i = getText(buf_, dataPtr, length, i);
      break;

    case ParseState::FIELD_VALUE:
      if (c == '\r') {
        state_ = ParseState::PREV_EOL;
        break;
      }

      if (c == '\n') {
        state_ = ParseState::PREV_FIELD_NAME;
        break;
      }

      if (!lastFieldHdKey_) {
        i = ignoreText(buf_, dataPtr, length, i);
        break;
      }

      i = getText(buf_, dataPtr, length, i);
      break;

    case ParseState::PREV_EOH:
      if (c != '\n') {
        throw DL_ABORT_EX("Bad HTTP header: "
                          "missing LF at the end of the header");
      }

      state_ = ParseState::HEADERS_COMPLETE;
      break;

    case ParseState::HEADERS_COMPLETE:
      goto fin;
    }
  }

fin:
  // See Apache's documentation
  // http://httpd.apache.org/docs/2.2/en/mod/core.html about size
  // limit of HTTP headers. The page states that the number of request
  // fields rarely exceeds 20.
  if (lastFieldName_.size() > 1024 || buf_.size() > 8_k) {
    throw DL_ABORT_EX("Too large HTTP header");
  }

  lastBytesProcessed_ = i;
  headers_.append(&dataPtr[0], &dataPtr[i]);

  if (state_ != ParseState::HEADERS_COMPLETE) {
    return false;
  }

  // If both transfer-encoding and (content-length or content-range)
  // are present, delete content-length and content-range.  RFC 7230
  // says that sender must not send both transfer-encoding and
  // content-length.  If both present, transfer-encoding overrides
  // content-length.  There is no text about transfer-encoding and
  // content-range.  But there is no reason to send transfer-encoding
  // when range is set.
  if (result_->defined(HttpHeader::InterestingHeader::TRANSFER_ENCODING)) {
    result_->remove(HttpHeader::InterestingHeader::CONTENT_LENGTH);
    result_->remove(HttpHeader::InterestingHeader::CONTENT_RANGE);
  }

  return true;
}

bool HttpHeaderProcessor::parse(std::string_view data)
{
  return parse(std::as_bytes(std::span{data}));
}

size_t HttpHeaderProcessor::getLastBytesProcessed() const
{
  return lastBytesProcessed_;
}

void HttpHeaderProcessor::clear()
{
  state_ = (mode_ == ParserMode::CLIENT_PARSER ? ParseState::PREV_RES_VERSION
                                               : ParseState::PREV_METHOD);
  lastBytesProcessed_ = 0;
  buf_.clear();
  lastFieldName_.clear();
  lastFieldHdKey_.reset();
  result_ = make_unique<HttpHeader>();
  headers_.clear();
}

std::unique_ptr<HttpHeader> HttpHeaderProcessor::getResult()
{
  return std::move(result_);
}

std::string HttpHeaderProcessor::getHeaderString() const { return headers_; }

} // namespace aria2
