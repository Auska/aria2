#include "SessionSerializer.h"

#include <iostream>
#include <fstream>

#include <cppunit/extensions/HelperMacros.h>

#include "TestUtil.h"
#include "RequestGroupMan.h"
#include "download_helper.h"
#include "prefs.h"
#include "Option.h"
#include "a2functional.h"
#include "FileEntry.h"
#include "SelectEventPoll.h"
#include "DownloadEngine.h"

namespace aria2 {

class SessionSerializerTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(SessionSerializerTest);
  CPPUNIT_TEST(testSaveErrorDownload);
  CPPUNIT_TEST_SUITE_END();

public:
  void testSaveErrorDownload();
};

CPPUNIT_TEST_SUITE_REGISTRATION(SessionSerializerTest);


void SessionSerializerTest::testSaveErrorDownload()
{
  std::shared_ptr<DownloadResult> dr =
      createDownloadResult(error_code::Value::TIME_OUT, "http://error");
  dr->fileEntries[0]->getSpentUris().swap(
      dr->fileEntries[0]->getRemainingUris());
  std::shared_ptr<Option> option(new Option());
  option->put(PREF_MAX_DOWNLOAD_RESULT, "10");
  RequestGroupMan rgman{std::vector<std::shared_ptr<RequestGroup>>(), 1,
                        option.get()};
  rgman.addDownloadResult(dr);
  SessionSerializer s(&rgman);
  std::string filename =
      A2_TEST_OUT_DIR "/aria2_SessionSerializerTest_testSaveErrorDownload";
  CPPUNIT_ASSERT(s.save(filename));
  std::ifstream ss(filename.c_str(), std::ios::binary);
  std::string line;
  std::getline(ss, line);
  CPPUNIT_ASSERT_EQUAL(std::string("http://error\t"), line);
}

} // namespace aria2
