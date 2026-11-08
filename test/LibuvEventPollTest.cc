#include "LibuvEventPoll.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cppunit/extensions/HelperMacros.h>

#include "Command.h"

namespace aria2 {

namespace {

class FakeCommand : public Command {
public:
  FakeCommand() : Command(1) {}

  bool execute() override { return true; }

  bool readEnabled() const { return readEventEnabled(); }

  bool writeEnabled() const { return writeEventEnabled(); }
};

timeval makeTv(long sec, long usec)
{
  timeval tv;
  tv.tv_sec = sec;
  tv.tv_usec = usec;
  return tv;
}

} // namespace

class LibuvEventPollTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(LibuvEventPollTest);
  CPPUNIT_TEST(testReadEvent);
  CPPUNIT_TEST(testWriteEvent);
  CPPUNIT_TEST(testDeletedEventIsNotNotified);
  CPPUNIT_TEST(testPollWithoutRegistration);
  CPPUNIT_TEST(testClosedSocketIsRejected);
  CPPUNIT_TEST_SUITE_END();

public:
  void testReadEvent();
  void testWriteEvent();
  void testDeletedEventIsNotNotified();
  void testPollWithoutRegistration();
  void testClosedSocketIsRejected();
};

CPPUNIT_TEST_SUITE_REGISTRATION(LibuvEventPollTest);

void LibuvEventPollTest::testReadEvent()
{
  sock_t sv[2];
  CPPUNIT_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  LibuvEventPoll eventPoll;
  CPPUNIT_ASSERT(eventPoll.good());

  FakeCommand command;
  CPPUNIT_ASSERT(
      eventPoll.addEvents(sv[0], &command, EventPoll::EventType::READ));

  char c = 'x';
  CPPUNIT_ASSERT(write(sv[1], &c, 1) == 1);
  eventPoll.poll(makeTv(0, 100000));

  CPPUNIT_ASSERT(command.readEnabled());
  CPPUNIT_ASSERT(!command.writeEnabled());

  eventPoll.deleteEvents(sv[0], &command, EventPoll::EventType::READ);
  close(sv[0]);
  close(sv[1]);
}

void LibuvEventPollTest::testWriteEvent()
{
  sock_t sv[2];
  CPPUNIT_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  LibuvEventPoll eventPoll;
  CPPUNIT_ASSERT(eventPoll.good());

  FakeCommand command;
  CPPUNIT_ASSERT(
      eventPoll.addEvents(sv[0], &command, EventPoll::EventType::WRITE));
  eventPoll.poll(makeTv(0, 100000));

  CPPUNIT_ASSERT(command.writeEnabled());

  eventPoll.deleteEvents(sv[0], &command, EventPoll::EventType::WRITE);
  close(sv[0]);
  close(sv[1]);
}

void LibuvEventPollTest::testDeletedEventIsNotNotified()
{
  sock_t sv[2];
  CPPUNIT_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  LibuvEventPoll eventPoll;
  CPPUNIT_ASSERT(eventPoll.good());

  FakeCommand command;
  CPPUNIT_ASSERT(
      eventPoll.addEvents(sv[0], &command, EventPoll::EventType::READ));
  CPPUNIT_ASSERT(
      eventPoll.deleteEvents(sv[0], &command, EventPoll::EventType::READ));

  char c = 'x';
  CPPUNIT_ASSERT(write(sv[1], &c, 1) == 1);
  eventPoll.poll(makeTv(0, 50000));

  CPPUNIT_ASSERT(!command.readEnabled());

  close(sv[0]);
  close(sv[1]);
}

void LibuvEventPollTest::testPollWithoutRegistration()
{
  LibuvEventPoll eventPoll;
  CPPUNIT_ASSERT(eventPoll.good());

  // No handle registered: must still return once the timeout expires.
  eventPoll.poll(makeTv(0, 20000));
  eventPoll.poll(makeTv(0, 0));
}

void LibuvEventPollTest::testClosedSocketIsRejected()
{
  sock_t sv[2];
  CPPUNIT_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  close(sv[0]);

  LibuvEventPoll eventPoll;
  CPPUNIT_ASSERT(eventPoll.good());

  FakeCommand command;
  CPPUNIT_ASSERT(
      !eventPoll.addEvents(sv[0], &command, EventPoll::EventType::READ));

  close(sv[1]);
}

} // namespace aria2
