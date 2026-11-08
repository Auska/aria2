#ifndef D_MOCK_BT_MESSAGE_FACTORY_H
#define D_MOCK_BT_MESSAGE_FACTORY_H

#include "BtMessageFactory.h"

#include "BtHandshakeMessage.h"
#include "BtRequestMessage.h"
#include "BtCancelMessage.h"
#include "BtPieceMessage.h"
#include "BtHaveMessage.h"
#include "BtChokeMessage.h"
#include "BtUnchokeMessage.h"
#include "BtInterestedMessage.h"
#include "BtNotInterestedMessage.h"
#include "BtBitfieldMessage.h"
#include "BtKeepAliveMessage.h"
#include "BtHaveAllMessage.h"
#include "BtHaveNoneMessage.h"
#include "BtRejectMessage.h"
#include "BtAllowedFastMessage.h"
#include "BtPortMessage.h"
#include "BtExtendedMessage.h"
#include "ExtensionMessage.h"

namespace aria2 {

class ExtensionMessage;

class MockBtMessageFactory : public BtMessageFactory {
public:
  MockBtMessageFactory() {}

  virtual ~MockBtMessageFactory() {}

  virtual std::unique_ptr<BtMessage>
  createBtMessage(std::span<const std::byte>) override
  {
    return nullptr;
  };

  virtual std::unique_ptr<BtHandshakeMessage>
  createHandshakeMessage(std::span<const std::byte>) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtHandshakeMessage>
  createHandshakeMessage(std::span<const unsigned char>,
                         std::span<const unsigned char>) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtRequestMessage>
  createRequestMessage(const std::shared_ptr<Piece>&, size_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtCancelMessage> createCancelMessage(size_t, int32_t,
                                                               int32_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtPieceMessage> createPieceMessage(size_t, int32_t,
                                                             int32_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtHaveMessage> createHaveMessage(size_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtChokeMessage> createChokeMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtUnchokeMessage> createUnchokeMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtInterestedMessage>
  createInterestedMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtNotInterestedMessage>
  createNotInterestedMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtBitfieldMessage> createBitfieldMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtKeepAliveMessage> createKeepAliveMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtHaveAllMessage> createHaveAllMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtHaveNoneMessage> createHaveNoneMessage() override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtRejectMessage> createRejectMessage(size_t, int32_t,
                                                               int32_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtAllowedFastMessage>
  createAllowedFastMessage(size_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtPortMessage> createPortMessage(uint16_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<BtExtendedMessage>
  createBtExtendedMessage(std::unique_ptr<ExtensionMessage>) override
  {
    return nullptr;
  }
};

} // namespace aria2

#endif // D_MOCK_BT_MESSAGE_FACTORY_H
