#pragma once

#include <cstdint>

// BLE OTA transport: a small GATT service that feeds the same resumable
// session as the HTTP uploader. It is deliberately independent of WiFi, so
// firmware can still be replaced when the network is broken.
namespace ota::ble {

struct Info {
  bool enabled = false;
  bool connected = false;
  uint16_t mtu = 23;
  const char *name = "eink-status-ota";
  const char *service_uuid = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0001";
  const char *control_uuid = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0002";
  const char *data_uuid = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0003";
  const char *status_uuid = "e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0004";
};

void start();
Info info();

} // namespace ota::ble
