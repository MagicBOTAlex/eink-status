#include <unity.h>

#include "control/commands.hpp"

void setUp(void) {}
void tearDown(void) {}

void test_post_and_take(void) {
  control::Queue queue;
  TEST_ASSERT_FALSE(queue.pending());

  control::Command command;
  command.action = control::Action::Refresh;
  queue.post(command);
  TEST_ASSERT_TRUE(queue.pending());

  const control::Command taken = queue.take();
  TEST_ASSERT_EQUAL_INT((int)control::Action::Refresh, (int)taken.action);
  TEST_ASSERT_FALSE(queue.pending());

  // Taking again yields nothing: a command is consumed once.
  TEST_ASSERT_EQUAL_INT((int)control::Action::None,
                        (int)queue.take().action);
}

void test_latest_request_wins(void) {
  control::Queue queue;

  control::Command refresh;
  refresh.action = control::Action::Refresh;
  queue.post(refresh);

  control::Command reboot;
  reboot.action = control::Action::Reboot;
  queue.post(reboot);

  TEST_ASSERT_EQUAL_INT((int)control::Action::Reboot,
                        (int)queue.take().action);
  TEST_ASSERT_FALSE(queue.pending());
}

void test_page_command_carries_name(void) {
  control::Queue queue;

  control::Command page;
  page.action = control::Action::ShowPage;
  page.page = "status";
  queue.post(page);

  const control::Command taken = queue.take();
  TEST_ASSERT_EQUAL_INT((int)control::Action::ShowPage, (int)taken.action);
  TEST_ASSERT_EQUAL_STRING("status", taken.page.c_str());
  // The slot is cleared, including the page string.
  TEST_ASSERT_EQUAL_STRING("", queue.take().page.c_str());
}

void test_shared_queue(void) {
  // The web server and the main loop share one instance.
  control::Command probe;
  probe.action = control::Action::ProbeNow;
  control::queue().post(probe);

  TEST_ASSERT_EQUAL_INT((int)control::Action::ProbeNow,
                        (int)control::queue().take().action);
  TEST_ASSERT_FALSE(control::queue().pending());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_post_and_take);
  RUN_TEST(test_latest_request_wins);
  RUN_TEST(test_page_command_carries_name);
  RUN_TEST(test_shared_queue);
  return UNITY_END();
}
