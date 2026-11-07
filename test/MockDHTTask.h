#ifndef D_MOCK_DHT_TASK_H
#define D_MOCK_DHT_TASK_H

#include "DHTTask.h"

#include <cstring>

#include "DHTNode.h"
#include "DHTConstants.h"

namespace aria2 {

class MockDHTTask : public DHTTask {
public:
  std::shared_ptr<DHTNode> remoteNode_;

  DHTID targetID_;

  bool finished_;

  MockDHTTask(const std::shared_ptr<DHTNode>& remoteNode)
      : remoteNode_(remoteNode), finished_(false)
  {
  }

  virtual ~MockDHTTask() {}

  virtual void startup() override {}

  virtual bool finished() override { return finished_; }

  void setTargetID(const DHTID& targetID) { targetID_ = targetID; }
};

} // namespace aria2

#endif // D_MOCK_DHT_TASK_H
