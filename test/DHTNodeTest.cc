#include "DHTNode.h"
#include "Exception.h"
#include "util.h"
#include <iostream>
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class DHTNodeTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(DHTNodeTest);
  CPPUNIT_TEST(testGenerateID);
  CPPUNIT_TEST(testLessThan);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}

  void testGenerateID();

  void testLessThan();
};

CPPUNIT_TEST_SUITE_REGISTRATION(DHTNodeTest);

void DHTNodeTest::testGenerateID()
{
  DHTNode node;
  std::cerr << util::toHex(std::as_bytes(std::span{node.getID()})) << std::endl;
}

void DHTNodeTest::testLessThan()
{
  DHTID a;
  a.fill(0);
  DHTID b;
  b.fill(0);
  b[0] = 1;
  DHTNode nodeA(a);
  DHTNode nodeB(b);

  CPPUNIT_ASSERT(nodeA < nodeB);
  CPPUNIT_ASSERT(!(nodeB < nodeA));
  // DHTNode::operator< returns true for identical IDs; this is the
  // historical (non-strict-weak-ordering) behaviour and is kept as is.
  CPPUNIT_ASSERT(nodeA < nodeA);
}

} // namespace aria2
