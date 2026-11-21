#pragma once

#include <cstddef>

namespace netwifi {

struct Seed {
  const char *ssid;
  const char *password;
};

} // namespace netwifi

// Credentials live in src/wifi_credentials.hpp, which is gitignored. Copy
// src/wifi_credentials.example.hpp to get a working bench build; without it
// the firmware compiles and runs with no networks to try.
#if defined(__has_include)
#if __has_include("wifi_credentials.hpp")
#include "wifi_credentials.hpp"
#define EINK_HAS_WIFI_CREDENTIALS 1
#endif
#endif

#ifndef EINK_HAS_WIFI_CREDENTIALS
namespace netwifi {
inline constexpr Seed kSeeds[] = {{nullptr, nullptr}};
inline constexpr size_t kSeedCount = 0;
} // namespace netwifi
#endif
