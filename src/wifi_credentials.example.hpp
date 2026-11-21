// Copy to src/wifi_credentials.hpp (gitignored) and fill in your networks.
#pragma once

#include <cstddef>

#include "wifi_seeds.hpp"

namespace netwifi {
inline constexpr Seed kSeeds[] = {
    {"your-ssid", "your-password"},
};
inline constexpr size_t kSeedCount = sizeof(kSeeds) / sizeof(kSeeds[0]);
} // namespace netwifi
