#include "monitor/prober.hpp"

#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/ip_addr.h"
#include "nvs.h"
#include "ping/ping_sock.h"
#include "wifi.hpp"

namespace monitor {
namespace {

constexpr const char *TAG = "MONITOR";
constexpr const char *kNvsNamespace = "monitor";
constexpr const char *kNvsKey = "config";
constexpr EventBits_t kConfigChangedBit = BIT0;
constexpr uint32_t kOfflinePollMs = 2000;

SemaphoreHandle_t s_mutex = nullptr;
EventGroupHandle_t s_events = nullptr;
Config s_config;
Snapshot s_snapshot;
Tracker s_trackers[kMaxTargets];
uint32_t s_config_generation = 0;
uint32_t s_seen_generation = 0;

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

struct ProbeResult {
  bool ok = false;
  int latency_ms = -1;
};

// --- JSON -----------------------------------------------------------------

bool parse_target(const cJSON *item, TargetConfig &target) {
  if (!cJSON_IsObject(item)) {
    return false;
  }
  const cJSON *kind = cJSON_GetObjectItemCaseSensitive(item, "kind");
  const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
  const cJSON *address = cJSON_GetObjectItemCaseSensitive(item, "address");
  const cJSON *body = cJSON_GetObjectItemCaseSensitive(item, "expect_ok_body");

  if (!cJSON_IsString(kind) || !kind_from_string(kind->valuestring, &target.kind)) {
    return false;
  }
  if (!cJSON_IsString(label) || !cJSON_IsString(address)) {
    return false;
  }
  target.label = label->valuestring;
  target.address = address->valuestring;
  target.expect_ok_body = cJSON_IsBool(body) && cJSON_IsTrue(body);
  return true;
}

bool config_from_json(const char *text, Config &out) {
  cJSON *root = cJSON_Parse(text);
  if (root == nullptr) {
    return false;
  }

  bool ok = true;
  out = Config{};

  const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
  out.version = cJSON_IsNumber(version) ? version->valueint : 0;

  const cJSON *seconds = cJSON_GetObjectItemCaseSensitive(root, "probe_seconds");
  out.probe_seconds = cJSON_IsNumber(seconds) ? seconds->valueint : 30;

  const cJSON *dns_main = cJSON_GetObjectItemCaseSensitive(root, "dns_main");
  const cJSON *dns_secondary =
      cJSON_GetObjectItemCaseSensitive(root, "dns_secondary");
  if (cJSON_IsString(dns_main)) {
    out.dns_main = dns_main->valuestring;
  }
  if (cJSON_IsString(dns_secondary)) {
    out.dns_secondary = dns_secondary->valuestring;
  }

  const cJSON *targets = cJSON_GetObjectItemCaseSensitive(root, "targets");
  if (!cJSON_IsArray(targets)) {
    ok = false;
  } else {
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, targets) {
      if (out.count >= kMaxTargets ||
          !parse_target(item, out.targets[out.count])) {
        ok = false;
        break;
      }
      ++out.count;
    }
  }

  cJSON_Delete(root);
  return ok;
}

std::string config_to_json(const Config &config) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "version", config.version);
  cJSON_AddNumberToObject(root, "probe_seconds", config.probe_seconds);
  cJSON_AddStringToObject(root, "dns_main", config.dns_main.c_str());
  cJSON_AddStringToObject(root, "dns_secondary", config.dns_secondary.c_str());
  cJSON *targets = cJSON_AddArrayToObject(root, "targets");
  for (int i = 0; i < config.count; ++i) {
    const TargetConfig &target = config.targets[i];
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "kind", to_string(target.kind));
    cJSON_AddStringToObject(item, "label", target.label.c_str());
    cJSON_AddStringToObject(item, "address", target.address.c_str());
    cJSON_AddBoolToObject(item, "expect_ok_body", target.expect_ok_body);
    cJSON_AddItemToArray(targets, item);
  }
  char *text = cJSON_PrintUnformatted(root);
  const std::string json = text != nullptr ? text : "{}";
  cJSON_free(text);
  cJSON_Delete(root);
  return json;
}

// --- persistence ----------------------------------------------------------

