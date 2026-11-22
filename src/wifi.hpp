#pragma once

#include <cstddef>
#include <cstdint>

namespace netwifi {

struct Status {
  bool connected;
  char ssid[33];
  char ip[16];
  // Monotonic milliseconds (esp_timer) when the current association began,
  // 0 when not connected. Used for the "net up" counter on the monitor page.
  uint32_t connected_since_ms;
};

// Brings up NVS, esp-netif, the default STA netif and the WiFi driver, then
// starts a task that associates with the first reachable seed network. No
// application network services are started (no HTTP, OTA or SNTP).
void start();

Status get_status();

} // namespace netwifi
