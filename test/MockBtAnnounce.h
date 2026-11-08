#ifndef D_MOCK_BT_ANNOUNCE_H
#define D_MOCK_BT_ANNOUNCE_H

#include "BtAnnounce.h"

#include <string_view>

namespace aria2 {

class MockBtAnnounce : public BtAnnounce {
private:
  bool announceReady;
  std::string announceUrl;
  std::string peerId;

public:
  MockBtAnnounce() {}
  virtual ~MockBtAnnounce() {}

  virtual bool isAnnounceReady() override { return announceReady; }

  void setAnnounceReady(bool flag) { this->announceReady = flag; }

  virtual std::string getAnnounceUrl() override { return announceUrl; }

  virtual std::shared_ptr<UDPTrackerRequest>
  createUDPTrackerRequest(std::string_view, uint16_t, uint16_t) override
  {
    return nullptr;
  }

  void setAnnounceUrl(const std::string& url) { this->announceUrl = url; }

  virtual void announceStart() override {}

  virtual void announceSuccess() override {}

  virtual void announceFailure() override {}

  virtual bool isAllAnnounceFailed() override { return false; }

  virtual void resetAnnounce() override {}

  virtual void processAnnounceResponse(std::span<const std::byte>) override {}

  virtual void
  processUDPTrackerResponse(const std::shared_ptr<UDPTrackerRequest>&) override
  {
  }

  virtual bool noMoreAnnounce() override { return false; }

  virtual void shuffleAnnounce() override {}

  virtual void overrideMinInterval(std::chrono::seconds) override {}

  virtual void setTcpPort(uint16_t) override {}

  void setPeerId(const std::string& peerId) { this->peerId = peerId; }
};

} // namespace aria2

#endif // D_MOCK_BT_ANNOUNCE_H
