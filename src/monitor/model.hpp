#pragma once

#include <cstdint>
#include <string>

// Platform-free model for the network monitor: what a probe result means, how
// a streak is tracked, and how those numbers are formatted. The ESP-IDF prober
// (monitor/prober.cpp) fills it in; the page, the web API and the host tests
// consume it.
namespace monitor {

enum class Kind : uint8_t { Http, Ping };

// Fits the 480x648 table (fourteen 36 px rows at 24 px type).
inline constexpr int kMaxTargets = 14;

struct TargetStatus {
  std::string label;
  Kind kind = Kind::Http;
  bool known = false;     // at least one probe has landed
  bool up = false;
  int latency_ms = -1;    // -1 when unknown/down
  uint32_t since_ms = 0;  // monotonic time the current state began
};

struct Snapshot {
  TargetStatus targets[kMaxTargets];
  int count = 0;
  bool wifi_connected = false;
  uint32_t connected_since_ms = 0;
  char ip[16] = {};
  uint32_t checked_ms = 0;  // last completed round, 0 before the first one
  int up_count = 0;
  int known_count = 0;
  int probe_seconds = 30;
};

// Tracks one target's current streak. Kept separate from the network code so
// the transition rules can be tested without a socket.
struct Tracker {
  bool known = false;
  bool up = false;
  uint32_t since_ms = 0;
};

void apply_result(Tracker &tracker, bool up, uint32_t now_ms);

const char *status_text(const TargetStatus &status); // "UP" / "DOWN" / "?"

// "12ms", "--" when unknown.
std::string format_latency(int latency_ms);

// Compact duration: "0h05m", "13h42m", "2d04h". Never more than 6 characters,
// because the row layout reserves a fixed column.
std::string format_uptime(uint32_t ms);
std::string format_since(uint32_t now_ms, uint32_t since_ms);

// "just now" / "12s ago" / "3m ago" -- used for the last probe marker.
std::string format_ago(uint32_t ms);

} // namespace monitor