bool save_config(const Config &config) {
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return false;
  }
  const std::string json = config_to_json(config);
  esp_err_t err = nvs_set_str(handle, kNvsKey, json.c_str());
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "could not save config: %s", esp_err_to_name(err));
  }
  return err == ESP_OK;
}

bool load_config(Config &out) {
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }
  size_t length = 0;
  if (nvs_get_str(handle, kNvsKey, nullptr, &length) != ESP_OK) {
    nvs_close(handle);
    return false;
  }
  std::string buffer(length, '\0');
  const bool ok = nvs_get_str(handle, kNvsKey, buffer.data(), &length) == ESP_OK;
  nvs_close(handle);
  if (!ok) {
    return false;
  }

  std::string error;
  if (!config_from_json(buffer.c_str(), out)) {
    ESP_LOGW(TAG, "stored config is unreadable; using defaults");
    return false;
  }
  if (!validate_config(out, error)) {
    ESP_LOGW(TAG, "stored config is invalid (%s); using defaults", error.c_str());
    return false;
  }
  if (out.version != kConfigVersion) {
    ESP_LOGI(TAG, "stored config is version %d (want %d); using new defaults",
             out.version, kConfigVersion);
    return false;
  }
  return true;
}

// --- DNS ------------------------------------------------------------------

// A private zone such as .internal lives on the LAN resolver, which may not be
// the one DHCP advertises. When the config names resolvers, pin them on the STA
// interface; otherwise leave whatever DHCP gave us alone.
void apply_dns_override(const Config &config) {
  if (config.dns_main.empty() && config.dns_secondary.empty()) {
    return;
  }
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    return;
  }

  auto set = [netif](esp_netif_dns_type_t type, const std::string &text) {
    if (text.empty()) {
      return;
    }
    esp_netif_dns_info_t info = {};
    info.ip.type = ESP_IPADDR_TYPE_V4;
    if (esp_netif_str_to_ip4(text.c_str(), &info.ip.u_addr.ip4) != ESP_OK) {
      return;
    }
    if (esp_netif_set_dns_info(netif, type, &info) != ESP_OK) {
      ESP_LOGW(TAG, "could not set DNS %s", text.c_str());
    }
  };

  set(ESP_NETIF_DNS_MAIN, config.dns_main);
  set(ESP_NETIF_DNS_BACKUP, config.dns_secondary);
}

void fill_dns(Snapshot &snapshot) {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) {
    return;
  }

  esp_netif_dns_info_t info = {};
  if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &info) == ESP_OK &&
      info.ip.type == ESP_IPADDR_TYPE_V4) {
    esp_ip4addr_ntoa(&info.ip.u_addr.ip4, snapshot.dns_main,
                     sizeof(snapshot.dns_main));
  }
  if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &info) == ESP_OK &&
      info.ip.type == ESP_IPADDR_TYPE_V4) {
    esp_ip4addr_ntoa(&info.ip.u_addr.ip4, snapshot.dns_secondary,
                     sizeof(snapshot.dns_secondary));
  }
}

// --- probes ---------------------------------------------------------------

ProbeResult probe_http(const TargetConfig &target) {
  esp_http_client_config_t cfg = {};
  cfg.url = target.address.c_str();
  cfg.timeout_ms = 5000;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.max_redirection_count = 3;
  cfg.method = HTTP_METHOD_GET;
  cfg.buffer_size = 256;

  const int64_t started = esp_timer_get_time();
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (client == nullptr) {
    return {};
  }

  bool ok = false;
  if (esp_http_client_open(client, 0) == ESP_OK) {
    esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    ok = status >= 200 && status < 400;

    if (ok && target.expect_ok_body) {
      char body[96] = {};
      const int read = esp_http_client_read(client, body, sizeof(body) - 1);
      if (read <= 0) {
        ok = false;
      } else {
        ok = std::strstr(body, "\"status\":true") != nullptr ||
             std::strstr(body, "\"status\": true") != nullptr;
      }
    }
    esp_http_client_close(client);
  }
  esp_http_client_cleanup(client);

  ProbeResult result;
  result.ok = ok;
  result.latency_ms =
      ok ? static_cast<int>((esp_timer_get_time() - started) / 1000) : -1;
  return result;
}

