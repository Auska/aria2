#include <array>
#include <span>

#include "ByteArrayDiskWriter.h"
#include <string>
#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

class ByteArrayDiskWriterTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(ByteArrayDiskWriterTest);
  CPPUNIT_TEST(testWriteAndRead);
  CPPUNIT_TEST(testWriteAndRead2);
  CPPUNIT_TEST_SUITE_END();

private:
public:
  void setUp() {}

  void testWriteAndRead();
  void testWriteAndRead2();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ByteArrayDiskWriterTest);

void ByteArrayDiskWriterTest::testWriteAndRead()
{
  ByteArrayDiskWriter bw;

  std::string msg1 = "Hello";
  bw.writeData(std::as_bytes(std::span{msg1}), 0);
  // write at the end of stream
  std::string msg2 = " World";
  bw.writeData(std::as_bytes(std::span{msg2}), 5);
  // write at the end of stream +1
  std::string msg3 = "!!";
  bw.writeData(std::as_bytes(std::span{msg3}), 12);
  // write space at the 'hole'
  std::string msg4 = " ";
  bw.writeData(std::as_bytes(std::span{msg4}), 11);

  std::array<std::byte, 100> buf;
  int32_t c = bw.readData(std::span{buf}, 1);
  buf[c] = std::byte{'\0'};

  CPPUNIT_ASSERT_EQUAL(
      std::string("ello World !!"),
      std::string(reinterpret_cast<const char*>(buf.data())));
  CPPUNIT_ASSERT_EQUAL((int64_t)14, bw.size());
}

void ByteArrayDiskWriterTest::testWriteAndRead2()
{
  ByteArrayDiskWriter bw;

  std::string msg1 = "Hello World";
  bw.writeData(std::as_bytes(std::span{msg1}), 0);
  std::string msg2 = "From Mars";
  bw.writeData(std::as_bytes(std::span{msg2}), 6);

  std::array<std::byte, 100> buf;
  int32_t c = bw.readData(std::span{buf}, 0);
  buf[c] = std::byte{'\0'};

  CPPUNIT_ASSERT_EQUAL(
      std::string("Hello From Mars"),
      std::string(reinterpret_cast<const char*>(buf.data())));
  CPPUNIT_ASSERT_EQUAL((int64_t)15, bw.size());
}

} // namespace aria2
