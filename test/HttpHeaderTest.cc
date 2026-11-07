#include "HttpHeader.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Range.h"
#include "DlAbortEx.h"

namespace aria2 {

class HttpHeaderTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpHeaderTest);
  CPPUNIT_TEST(testGetRange);
  CPPUNIT_TEST(testFindAll);
  CPPUNIT_TEST(testClearField);
  CPPUNIT_TEST(testFieldContains);
  CPPUNIT_TEST(testRemove);
  CPPUNIT_TEST_SUITE_END();

public:
  void testGetRange();
  void testFindAll();
  void testClearField();
  void testFieldContains();
  void testRemove();
};

CPPUNIT_TEST_SUITE_REGISTRATION(HttpHeaderTest);

void HttpHeaderTest::testGetRange()
{
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::InterestingHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775800LL, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775801LL, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(
        HttpHeader::InterestingHeader::CONTENT_RANGE,
        "9223372036854775800-9223372036854775801/9223372036854775807");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775800LL, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775801LL, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775807LL, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes */1024");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes 0-9/*");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes */*");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes 0");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes 0/");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.entityLength);
  }
  {
    // Support for non-compliant server
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes=0-1023/1024");

    Range range = httpHeader.getRange();

    CPPUNIT_ASSERT_EQUAL((int64_t)0, range.startByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)1023, range.endByte);
    CPPUNIT_ASSERT_EQUAL((int64_t)1024, range.entityLength);
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes 0-/3");
    try {
      httpHeader.getRange();
      CPPUNIT_FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
  {
    HttpHeader httpHeader;
    httpHeader.put(HttpHeader::InterestingHeader::CONTENT_RANGE, "bytes -0/3");
    try {
      httpHeader.getRange();
      CPPUNIT_FAIL("Exception must be thrown");
    }
    catch (const DlAbortEx& e) {
      // success
    }
  }
}

void HttpHeaderTest::testFindAll()
{
  HttpHeader h;
  h.put(HttpHeader::InterestingHeader::LINK, "100");
  h.put(HttpHeader::InterestingHeader::LINK, "101");
  h.put(HttpHeader::InterestingHeader::CONNECTION, "200");

  std::vector<std::string> r(h.findAll(HttpHeader::InterestingHeader::LINK));
  CPPUNIT_ASSERT_EQUAL((size_t)2, r.size());
  CPPUNIT_ASSERT_EQUAL(std::string("100"), r[0]);
  CPPUNIT_ASSERT_EQUAL(std::string("101"), r[1]);
}

void HttpHeaderTest::testClearField()
{
  HttpHeader h;
  h.setStatusCode(200);
  h.setVersion("HTTP/1.1");
  h.put(HttpHeader::InterestingHeader::LINK, "Bar");

  CPPUNIT_ASSERT_EQUAL(std::string("Bar"), h.find(HttpHeader::InterestingHeader::LINK));

  h.clearField();

  CPPUNIT_ASSERT_EQUAL(std::string(""), h.find(HttpHeader::InterestingHeader::LINK));
  CPPUNIT_ASSERT_EQUAL(200, h.getStatusCode());
  CPPUNIT_ASSERT_EQUAL(std::string("HTTP/1.1"), h.getVersion());
}

void HttpHeaderTest::testFieldContains()
{
  HttpHeader h;
  h.put(HttpHeader::InterestingHeader::CONNECTION, "Keep-Alive, Upgrade");
  h.put(HttpHeader::InterestingHeader::UPGRADE, "WebSocket");
  h.put(HttpHeader::InterestingHeader::SEC_WEBSOCKET_VERSION, "13");
  h.put(HttpHeader::InterestingHeader::SEC_WEBSOCKET_VERSION, "8, 7");
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::InterestingHeader::CONNECTION, "upgrade"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::InterestingHeader::CONNECTION, "keep-alive"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::InterestingHeader::CONNECTION, "close"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::InterestingHeader::UPGRADE, "websocket"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::InterestingHeader::UPGRADE, "spdy"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::InterestingHeader::SEC_WEBSOCKET_VERSION, "13"));
  CPPUNIT_ASSERT(h.fieldContains(HttpHeader::InterestingHeader::SEC_WEBSOCKET_VERSION, "8"));
  CPPUNIT_ASSERT(!h.fieldContains(HttpHeader::InterestingHeader::SEC_WEBSOCKET_VERSION, "6"));
}

void HttpHeaderTest::testRemove()
{
  HttpHeader h;
  h.put(HttpHeader::InterestingHeader::CONNECTION, "close");
  h.put(HttpHeader::InterestingHeader::TRANSFER_ENCODING, "chunked");
  h.put(HttpHeader::InterestingHeader::TRANSFER_ENCODING, "gzip");

  h.remove(HttpHeader::InterestingHeader::TRANSFER_ENCODING);

  CPPUNIT_ASSERT(!h.defined(HttpHeader::InterestingHeader::TRANSFER_ENCODING));
  CPPUNIT_ASSERT(h.defined(HttpHeader::InterestingHeader::CONNECTION));
}

} // namespace aria2
