#ifndef D_MOCK_DHT_TASK_FACTORY_H
#define D_MOCK_DHT_TASK_FACTORY_H

#include "DHTTaskFactory.h"

namespace aria2 {

class MockDHTTaskFactory : public DHTTaskFactory {
public:
  virtual ~MockDHTTaskFactory() {}

  virtual std::shared_ptr<DHTTask>
  createPingTask(const std::shared_ptr<DHTNode>&, int = 0) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask> createNodeLookupTask(const DHTID&) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask> createBucketRefreshTask() override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask>
  createPeerLookupTask(const std::shared_ptr<DownloadContext>&, uint16_t,
                       const std::shared_ptr<PeerStorage>&) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask> createPeerAnnounceTask(const DHTID&) override
  {
    return nullptr;
  }

  virtual std::shared_ptr<DHTTask>
  createReplaceNodeTask(const std::shared_ptr<DHTBucket>&,
                        const std::shared_ptr<DHTNode>&) override
  {
    return nullptr;
  }
};

} // namespace aria2

#endif // D_MOCK_DHT_TASK_FACTORY_H
