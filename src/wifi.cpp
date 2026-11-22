#include "wifi.hpp"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "wifi_seeds.hpp"

namespace netwifi {
namespace {

constexpr const char *TAG = "WIFI";

constexpr EventBits_t kConnectedBit = BIT0;
constexpr EventBits_t kDisconnectedBit = BIT1;
constexpr uint32_t kAttemptTimeoutMs = 20000;

EventGroupHandle_t s_events = nullptr;
SemaphoreHandle_t s_mutex = nullptr;
Status s_status = {false, "", "", 0};

uint32_t now_ms() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void publish(bool connected, const char *ssid, const char *ip) {
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  // Capture the previous state before overwriting it: comparing after the
  // assignment made the "net up" counter stay at zero forever.
  const bool was_connected = s_status.connected;
  s_status.connected = connected;
  if (connected && !was_connected) {
    s_status.connected_since_ms = now_ms();
  } else if (!connected) {
    s_status.connected_since_ms = 0;
  }
  if (ssid != nullptr) {
    std::snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", ssid);
  }
  if (ip != nullptr) {
    std::snprintf(s_status.ip, sizeof(s_status.ip), "%s", ip);
  }
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
}

void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)arg;

  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    const auto *event = static_cast<wifi_event_sta_disconnected_t *>(data);
    ESP_LOGW(TAG, "disconnected (reason %d)", event->reason);
    publish(false, nullptr, "");
    xEventGroupSetBits(s_events, kDisconnectedBit);
    return;
  }

  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    const auto *event = static_cast<ip_event_got_ip_t *>(data);
    char ip[16] = {};
    esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
    char ssid[33] = {};
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      std::snprintf(ssid, sizeof(ssid), "%s", reinterpret_cast<char *>(ap.ssid));
    }
    ESP_LOGI(TAG, "connected to '%s', ip %s", ssid, ip);
    publish(true, ssid, ip);
    xEventGroupSetBits(s_events, kConnectedBit);
  }
}

bool status_connected() {
  bool connected = false;
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  connected = s_status.connected;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
  return connected;
}

void wifi_task(void *arg) {
  (void)arg;

  for (;;) {
    if (status_connected()) {
      // Wait for the link to drop, then start the credential sweep again.
      xEventGroupWaitBits(s_events, kDisconnectedBit, pdTRUE, pdFALSE,
                          portMAX_DELAY);
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    bool connected = false;
    for (size_t seed_index = 0; seed_index < kSeedCount; ++seed_index) {
      const Seed &cred = kSeeds[seed_index];
      xEventGroupClearBits(s_events, kConnectedBit | kDisconnectedBit);

      wifi_config_t cfg = {};
      std::snprintf(reinterpret_cast<char *>(cfg.sta.ssid), sizeof(cfg.sta.ssid),
                    "%s", cred.ssid);
      std::snprintf(reinterpret_cast<char *>(cfg.sta.password),
                    sizeof(cfg.sta.password), "%s", cred.password);
      cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

      esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed: %s", esp_err_to_name(err));
        continue;
      }

      ESP_LOGI(TAG, "connecting to '%s'", cred.ssid);
      err = esp_wifi_connect();
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(err));
        continue;
      }

      const EventBits_t bits = xEventGroupWaitBits(
          s_events, kConnectedBit | kDisconnectedBit, pdTRUE, pdFALSE,
          pdMS_TO_TICKS(kAttemptTimeoutMs));

      if ((bits & kConnectedBit) != 0) {
        connected = true;
        break;
      }

      ESP_LOGW(TAG, "attempt '%s' did not associate", cred.ssid);
      esp_wifi_disconnect();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!connected) {
      ESP_LOGW(TAG, "no seed network reachable; retrying in 10 s");
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}

} // namespace

void start() {
  s_mutex = xSemaphoreCreateMutex();
  s_events = xEventGroupCreate();

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "erasing NVS (%s)", esp_err_to_name(err));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, nullptr));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  xTaskCreate(&wifi_task, "wifi_task", 4096, nullptr, 4, nullptr);
  ESP_LOGI(TAG, "STA started; %u seed network(s)", (unsigned)kSeedCount);
  if (kSeedCount == 0) {
    ESP_LOGW(TAG, "no credentials: copy src/wifi_credentials.example.hpp to "
                  "src/wifi_credentials.hpp");
  }
}

Status get_status() {
  Status out = {};
  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }
  out = s_status;
  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
  return out;
}

} // namespace netwifi
