#include <unity.h>

#include <string>

#include "monitor/config.hpp"
#include "monitor/model.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

int index_of(const monitor::Config &config, const std::string &label) {
  for (int i = 0; i < config.count; ++i) {
    if (config.targets[i].label == label) {
      return i;
    }
  }
  return -1;
}

} // namespace

void test_default_config_covers_the_bench(void) {
  monitor::Config config = monitor::default_config();

  TEST_ASSERT_EQUAL_INT(monitor::kMaxTargets, config.count);
  TEST_ASSERT_EQUAL_INT(30, config.probe_seconds);

  const char *labels[] = {"webui", "akupunktur", "ha", "deprived",
                          "botserver", "metal", "botmain"};
  for (const char *label : labels) {
    TEST_ASSERT_TRUE(index_of(config, label) >= 0);
  }

  const int health = index_of(config, "webui");
  TEST_ASSERT_EQUAL_INT((int)monitor::Kind::Http, (int)config.targets[health].kind);
  TEST_ASSERT_TRUE(config.targets[health].expect_ok_body);

  const int deprived = index_of(config, "deprived");
  TEST_ASSERT_EQUAL_STRING("https://deprived.dev/",
                           config.targets[deprived].address.c_str());

  const int botserver = index_of(config, "botserver");
  TEST_ASSERT_EQUAL_INT((int)monitor::Kind::Ping,
                        (int)config.targets[botserver].kind);

  std::string error;
  TEST_ASSERT_TRUE(monitor::validate_config(config, error));
  TEST_ASSERT_TRUE(error.empty());
}

void test_validate_rejects_bad_configs(void) {
  std::string error;

  monitor::Config empty;
  empty.count = 0;
  TEST_ASSERT_FALSE(monitor::validate_config(empty, error));
  TEST_ASSERT_TRUE(error.find("at least one") != std::string::npos);

  monitor::Config too_many = monitor::default_config();
  too_many.count = monitor::kMaxTargets + 1;
  TEST_ASSERT_FALSE(monitor::validate_config(too_many, error));

  monitor::Config no_label = monitor::default_config();
  no_label.targets[0].label.clear();
  TEST_ASSERT_FALSE(monitor::validate_config(no_label, error));
  TEST_ASSERT_TRUE(error.find("label is required") != std::string::npos);

  monitor::Config long_label = monitor::default_config();
  long_label.targets[0].label = "0123456789abc"; // 13 > kMaxLabelLength
  TEST_ASSERT_FALSE(monitor::validate_config(long_label, error));

  monitor::Config duplicate = monitor::default_config();
  duplicate.targets[1].label = duplicate.targets[0].label;
  TEST_ASSERT_FALSE(monitor::validate_config(duplicate, error));
  TEST_ASSERT_TRUE(error.find("duplicate") != std::string::npos);

  monitor::Config bad_ping = monitor::default_config();
  bad_ping.targets[4].address = "192.168.50.999";
  TEST_ASSERT_FALSE(monitor::validate_config(bad_ping, error));
  TEST_ASSERT_TRUE(error.find("not valid for ping") != std::string::npos);

  monitor::Config bad_http = monitor::default_config();
  bad_http.targets[0].address = "192.168.50.1";
  TEST_ASSERT_FALSE(monitor::validate_config(bad_http, error));
  TEST_ASSERT_TRUE(error.find("not valid for http") != std::string::npos);
}

void test_validate_clamps_interval_and_normalises(void) {
  std::string error;

  monitor::Config fast = monitor::default_config();
  fast.probe_seconds = 1;
  TEST_ASSERT_TRUE(monitor::validate_config(fast, error));
  TEST_ASSERT_EQUAL_INT(monitor::kMinProbeSeconds, fast.probe_seconds);

  monitor::Config slow = monitor::default_config();
  slow.probe_seconds = 99999;
  TEST_ASSERT_TRUE(monitor::validate_config(slow, error));
  TEST_ASSERT_EQUAL_INT(monitor::kMaxProbeSeconds, slow.probe_seconds);

  // expect_ok_body is meaningless for ICMP and gets cleared.
  monitor::Config ping = monitor::default_config();
  ping.targets[4].expect_ok_body = true;
  TEST_ASSERT_TRUE(monitor::validate_config(ping, error));
  TEST_ASSERT_FALSE(ping.targets[4].expect_ok_body);
}