struct PingContext {
  volatile bool done;
  volatile bool ok;
  volatile uint32_t elapsed_ms;
};

void ping_success(esp_ping_handle_t handle, void *args) {
  auto *ctx = static_cast<PingContext *>(args);
  uint32_t elapsed = 0;
  esp_ping_get_profile(handle, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
  ctx->elapsed_ms = elapsed;
  ctx->ok = true;
}

void ping_timeout(esp_ping_handle_t, void *args) {
  static_cast<PingContext *>(args)->ok = false;
}

void ping_end(esp_ping_handle_t, void *args) {
  static_cast<PingContext *>(args)->done = true;
}

ProbeResult probe_ping(const TargetConfig &target) {
  ip_addr_t address;
  if (ipaddr_aton(target.address.c_str(), &address) == 0) {
    return {};
  }

  PingContext ctx = {};
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.target_addr = address;
  cfg.count = 1;
  cfg.interval_ms = 100;
  cfg.timeout_ms = 1500;
  cfg.data_size = 32;
  cfg.task_stack_size = 3072;

  esp_ping_callbacks_t callbacks = {};
  callbacks.cb_args = &ctx;
  callbacks.on_ping_success = ping_success;
  callbacks.on_ping_timeout = ping_timeout;
  callbacks.on_ping_end = ping_end;

  esp_ping_handle_t handle = nullptr;
  if (esp_ping_new_session(&cfg, &callbacks, &handle) != ESP_OK) {
    return {};
  }

  esp_ping_start(handle);
  const int64_t started = esp_timer_get_time();
  while (!ctx.done && (esp_timer_get_time() - started) / 1000 < 2500) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  esp_ping_stop(handle);
  esp_ping_delete_session(handle);

  ProbeResult result;
  result.ok = ctx.done && ctx.ok;
  result.latency_ms = result.ok ? static_cast<int>(ctx.elapsed_ms) : -1;
  return result;
}

ProbeResult probe(const TargetConfig &target) {
  return target.kind == Kind::Ping ? probe_ping(target) : probe_http(target);
}

// --- task -----------------------------------------------------------------

void publish(const Snapshot &next) {
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  s_snapshot = next;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
}

Snapshot copy_snapshot() {
  Snapshot out;
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  out = s_snapshot;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
  return out;
}

Config copy_config() {
  Config out;
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  out = s_config;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
  return out;
}

void prober_task(void *) {
  for (;;) {
    const netwifi::Status wifi = netwifi::get_status();

    if (!wifi.connected) {
      // Keep the last known target states, but say the link is down: the
      // targets are not necessarily down, we just cannot see them.
      Snapshot next = copy_snapshot();
      next.wifi_connected = false;
      next.connected_since_ms = 0;
      publish(next);
      xEventGroupWaitBits(s_events, kConfigChangedBit, pdTRUE, pdFALSE,
                          pdMS_TO_TICKS(kOfflinePollMs));
      continue;
    }

    const Config config = copy_config();
    if (s_config_generation != s_seen_generation) {
      s_seen_generation = s_config_generation;
      for (auto &tracker : s_trackers) {
        tracker = Tracker{};
      }
    }

    Snapshot next;
    next.count = config.count;
    next.probe_seconds = config.probe_seconds;
    next.wifi_connected = true;
    next.connected_since_ms = wifi.connected_since_ms;
    std::snprintf(next.ip, sizeof(next.ip), "%s", wifi.ip);
    apply_dns_override(config);
    fill_dns(next);

    for (int i = 0; i < config.count; ++i) {
      const TargetConfig &target = config.targets[i];
      const ProbeResult result = probe(target);
      apply_result(s_trackers[i], result.ok, now_ms());

      TargetStatus &status = next.targets[i];
      status.label = target.label;
      status.kind = target.kind;
      status.known = s_trackers[i].known;
      status.up = s_trackers[i].up;
      status.since_ms = s_trackers[i].since_ms;
      status.latency_ms = result.ok ? result.latency_ms : -1;

      if (status.known) {
        ++next.known_count;
      }
      if (status.known && status.up) {
        ++next.up_count;
      }
    }

    next.checked_ms = now_ms();
    publish(next);

    xEventGroupWaitBits(s_events, kConfigChangedBit, pdTRUE, pdFALSE,
                        pdMS_TO_TICKS(config.probe_seconds * 1000));
  }
}

} // namespace

