#include "DHKeyExchange.h"

#include <string_view>
#include "Exception.h"
#include "util.h"
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class DHKeyExchangeTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHKeyExchangeTest);
  CPPUNIT_TEST(testHandshake);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testHandshake();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHKeyExchangeTest);

void DHKeyExchangeTest::testHandshake()
{
  DHKeyExchange dhA;
  DHKeyExchange dhB;

  constexpr std::string_view PRIME =
      "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA6"
      "3B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
      "E485B576625E7EC6F44C42E9A63A36210000000000090563";

  const size_t PRIME_BITS = 768;

  constexpr std::string_view GENERATOR = "2";

  dhA.init(PRIME, PRIME_BITS, GENERATOR, 160);
  dhB.init(PRIME, PRIME_BITS, GENERATOR, 160);

  dhA.generatePublicKey();
  dhB.generatePublicKey();

  unsigned char publicKeyA[96];
  unsigned char publicKeyB[96];
  dhA.getPublicKey(std::span{publicKeyA});
  dhB.getPublicKey(std::span{publicKeyB});

  unsigned char secretA[96];
  unsigned char secretB[96];
  dhA.computeSecret(std::span{secretA}, std::span{publicKeyB});
  dhB.computeSecret(std::span{secretB}, std::span{publicKeyA});

  CPPUNIT_ASSERT_EQUAL(util::toHex(std::as_bytes(std::span{secretA, sizeof(secretA)})),
                       util::toHex(std::as_bytes(std::span{secretB, sizeof(secretB)})));
}

} // namespace aria2
