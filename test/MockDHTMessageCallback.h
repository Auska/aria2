#ifndef D_MOCK_DHT_MESSAGE_CALLBACK_H
#define D_MOCK_DHT_MESSAGE_CALLBACK_H

#include "DHTMessageCallback.h"

namespace aria2 {

class MockDHTMessageCallback : public DHTMessageCallback {
public:
  MockDHTMessageCallback() {}

  virtual ~MockDHTMessageCallback() {}

  virtual void visit(const DHTAnnouncePeerReplyMessage*) override {}

  virtual void visit(const DHTFindNodeReplyMessage*) override {}

  virtual void visit(const DHTGetPeersReplyMessage*) override {}

  virtual void visit(const DHTPingReplyMessage*) override {}

  virtual void onTimeout(const std::shared_ptr<DHTNode>&) override {}
};

} // namespace aria2

#endif // D_MOCK_DHT_MESSAGE_CALLBACK_H