void start() {
  if (s_mutex != nullptr) {
    return;
  }
  s_mutex = xSemaphoreCreateMutex();
  s_events = xEventGroupCreate();

  if (!load_config(s_config)) {
    s_config = default_config();
    save_config(s_config);
  }

  // Publish the configured rows straight away so the panel shows labels and
  // "?" instead of an empty table while the first round runs.
  Snapshot initial;
  initial.count = s_config.count;
  initial.probe_seconds = s_config.probe_seconds;
  for (int i = 0; i < s_config.count; ++i) {
    initial.targets[i].label = s_config.targets[i].label;
    initial.targets[i].kind = s_config.targets[i].kind;
  }
  publish(initial);

  ESP_LOGI(TAG, "prober started: %d targets, every %d s", s_config.count,
           s_config.probe_seconds);

  xTaskCreate(&prober_task, "monitor", 8192, nullptr, 3, nullptr);
}

void request_probe() {
  if (s_events != nullptr) {
    xEventGroupSetBits(s_events, kConfigChangedBit);
  }
}

Snapshot snapshot() { return copy_snapshot(); }

Config config() { return copy_config(); }

std::string config_json() { return config_to_json(copy_config()); }

std::string snapshot_json() {
  const Snapshot snap = copy_snapshot();
  const uint32_t now = now_ms();

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "wifi_connected", snap.wifi_connected);
  cJSON_AddStringToObject(root, "ip", snap.ip);
  cJSON_AddStringToObject(root, "dns_main", snap.dns_main);
  cJSON_AddStringToObject(root, "dns_secondary", snap.dns_secondary);
  cJSON_AddNumberToObject(root, "uptime_s", now / 1000);
  cJSON_AddNumberToObject(
      root, "net_uptime_s",
      snap.wifi_connected && snap.connected_since_ms != 0
          ? static_cast<double>(now - snap.connected_since_ms) / 1000.0
          : 0);
  cJSON_AddNumberToObject(root, "checked_ago_s",
                          snap.checked_ms == 0
                              ? -1
                              : static_cast<double>(now - snap.checked_ms) / 1000.0);
  cJSON_AddNumberToObject(root, "up_count", snap.up_count);
  cJSON_AddNumberToObject(root, "known_count", snap.known_count);

  cJSON *targets = cJSON_AddArrayToObject(root, "targets");
  for (int i = 0; i < snap.count; ++i) {
    const TargetStatus &status = snap.targets[i];
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "label", status.label.c_str());
    cJSON_AddStringToObject(item, "kind", to_string(status.kind));
    cJSON_AddBoolToObject(item, "known", status.known);
    cJSON_AddBoolToObject(item, "up", status.up);
    cJSON_AddNumberToObject(item, "latency_ms", status.latency_ms);
    cJSON_AddNumberToObject(item, "streak_s",
                            status.known ? (now - status.since_ms) / 1000.0 : 0);
    cJSON_AddItemToArray(targets, item);
  }

  char *text = cJSON_PrintUnformatted(root);
  const std::string json = text != nullptr ? text : "{}";
  cJSON_free(text);
  cJSON_Delete(root);
  return json;
}

bool set_config(const Config &config, std::string &error) {
  Config normalised = config;
  if (!validate_config(normalised, error)) {
    return false;
  }
  if (!save_config(normalised)) {
    error = "could not write NVS";
    return false;
  }

  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  s_config = normalised;
  ++s_config_generation;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }

  if (s_events != nullptr) {
    xEventGroupSetBits(s_events, kConfigChangedBit);
  }
  ESP_LOGI(TAG, "config updated: %d targets, every %d s", normalised.count,
           normalised.probe_seconds);
  return true;
}

bool set_config_json(const std::string &json, std::string &error) {
  Config parsed;
  if (!config_from_json(json.c_str(), parsed)) {
    error = "malformed configuration";
    return false;
  }
  return set_config(parsed, error);
}

} // namespace monitor
