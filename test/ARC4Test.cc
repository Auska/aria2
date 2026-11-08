#include "ARC4Encryptor.h"

#include <cstring>
#include <cppunit/extensions/HelperMacros.h>

#include "Exception.h"
#include "util.h"

namespace aria2 {

class ARC4Test : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ARC4Test);
  CPPUNIT_TEST(testEncrypt);
  CPPUNIT_TEST(testKeystreamVector);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testEncrypt();
  void testKeystreamVector();
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

// RFC 6229 test vector for a 40-bit key. The internal ARC4 implementation
// is now the only one, so pin its keystream against a published vector.
void ARC4Test::testKeystreamVector()
{
  const unsigned char key[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  const unsigned char expected[] = {0xb2, 0x39, 0x63, 0x05, 0xf0, 0x3d,
                                    0xc0, 0x27, 0xcc, 0xc3, 0x52, 0x4a,
                                    0x0a, 0x11, 0x18, 0xa8};
  unsigned char zeros[sizeof(expected)] = {};
  unsigned char out[sizeof(expected)];

  ARC4Encryptor enc;
  enc.init(key, sizeof(key));
  enc.encrypt(std::as_writable_bytes(std::span{out}),
              std::as_bytes(std::span{zeros}));
  CPPUNIT_ASSERT_EQUAL(std::string(std::begin(expected), std::end(expected)),
                       std::string(std::begin(out), std::end(out)));
}

} // namespace aria2
