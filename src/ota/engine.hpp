#pragma once

#include <cstdint>
#include <string>

#include "ota/session.hpp"

namespace ota {

struct Info {
  bool available = false;
  State state = State::Idle;
  uint32_t offset = 0;
  uint32_t total = 0;
  uint32_t capacity = 0;
  char partition[17] = {}; // inactive slot, target of the next update
  char running[17] = {};   // slot this firmware booted from
  char sha256[kSha256Hex + 1] = {};
  char error[128] = {};
};

// Selects the inactive slot, loads any interrupted session from NVS and makes
// the engine callable from both the HTTP and the BLE task.
void init();

Info info();
bool begin(uint32_t total, const std::string &sha256, bool restart,
           uint32_t &offset, std::string &error);

// Writes at an explicit offset (HTTP) or at the session cursor (BLE). Both
// paths share one mutex; a transport that falls behind gets an offset mismatch
// instead of corrupting the image.
bool append(uint32_t offset, const uint8_t *data, uint32_t length,
            std::string &error);
bool append_next(const uint8_t *data, uint32_t length, std::string &error);

bool finish(std::string &error);
bool abort(std::string &error);

uint32_t offset();
uint32_t total();
bool active();

} // namespace ota
