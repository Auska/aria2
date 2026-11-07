#include "ARC4Encryptor.h"

#include <cstring>
#include <cppunit/extensions/HelperMacros.h>

#include "Exception.h"
#include "util.h"

namespace aria2 {

class ARC4Test : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ARC4Test);
  CPPUNIT_TEST(testEncrypt);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testEncrypt();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ARC4Test);

void ARC4Test::testEncrypt()
{
  ARC4Encryptor enc;
  ARC4Encryptor dec;
  const size_t LEN = 20;
  unsigned char key[LEN];
  memset(key, 0, LEN);
  util::generateRandomData(std::as_writable_bytes(std::span{key, sizeof(key)}));
  enc.init(key, sizeof(key));
  dec.init(key, sizeof(key));

  unsigned char encrypted[LEN];
  unsigned char decrypted[LEN];

  enc.encrypt(std::as_writable_bytes(std::span{encrypted}),
              std::as_bytes(std::span{key}));
  CPPUNIT_ASSERT(memcmp(key, encrypted, LEN) != 0);
  dec.encrypt(std::as_writable_bytes(std::span{decrypted}),
              std::as_bytes(std::span{encrypted}));
  CPPUNIT_ASSERT(memcmp(key, decrypted, LEN) == 0);

  // once more
  enc.encrypt(std::as_writable_bytes(std::span{encrypted}),
              std::as_bytes(std::span{key}));
  CPPUNIT_ASSERT(memcmp(key, encrypted, LEN) != 0);
  dec.encrypt(std::as_writable_bytes(std::span{decrypted}),
              std::as_bytes(std::span{encrypted}));
  CPPUNIT_ASSERT(memcmp(key, decrypted, LEN) == 0);
}

} // namespace aria2
