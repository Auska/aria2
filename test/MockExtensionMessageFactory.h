#ifndef D_MOCK_EXTENSION_MESSAGE_FACTORY_H
#define D_MOCK_EXTENSION_MESSAGE_FACTORY_H

#include "ExtensionMessageFactory.h"
#include "MockExtensionMessage.h"

namespace aria2 {

class MockExtensionMessageFactory : public ExtensionMessageFactory {
public:
  virtual ~MockExtensionMessageFactory() {}

  virtual std::unique_ptr<ExtensionMessage>
  createMessage(std::span<const std::byte> data) override
  {
    auto dataPtr = reinterpret_cast<const unsigned char*>(data.data());
    return make_unique<MockExtensionMessage>("a2_mock", *dataPtr, dataPtr + 1,
                                             data.size() - 1, nullptr);
  }
};

} // namespace aria2

#endif // D_MOCK_EXTENSION_MESSAGE_FACTORY_H
