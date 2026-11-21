#pragma once

#include <cstddef>

namespace netwifi {

struct Status {
  bool connected;
  char ssid[33];
  char ip[16];
};

// Brings up NVS, esp-netif, the default STA netif and the WiFi driver, then
// starts a task that associates with the first reachable seed network. No
// application network services are started (no HTTP, OTA or SNTP).
void start();

Status get_status();

} // namespace netwifi
