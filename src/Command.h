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
#ifndef D_COMMAND_H
#define D_COMMAND_H

#include "common.h"

namespace aria2 {

using cuid_t = int64_t;

class Command {
public:
  enum class Status { ALL, INACTIVE, ACTIVE, REALTIME, ONESHOT_REALTIME };

private:
  cuid_t cuid_;

  Status status_;

  bool readEvent_;
  bool writeEvent_;
  bool errorEvent_;
  bool hupEvent_;

protected:
  bool readEventEnabled() const { return readEvent_; }

  bool writeEventEnabled() const { return writeEvent_; }

  bool errorEventEnabled() const { return errorEvent_; }

  bool hupEventEnabled() const { return hupEvent_; }

public:
  Command(cuid_t cuid);

  virtual ~Command() noexcept = default;

  virtual bool execute() = 0;

  cuid_t getCuid() const { return cuid_; }

  void setStatusActive() { status_ = Status::ACTIVE; }

  void setStatusInactive() { status_ = Status::INACTIVE; }

  void setStatusRealtime() { status_ = Status::REALTIME; }

  void setStatus(Status status);

  bool statusMatch(Command::Status statusFilter) const
  {
    return statusFilter <= status_;
  }

  void transitStatus();

  void readEventReceived();

  void writeEventReceived();

  void errorEventReceived();

  void hupEventReceived();

  void clearIOEvents();
};

} // namespace aria2

#endif // D_COMMAND_H
