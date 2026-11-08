#include <span>

#include "ValueBaseBencodeParser.h"

#include <cppunit/extensions/HelperMacros.h>

#include "ValueBase.h"
#include "BencodeParser.h"

namespace aria2 {

class ValueBaseBencodeParserTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ValueBaseBencodeParserTest);
  CPPUNIT_TEST(testParseUpdate);
  CPPUNIT_TEST_SUITE_END();

public:
  void testParseUpdate();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ValueBaseBencodeParserTest);

namespace {
// Parses |src| and asserts success, returning the decoded value.
std::unique_ptr<ValueBase>
parseValue(bittorrent::ValueBaseBencodeParser& parser, const std::string& src)
{
  auto res = parser.parseFinal(std::as_bytes(std::span{src}));
  CPPUNIT_ASSERT(res);
  return std::move(*res);
}

void checkDecodeError(const std::string& src)
{
  bittorrent::ValueBaseBencodeParser parser;
  auto r = parser.parseFinal(std::as_bytes(std::span{src}));
  CPPUNIT_ASSERT(!r);
}
} // namespace

void ValueBaseBencodeParserTest::testParseUpdate()
{
  bittorrent::ValueBaseBencodeParser parser;
  {
    // empty string
    std::string src = "0:";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT_EQUAL(std::string(""), downcast<String>(s)->s());
  }
  {
    // integer 0
    std::string src = "i0e";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, downcast<Integer>(s)->i());
  }
  {
    // empty dict
    std::string src = "de";
    auto d = parseValue(parser, src);
    CPPUNIT_ASSERT(downcast<Dict>(d)->empty());
  }
  {
    // empty list
    std::string src = "le";
    auto l = parseValue(parser, src);
    CPPUNIT_ASSERT(downcast<List>(l)->empty());
  }
  {
    // string
    std::string src = "3:foo";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), downcast<String>(s)->s());
  }
  {
    // integer
    std::string src = "i9223372036854775807e";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT_EQUAL((int64_t)9223372036854775807LL,
                         downcast<Integer>(s)->i());
  }
  {
    // float number, ignored and always 0.
    std::string src = "i+343243.342E-1333e";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT_EQUAL((int64_t)0, downcast<Integer>(s)->i());
  }
  {
    // dict, size 1
    std::string src = "d3:fooi123ee";
    auto d = parseValue(parser, src);
    Dict* dict = downcast<Dict>(d);
    CPPUNIT_ASSERT(dict);
    CPPUNIT_ASSERT(dict->get("foo"));
    CPPUNIT_ASSERT_EQUAL((int64_t)123,
                         downcast<Integer>(dict->get("foo"))->i());
  }
  {
    // dict, size 2
    std::string src = "d3:fooi123e3:bar1:ee";
    auto d = parseValue(parser, src);
    Dict* dict = downcast<Dict>(d);
    CPPUNIT_ASSERT(dict);
    CPPUNIT_ASSERT_EQUAL((size_t)2, dict->size());
    CPPUNIT_ASSERT(dict->get("foo"));
    CPPUNIT_ASSERT_EQUAL((int64_t)123,
                         downcast<Integer>(dict->get("foo"))->i());
    CPPUNIT_ASSERT(dict->get("bar"));
    CPPUNIT_ASSERT_EQUAL(std::string("e"),
                         downcast<String>(dict->get("bar"))->s());
  }
  {
    // list, size 1
    std::string src = "l3:fooe";
    auto l = parseValue(parser, src);
    List* list = downcast<List>(l);
    CPPUNIT_ASSERT(list);
    CPPUNIT_ASSERT_EQUAL((size_t)1, list->size());
    CPPUNIT_ASSERT_EQUAL(std::string("foo"),
                         downcast<String>(list->get(0))->s());
  }
  {
    // list, size 2
    std::string src = "l3:fooi123ee";
    auto l = parseValue(parser, src);
    List* list = downcast<List>(l);
    CPPUNIT_ASSERT(list);
    CPPUNIT_ASSERT_EQUAL((size_t)2, list->size());
    CPPUNIT_ASSERT_EQUAL(std::string("foo"),
                         downcast<String>(list->get(0))->s());
    CPPUNIT_ASSERT_EQUAL((int64_t)123, downcast<Integer>(list->get(1))->i());
  }
  {
    // string, integer and list in dict
    std::string src = "d4:name5:aria24:sizei12345678900e5:filesl3:bin3:docee";
    auto r = parseValue(parser, src);
    const Dict* dict = downcast<Dict>(r);
    CPPUNIT_ASSERT(dict);
    CPPUNIT_ASSERT_EQUAL(std::string("aria2"),
                         downcast<String>(dict->get("name"))->s());
    CPPUNIT_ASSERT_EQUAL(static_cast<Integer::ValueType>(12345678900LL),
                         downcast<Integer>(dict->get("size"))->i());
    const List* list = downcast<List>(dict->get("files"));
    CPPUNIT_ASSERT(list);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), list->size());
    CPPUNIT_ASSERT_EQUAL(std::string("bin"),
                         downcast<String>(list->get(0))->s());
    CPPUNIT_ASSERT_EQUAL(std::string("doc"),
                         downcast<String>(list->get(1))->s());
  }
  {
    // dict in list
    std::string src = "ld1:ki123eee";
    auto r = parseValue(parser, src);
    const List* list = downcast<List>(r);
    CPPUNIT_ASSERT(list);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), list->size());
    const Dict* dict = downcast<Dict>(list->get(0));
    CPPUNIT_ASSERT(dict);
    CPPUNIT_ASSERT_EQUAL(static_cast<Integer::ValueType>(123),
                         downcast<Integer>(dict->get("k"))->i());
  }
  {
    // empty key is allowed
    std::string src = "d0:1:ve";
    auto s = parseValue(parser, src);
  }
  {
    // empty encoded data
    std::string src = "";
    auto s = parseValue(parser, src);
    CPPUNIT_ASSERT(!s);
  }
  // integer, without ending 'e'
  checkDecodeError("i3");
  // dict, without ending 'e'
  checkDecodeError("d");
  // list, without ending 'e'
  checkDecodeError("l");
  // string, less than the specified length.
  checkDecodeError("3:ab");
  // string, but length is invalid
  checkDecodeError("x:abc");
  // string with minus length
  checkDecodeError("-1:a");
  // too deep structure
  checkDecodeError(std::string(51, 'l') + std::string(51, 'e'));
  checkDecodeError(std::string(50, 'l') + "d3:fooi100ee" +
                   std::string(50, 'e'));
  // float number, but including bad characters
  checkDecodeError("i-1.134a+33e");
  checkDecodeError("ixe");
  // empty number
  checkDecodeError("ie");
  {
    // ignore trailing garbage at the end of the input.
    std::string src = "5:aria2trail";
    auto res = parser.parseFinal(std::as_bytes(std::span{src}));
    CPPUNIT_ASSERT(res);
    // Get trailing garbage position
    CPPUNIT_ASSERT_EQUAL((size_t)7, parser.getConsumed());
    CPPUNIT_ASSERT_EQUAL(std::string("aria2"), downcast<String>(*res)->s());
  }
  {
    // dict, empty member name
    std::string src = "d0:i123ee";
    auto d = parseValue(parser, src);
    Dict* dict = downcast<Dict>(d);
    CPPUNIT_ASSERT(dict);
    CPPUNIT_ASSERT(dict->get(""));
    CPPUNIT_ASSERT_EQUAL((int64_t)123, downcast<Integer>(dict->get(""))->i());
  }
}

} // namespace aria2
