#pragma once

#include <string>

#include "monitor/config.hpp"
#include "monitor/model.hpp"

namespace monitor {

// Loads the saved configuration (or the defaults), publishes an initial
// snapshot and starts the probe task. Call once, after netwifi::start().
void start();

// Thread-safe copies for the UI and the web API.
Snapshot snapshot();
Config config();

std::string config_json();
std::string snapshot_json();

// Validates, stores in NVS and wakes the prober to re-probe immediately.
// Returns false and fills `error` when the configuration is rejected.
bool set_config(const Config &config, std::string &error);
bool set_config_json(const std::string &json, std::string &error);

} // namespace monitor