void test_valid_address(void) {
  using monitor::Kind;
  TEST_ASSERT_TRUE(monitor::valid_address(Kind::Http, "https://deprived.dev/"));
  TEST_ASSERT_TRUE(monitor::valid_address(Kind::Http, "http://x"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Http, "https://"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Http, "deprived.dev"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Http, ""));

  TEST_ASSERT_TRUE(monitor::valid_address(Kind::Ping, "192.168.50.82"));
  TEST_ASSERT_TRUE(monitor::valid_address(Kind::Ping, "0.0.0.0"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Ping, "192.168.50.999"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Ping, "1.2.3"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Ping, "1.2.3.4.5"));
  TEST_ASSERT_FALSE(monitor::valid_address(Kind::Ping, "botserver"));
}

void test_kind_strings(void) {
  monitor::Kind kind = monitor::Kind::Http;
  TEST_ASSERT_TRUE(monitor::kind_from_string("ping", &kind));
  TEST_ASSERT_EQUAL_INT((int)monitor::Kind::Ping, (int)kind);
  TEST_ASSERT_TRUE(monitor::kind_from_string("http", &kind));
  TEST_ASSERT_EQUAL_INT((int)monitor::Kind::Http, (int)kind);
  TEST_ASSERT_FALSE(monitor::kind_from_string("icmp", &kind));
  TEST_ASSERT_EQUAL_STRING("http", monitor::to_string(monitor::Kind::Http));
  TEST_ASSERT_EQUAL_STRING("ping", monitor::to_string(monitor::Kind::Ping));
}

void test_tracker_keeps_streaks(void) {
  monitor::Tracker tracker;

  monitor::apply_result(tracker, true, 1000);
  TEST_ASSERT_TRUE(tracker.known);
  TEST_ASSERT_TRUE(tracker.up);
  TEST_ASSERT_EQUAL_UINT32(1000, tracker.since_ms);

  monitor::apply_result(tracker, true, 5000);
  TEST_ASSERT_EQUAL_UINT32(1000, tracker.since_ms); // same state: streak grows

  monitor::apply_result(tracker, false, 9000);
  TEST_ASSERT_FALSE(tracker.up);
  TEST_ASSERT_EQUAL_UINT32(9000, tracker.since_ms); // state changed: reset

  monitor::apply_result(tracker, false, 12000);
  TEST_ASSERT_EQUAL_UINT32(9000, tracker.since_ms);
}

void test_status_text(void) {
  monitor::TargetStatus status;
  TEST_ASSERT_EQUAL_STRING("?", monitor::status_text(status));
  status.known = true;
  status.up = true;
  TEST_ASSERT_EQUAL_STRING("UP", monitor::status_text(status));
  status.up = false;
  TEST_ASSERT_EQUAL_STRING("DOWN", monitor::status_text(status));
}

void test_formatting(void) {
  TEST_ASSERT_EQUAL_STRING("--", monitor::format_latency(-1).c_str());
  TEST_ASSERT_EQUAL_STRING("12ms", monitor::format_latency(12).c_str());
  TEST_ASSERT_EQUAL_STRING("9999+", monitor::format_latency(12345).c_str());

  TEST_ASSERT_EQUAL_STRING("0h00m", monitor::format_uptime(0).c_str());
  TEST_ASSERT_EQUAL_STRING("0h00m", monitor::format_uptime(59000).c_str());
  TEST_ASSERT_EQUAL_STRING("0h01m", monitor::format_uptime(65000).c_str());
  TEST_ASSERT_EQUAL_STRING("1h01m", monitor::format_uptime(3661000).c_str());
  TEST_ASSERT_EQUAL_STRING("2d04h", monitor::format_uptime(187200000).c_str());

  // Wrap-safe: 0x10 ms after 0xFFFFFF00 is 0x110 ms.
  TEST_ASSERT_EQUAL_STRING("0h00m",
                           monitor::format_since(0x00000110u, 0xFFFFFF00u).c_str());

  TEST_ASSERT_EQUAL_STRING("just now", monitor::format_ago(1000).c_str());
  TEST_ASSERT_EQUAL_STRING("just now", monitor::format_ago(59000).c_str());
  TEST_ASSERT_EQUAL_STRING("1m ago", monitor::format_ago(90000).c_str());
  TEST_ASSERT_EQUAL_STRING("3h ago", monitor::format_ago(3 * 3600 * 1000).c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_default_config_covers_the_bench);
  RUN_TEST(test_validate_rejects_bad_configs);
  RUN_TEST(test_validate_clamps_interval_and_normalises);
  RUN_TEST(test_valid_address);
  RUN_TEST(test_kind_strings);
  RUN_TEST(test_tracker_keeps_streaks);
  RUN_TEST(test_status_text);
  RUN_TEST(test_formatting);
  return UNITY_END();
}
