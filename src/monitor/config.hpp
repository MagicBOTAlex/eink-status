#pragma once

#include <cstddef>
#include <string>

#include "monitor/model.hpp"

// Runtime configuration: what the monitor checks and how often. Editable from
// the web UI, persisted in NVS, validated here so the API and the boot path
// agree on what is acceptable.
namespace monitor {

// Bumped when the default target list changes; a stored config with an older
// version is replaced by the new defaults instead of pinning the old list.
inline constexpr int kConfigVersion = 3;

inline constexpr int kMinProbeSeconds = 10;
inline constexpr int kMaxProbeSeconds = 3600;
inline constexpr size_t kMaxLabelLength = 12;
inline constexpr size_t kMaxAddressLength = 96;

struct TargetConfig {
  Kind kind = Kind::Http;
  std::string label;
  std::string address;
  bool expect_ok_body = false;
};

struct Config {
  TargetConfig targets[kMaxTargets];
  int count = 0;
  int probe_seconds = 30;
  int version = kConfigVersion;
  // Empty means "keep the resolvers DHCP gave us". Set them to reach zones
  // (like a private .internal) that a public resolver cannot answer.
  std::string dns_main;
  std::string dns_secondary;
};

// The bench defaults, including https://deprived.dev/.
Config default_config();

// True when `address` is a plausible endpoint for `kind`:
// Http wants http:// or https://, Ping wants a dotted quad.
bool valid_address(Kind kind, const std::string &address);

// Clamps the interval, rejects an empty/oversized list, empty or duplicate
// labels, over-long fields and unusable addresses. Returns false and fills
// `error` on rejection; on success the config is normalised in place.
bool validate_config(Config &config, std::string &error);

const char *to_string(Kind kind);
bool kind_from_string(const std::string &text, Kind *kind);

} // namespace monitor
