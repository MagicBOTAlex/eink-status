#include "monitor/model.hpp"

#include <cstdio>

namespace monitor {

void apply_result(Tracker &tracker, bool up, uint32_t now_ms) {
  if (!tracker.known || tracker.up != up) {
    tracker.known = true;
    tracker.up = up;
    tracker.since_ms = now_ms;
  }
  // Same state: keep the original start time so the streak keeps growing.
}

const char *status_text(const TargetStatus &status) {
  if (!status.known) {
    return "?";
  }
  return status.up ? "UP" : "DOWN";
}

std::string format_latency(int latency_ms) {
  if (latency_ms < 0) {
    return "--";
  }
  char buf[16];
  if (latency_ms > 9999) {
    return "9999+";
  }
  std::snprintf(buf, sizeof(buf), "%dms", latency_ms);
  return buf;
}

std::string format_uptime(uint32_t ms) {
  const uint32_t total_s = ms / 1000;
  const uint32_t days = total_s / 86400;
  const uint32_t hours = (total_s % 86400) / 3600;
  const uint32_t minutes = (total_s % 3600) / 60;

  char buf[24];
  if (days > 0) {
    std::snprintf(buf, sizeof(buf), "%lud%02luh", (unsigned long)days,
                  (unsigned long)hours);
  } else {
    std::snprintf(buf, sizeof(buf), "%luh%02lum", (unsigned long)hours,
                  (unsigned long)minutes);
  }
  return buf;
}

std::string format_since(uint32_t now_ms, uint32_t since_ms) {
  // Wrap-safe subtraction: both values come from the same monotonic clock.
  return format_uptime(static_cast<uint32_t>(now_ms - since_ms));
}

std::string format_ago(uint32_t ms) {
  const uint32_t total_s = ms / 1000;
  char buf[24];
  // Minute granularity at most: an "Xs ago" label would otherwise repaint the
  // panel every second.
  if (total_s < 60) {
    return "just now";
  }
  if (total_s < 3600) {
    std::snprintf(buf, sizeof(buf), "%lum ago", (unsigned long)(total_s / 60));
    return buf;
  }
  std::snprintf(buf, sizeof(buf), "%luh ago", (unsigned long)(total_s / 3600));
  return buf;
}

} // namespace monitor
