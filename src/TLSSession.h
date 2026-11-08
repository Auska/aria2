/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2013 Tatsuhiro Tsujikawa
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
#ifndef TLS_SESSION_H
#define TLS_SESSION_H

#include <span>

#include "common.h"
#include "a2netcompat.h"
#include "TLSContext.h"

namespace aria2 {

enum class TLSDirection { WANT_READ = 1, WANT_WRITE };

enum class TLSErrorCode { OK = 0, ERROR = -1, WOULDBLOCK = -2 };

// To create another SSL/TLS backend, implement TLSSession class below.
//
class TLSSession {
public:
  static TLSSession* make(TLSContext* ctx);

  // MUST deallocate all resources
  virtual ~TLSSession() noexcept = default;

  // Initializes SSL/TLS session. The |sockfd| is the underlying
  // transport socket. This function returns TLS_ERR_OK if it
  // succeeds, or TLS_ERR_ERROR.
  virtual TLSErrorCode init(sock_t sockfd) = 0;

  // Sets |hostname| for TLS SNI extension. This is only meaningful for
  // client side session. This function returns TLS_ERR_OK if it
  // succeeds, or TLS_ERR_ERROR.
  virtual TLSErrorCode setSNIHostname(const std::string& hostname) = 0;

  // Closes the SSL/TLS session. Don't close underlying transport
  // socket. This function returns TLS_ERR_OK if it succeeds, or
  // TLS_ERR_ERROR.
  virtual TLSErrorCode closeConnection() = 0;

  // Returns TLS_WANT_READ if SSL/TLS session needs more data from
  // remote endpoint to proceed, or TLS_WANT_WRITE if SSL/TLS session
  // needs to write more data to proceed. If SSL/TLS session needs
  // neither read nor write data at the moment, TLS_WANT_READ must be
  // returned.
  virtual TLSDirection checkDirection() = 0;

  // Sends |data| with length |len|. This function returns the number
  // of bytes sent if it succeeds, or TLS_ERR_WOULDBLOCK if the
  // underlying transport blocks, or TLS_ERR_ERROR.
  virtual ssize_t writeData(std::span<const std::byte> data) = 0;

  // Receives data into |data| with length |len|. This function returns
  // the number of bytes received if it succeeds, or TLS_ERR_WOULDBLOCK
  // if the underlying transport blocks, or TLS_ERR_ERROR.
  virtual ssize_t readData(std::span<std::byte> data) = 0;

  // Performs client side handshake. The |hostname| is the hostname of
  // the remote endpoint and is used to verify its certificate. This
  // function returns TLS_ERR_OK if it succeeds, or TLS_ERR_WOULDBLOCK
  // if the underlying transport blocks, or TLS_ERR_ERROR.
  // When returning TLS_ERR_ERROR, provide certificate validation error
  // in |handshakeErr|.
  virtual TLSErrorCode tlsConnect(const std::string& hostname,
                                  TLSVersion& version,
                                  std::string& handshakeErr) = 0;

  // Performs server side handshake. This function returns TLS_ERR_OK
  // if it succeeds, or TLS_ERR_WOULDBLOCK if the underlying transport
  // blocks, or TLS_ERR_ERROR.
  virtual TLSErrorCode tlsAccept(TLSVersion& version) = 0;

  // Returns last error string
  virtual std::string getLastErrorString() = 0;

  // Returns buffered length, which can be read immediately without
  // contacting network.
  virtual size_t getRecvBufferedLength() = 0;

protected:
  TLSSession() = default;

private:
  TLSSession(const TLSSession&) = delete;
  TLSSession& operator=(const TLSSession&) = delete;
};
} // namespace aria2

#endif // TLS_SESSION_H
