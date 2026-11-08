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
#include "ReceiverMSEHandshakeCommand.h"
#include "PeerReceiveHandshakeCommand.h"
#include "PeerConnection.h"
#include "DownloadEngine.h"
#include "BtHandshakeMessage.h"
#include "DlAbortEx.h"
#include "Peer.h"
#include "message.h"
#include "SocketCore.h"
#include "Logger.h"
#include "prefs.h"
#include "Option.h"
#include "MSEHandshake.h"
#include "ARC4Encryptor.h"
#include "RequestGroupMan.h"
#include "BtRegistry.h"
#include "DownloadContext.h"
#include "array_fun.h"

namespace aria2 {

ReceiverMSEHandshakeCommand::ReceiverMSEHandshakeCommand(
    cuid_t cuid, const std::shared_ptr<Peer>& peer, DownloadEngine* e,
    const std::shared_ptr<SocketCore>& s)
    :

      PeerAbstractCommand(cuid, peer, e, s),
      sequence_(Seq::IDENTIFY_HANDSHAKE),
      mseHandshake_(make_unique<MSEHandshake>(cuid, s, e->getOption()))
{
  setTimeout(std::chrono::seconds(
      e->getOption()->getAsInt(PREF_PEER_CONNECTION_TIMEOUT)));
  mseHandshake_->setWantRead(true);
}

ReceiverMSEHandshakeCommand::~ReceiverMSEHandshakeCommand() = default;

bool ReceiverMSEHandshakeCommand::exitBeforeExecute()
{
  return getDownloadEngine()->isHaltRequested() ||
         getDownloadEngine()->getRequestGroupMan()->downloadFinished();
}

bool ReceiverMSEHandshakeCommand::executeInternal()
{
  if (mseHandshake_->getWantRead()) {
    mseHandshake_->read();
  }
  bool done = false;
  while (!done) {
    switch (sequence_) {
    case Seq::IDENTIFY_HANDSHAKE: {
      auto type = mseHandshake_->identifyHandshakeType();
      switch (type) {
      case MSEHandshake::HandshakeType::NotYet:
        done = true;
        break;
      case MSEHandshake::HandshakeType::Encrypted:
        mseHandshake_->initEncryptionFacility(false);
        sequence_ = Seq::WAIT_KEY;
        break;
      case MSEHandshake::HandshakeType::Legacy: {
        const auto option = getDownloadEngine()->getOption();
        if (option->getAsBool(PREF_BT_FORCE_ENCRYPTION) ||
            option->getAsBool(PREF_BT_REQUIRE_CRYPTO)) {
          throw DL_ABORT_EX(
              "The legacy BitTorrent handshake is not acceptable by the"
              " preference.");
        }
        auto peerConnection =
            make_unique<PeerConnection>(getCuid(), getPeer(), getSocket());
        peerConnection->presetBuffer(mseHandshake_->getBuffer(),
                                     mseHandshake_->getBufferLength());
        getDownloadEngine()->addCommand(
            make_unique<PeerReceiveHandshakeCommand>(
                getCuid(), getPeer(), getDownloadEngine(), getSocket(),
                std::move(peerConnection)));
        return true;
      }
      default:
        throw DL_ABORT_EX("Not supported handshake type.");
      }
      break;
    }
    case Seq::WAIT_KEY: {
      if (mseHandshake_->receivePublicKey()) {
        mseHandshake_->sendPublicKey();
        sequence_ = Seq::SEND_KEY_PENDING;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::SEND_KEY_PENDING:
      if (mseHandshake_->send()) {
        sequence_ = Seq::FIND_HASH_MARKER;
      }
      else {
        done = true;
      }
      break;
    case Seq::FIND_HASH_MARKER: {
      if (mseHandshake_->findReceiverHashMarker()) {
        sequence_ = Seq::RECEIVE_PAD_C_LENGTH;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::RECEIVE_PAD_C_LENGTH: {
      std::vector<std::shared_ptr<DownloadContext>> downloadContexts;
      getDownloadEngine()->getBtRegistry()->getAllDownloadContext(
          std::back_inserter(downloadContexts));
      if (mseHandshake_->receiveReceiverHashAndPadCLength(downloadContexts)) {
        sequence_ = Seq::RECEIVE_PAD_C;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::RECEIVE_PAD_C: {
      if (mseHandshake_->receivePad()) {
        sequence_ = Seq::RECEIVE_IA_LENGTH;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::RECEIVE_IA_LENGTH: {
      if (mseHandshake_->receiveReceiverIALength()) {
        sequence_ = Seq::RECEIVE_IA;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::RECEIVE_IA: {
      if (mseHandshake_->receiveReceiverIA()) {
        mseHandshake_->sendReceiverStep2();
        sequence_ = Seq::SEND_STEP2_PENDING;
      }
      else {
        done = true;
      }
      break;
    }
    case Seq::SEND_STEP2_PENDING:
      if (mseHandshake_->send()) {
        createCommand();
        return true;
      }
      else {
        done = true;
      }
      break;
    }
  }
  if (mseHandshake_->getWantRead()) {
    setReadCheckSocket(getSocket());
  }
  else {
    disableReadCheckSocket();
  }
  if (mseHandshake_->getWantWrite()) {
    setWriteCheckSocket(getSocket());
  }
  else {
    disableWriteCheckSocket();
  }
  addCommandSelf();
  return false;
}

void ReceiverMSEHandshakeCommand::createCommand()
{
  auto peerConnection =
      make_unique<PeerConnection>(getCuid(), getPeer(), getSocket());
  if (mseHandshake_->getNegotiatedCryptoType() == MSEHandshake::CryptoType::Arc4) {
    peerConnection->enableEncryption(mseHandshake_->popEncryptor(),
                                     mseHandshake_->popDecryptor());
  }
  // Since initiator cannot send payload stream before reading step2
  // from receiver, mseHandshake_->getBufferLength() should be 0.
  peerConnection->presetBuffer(mseHandshake_->getIA(),
                               mseHandshake_->getIALength());
  // TODO add mseHandshake_->getInfoHash() to PeerReceiveHandshakeCommand
  // as a hint. If this info hash and one in BitTorrent Handshake does not
  // match, then drop connection.
  getDownloadEngine()->addCommand(make_unique<PeerReceiveHandshakeCommand>(
      getCuid(), getPeer(), getDownloadEngine(), getSocket(),
      std::move(peerConnection)));
}

} // namespace aria2
