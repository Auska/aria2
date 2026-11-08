#ifndef D_MOCK_DHT_MESSAGE_FACTORY_H
#define D_MOCK_DHT_MESSAGE_FACTORY_H

#include "DHTMessageFactory.h"
#include "DHTNode.h"
#include "MockDHTMessage.h"
#include "DHTPingMessage.h"
#include "DHTPingReplyMessage.h"
#include "DHTFindNodeMessage.h"
#include "DHTFindNodeReplyMessage.h"
#include "DHTGetPeersMessage.h"
#include "DHTGetPeersReplyMessage.h"
#include "DHTAnnouncePeerMessage.h"
#include "DHTAnnouncePeerReplyMessage.h"
#include "DHTUnknownMessage.h"

namespace aria2 {

class MockDHTMessageFactory : public DHTMessageFactory {
protected:
  std::shared_ptr<DHTNode> localNode_;

public:
  MockDHTMessageFactory() {}

  virtual std::unique_ptr<DHTQueryMessage>
  createQueryMessage(const Dict*, const std::string&, uint16_t) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTResponseMessage>
  createResponseMessage(const std::string&, const Dict* dict,
                        const std::string& ipaddr, uint16_t port) override
  {
    auto remoteNode = std::make_shared<DHTNode>();
    // TODO At this point, removeNode's ID is random.
    remoteNode->setIPAddress(ipaddr);
    remoteNode->setPort(port);
    return make_unique<MockDHTResponseMessage>(
        localNode_, remoteNode, downcast<String>(dict->get("t"))->s());
  }

  virtual std::unique_ptr<DHTPingMessage>
  createPingMessage(const std::shared_ptr<DHTNode>&,
                    const std::string& = "") override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTPingReplyMessage>
  createPingReplyMessage(const std::shared_ptr<DHTNode>&, const DHTID&,
                         const std::string&) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTFindNodeMessage>
  createFindNodeMessage(const std::shared_ptr<DHTNode>&, const DHTID&,
                        const std::string& = "") override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTFindNodeReplyMessage>
  createFindNodeReplyMessage(const std::shared_ptr<DHTNode>&,
                             std::vector<std::shared_ptr<DHTNode>>,
                             const std::string&) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTGetPeersMessage>
  createGetPeersMessage(const std::shared_ptr<DHTNode>&, const DHTID&,
                        const std::string&) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTGetPeersReplyMessage>
  createGetPeersReplyMessage(const std::shared_ptr<DHTNode>&,
                             std::vector<std::shared_ptr<DHTNode>>,
                             std::vector<std::shared_ptr<Peer>>,
                             const std::string&, const std::string&) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTAnnouncePeerMessage>
  createAnnouncePeerMessage(const std::shared_ptr<DHTNode>&, const DHTID&,
                            uint16_t, const std::string&,
                            const std::string& = "") override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTAnnouncePeerReplyMessage>
  createAnnouncePeerReplyMessage(const std::shared_ptr<DHTNode>&,
                                 const std::string&) override
  {
    return nullptr;
  }

  virtual std::unique_ptr<DHTUnknownMessage>
  createUnknownMessage(std::span<const unsigned char>, const std::string&,
                       uint16_t) override
  {
    return nullptr;
  }

  void setLocalNode(const std::shared_ptr<DHTNode>& node) { localNode_ = node; }
};

} // namespace aria2

#endif // D_MOCK_DHT_MESSAGE_FACTORY_H
