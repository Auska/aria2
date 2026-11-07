#include "fmt.h"

#include <cppunit/extensions/HelperMacros.h>

#include <cstdint>
#include <string>

namespace aria2 {

class FmtTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(FmtTest);
  CPPUNIT_TEST(testFmt);
  CPPUNIT_TEST(testFmtSpecifiers);
  CPPUNIT_TEST(testFmtRuntime);
  CPPUNIT_TEST_SUITE_END();

public:
  void testFmt();
  void testFmtSpecifiers();
  void testFmtRuntime();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FmtTest);

void FmtTest::testFmt()
{
  int major = 1;
  int minor = 0;
  int release = 7;
  CPPUNIT_ASSERT_EQUAL(std::string("aria2-1.0.7-beta"),
                       fmt("aria2-{}.{}.{}-{}", major, minor, release, "beta"));
  // Strings are formatted directly, no c_str() needed any more.
  CPPUNIT_ASSERT_EQUAL(std::string("[s]"),
                       fmt("[{}]", std::string("s")));
}

void FmtTest::testFmtSpecifiers()
{
  // The printf specifiers that survived the migration must render exactly
  // the same bytes as before.
  CPPUNIT_ASSERT_EQUAL(std::string("0000002f"), fmt("{:08x}", 0x2fu));
  CPPUNIT_ASSERT_EQUAL(std::string("2F"), fmt("{:02X}", (unsigned char)0x2f));
  CPPUNIT_ASSERT_EQUAL(std::string("%2F"), fmt("%{:02X}", (unsigned char)0x2f));
  CPPUNIT_ASSERT_EQUAL(std::string("3.14"), fmt("{:.2f}", 3.14159));
  CPPUNIT_ASSERT_EQUAL(std::string("1.2"), fmt("{:.1f}", 1.25));
  // Literal braces are escaped with {{ }}.
  CPPUNIT_ASSERT_EQUAL(std::string("a.{size=1}"), fmt("a.{{size={}}}", 1));
  int x = 0;
  CPPUNIT_ASSERT_EQUAL(fmt("{:p}", (void*)&x), fmt("{:p}", (void*)&x));
}

void FmtTest::testFmtRuntime()
{
  // fmtRuntime() takes a format string built at run time.
  std::string f = "{:03}";
  CPPUNIT_ASSERT_EQUAL(std::string("007"), fmtRuntime(f, 7));
  CPPUNIT_ASSERT_EQUAL(std::string("a=1,b=2"), fmtRuntime("a={},b={}", 1, 2));
}

} // namespace aria2
