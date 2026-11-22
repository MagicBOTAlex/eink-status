#include "monitor/config.hpp"

#include <cctype>
#include <cstdio>

namespace monitor {
namespace {

bool starts_with(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool valid_ipv4(const std::string &address) {
  int octets = 0;
  size_t i = 0;
  while (i < address.size() && octets < 4) {
    int value = 0;
    int digits = 0;
    while (i < address.size() && std::isdigit(static_cast<unsigned char>(address[i]))) {
      value = value * 10 + (address[i] - '0');
      ++digits;
      if (digits > 3 || value > 255) {
        return false;
      }
      ++i;
    }
    if (digits == 0) {
      return false;
    }
    ++octets;
    if (octets < 4) {
      if (i >= address.size() || address[i] != '.') {
        return false;
      }
      ++i;
    }
  }
  return octets == 4 && i == address.size();
}

} // namespace

Config default_config() {
  Config config;
  config.probe_seconds = 30;

  const struct {
    Kind kind;
    const char *label;
    const char *address;
    bool expect_ok_body;
  } defaults[] = {
      {Kind::Http, "webui", "https://webui.deprived.dev/health", true},
      {Kind::Http, "akupunktur", "https://www.akupunktur-herlev.dk/", false},
      {Kind::Http, "ha", "https://ha.deprived.dev/", false},
      {Kind::Http, "deprived", "https://deprived.dev/", false},
      {Kind::Ping, "botserver", "192.168.50.82", false},
      {Kind::Ping, "metal", "192.168.50.59", false},
      {Kind::Ping, "botmain", "192.168.50.58", false},
  };

  for (const auto &entry : defaults) {
    if (config.count >= kMaxTargets) {
      break;
    }
    TargetConfig &target = config.targets[config.count++];
    target.kind = entry.kind;
    target.label = entry.label;
    target.address = entry.address;
    target.expect_ok_body = entry.expect_ok_body;
  }
  return config;
}

bool valid_address(Kind kind, const std::string &address) {
  if (address.empty() || address.size() > kMaxAddressLength) {
    return false;
  }
  if (kind == Kind::Ping) {
    return valid_ipv4(address);
  }
  if (!starts_with(address, "http://") && !starts_with(address, "https://")) {
    return false;
  }
  // Reject the scheme alone and a scheme with no host.
  const size_t scheme = address.find("://") + 3;
  return address.size() > scheme;
}

bool validate_config(Config &config, std::string &error) {
  if (config.count < 1) {
    error = "at least one target is required";
    return false;
  }
  if (config.count > kMaxTargets) {
    error = "at most " + std::to_string(kMaxTargets) + " targets are supported";
    return false;
  }

  if (config.probe_seconds < kMinProbeSeconds) {
    config.probe_seconds = kMinProbeSeconds;
  }
  if (config.probe_seconds > kMaxProbeSeconds) {
    config.probe_seconds = kMaxProbeSeconds;
  }

  for (int i = 0; i < config.count; ++i) {
    TargetConfig &target = config.targets[i];
    const std::string index = std::to_string(i + 1);

    if (target.label.empty()) {
      error = "target " + index + ": label is required";
      return false;
    }
    if (target.label.size() > kMaxLabelLength) {
      error = "target " + index + ": label is longer than " +
              std::to_string(kMaxLabelLength);
      return false;
    }
    if (!valid_address(target.kind, target.address)) {
      error = "target " + index + ": address is not valid for " +
              to_string(target.kind);
      return false;
    }
    for (int other = 0; other < i; ++other) {
      if (config.targets[other].label == target.label) {
        error = "target " + index + ": duplicate label '" + target.label + "'";
        return false;
      }
    }
    if (target.kind == Kind::Ping) {
      target.expect_ok_body = false; // meaningless for ICMP
    }
  }

  return true;
}

const char *to_string(Kind kind) {
  return kind == Kind::Ping ? "ping" : "http";
}

bool kind_from_string(const std::string &text, Kind *kind) {
  if (text == "http") {
    *kind = Kind::Http;
    return true;
  }
  if (text == "ping") {
    *kind = Kind::Ping;
    return true;
  }
  return false;
}

} // namespace monitor
