#include "ota/ble.hpp"

#include "sdkconfig.h"

#if defined(CONFIG_BT_ENABLED) && CONFIG_BT_ENABLED && \
    defined(CONFIG_BT_NIMBLE_ENABLED) && CONFIG_BT_NIMBLE_ENABLED

#include <cstdio>
#include <cstring>
#include <string>

#include "esp_log.h"
#include "esp_system.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ota/engine.hpp"

namespace ota::ble {
namespace {

constexpr const char *TAG = "OTA-BLE";
constexpr const char *kDeviceName = "eink-status-ota";
constexpr size_t kMaxWrite = 512;
constexpr uint32_t kNotifyEvery = 8192;

// e1a70000-6f7a-4b3a-9c1e-2f5d8c0b0001..4, in NimBLE's little-endian order.
const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x01, 0x00, 0x0b, 0x8c, 0x5d, 0x2f, 0x1e, 0x9c, 0x3a, 0x4b, 0x7a, 0x6f,
    0x00, 0x00, 0xa7, 0xe1);
const ble_uuid128_t kControlUuid = BLE_UUID128_INIT(
    0x02, 0x00, 0x0b, 0x8c, 0x5d, 0x2f, 0x1e, 0x9c, 0x3a, 0x4b, 0x7a, 0x6f,
    0x00, 0x00, 0xa7, 0xe1);
const ble_uuid128_t kDataUuid = BLE_UUID128_INIT(
    0x03, 0x00, 0x0b, 0x8c, 0x5d, 0x2f, 0x1e, 0x9c, 0x3a, 0x4b, 0x7a, 0x6f,
    0x00, 0x00, 0xa7, 0xe1);
const ble_uuid128_t kStatusUuid = BLE_UUID128_INIT(
    0x04, 0x00, 0x0b, 0x8c, 0x5d, 0x2f, 0x1e, 0x9c, 0x3a, 0x4b, 0x7a, 0x6f,
    0x00, 0x00, 0xa7, 0xe1);

// Control opcodes. Beginning is split so it works even with a 23-byte MTU:
//   0x01 BEGIN  u32 total (LE) + u8 flags (bit0 = restart)
//   0x02 SHA    ASCII hex, appended (64 chars across one or more writes)
//   0x03 COMMIT start/resume with the accumulated size and hash
//   0x04 FINISH verify and select for next boot
//   0x05 ABORT  discard the session
//   0x06 STATUS notify the compact progress frame
//   0x07 REBOOT reboot the device (only when an image is verified)
constexpr uint8_t kOpBegin = 0x01;
constexpr uint8_t kOpSha = 0x02;
constexpr uint8_t kOpCommit = 0x03;
constexpr uint8_t kOpFinish = 0x04;
constexpr uint8_t kOpAbort = 0x05;
constexpr uint8_t kOpStatus = 0x06;
constexpr uint8_t kOpReboot = 0x07;

bool s_started = false;
uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
uint16_t s_control_handle = 0;
uint16_t s_data_handle = 0;
uint16_t s_status_handle = 0;
uint16_t s_mtu = 23;
uint32_t s_last_notify_offset = 0;
uint32_t s_pending_total = 0;
bool s_pending_restart = false;
std::string s_pending_sha;

std::string status_text() {
  const ota::Info status = ota::info();
  char text[320];
  std::snprintf(text, sizeof(text),
                "{\"state\":\"%s\",\"offset\":%u,\"total\":%u,"
                "\"capacity\":%u,\"partition\":\"%s\",\"sha256\":\"%s\","
                "\"error\":\"%s\"}",
                ota::state_name(status.state), (unsigned)status.offset,
                (unsigned)status.total, (unsigned)status.capacity,
                status.partition, status.sha256, status.error);
  return text;
}

void notify_status() {
  if (s_conn == BLE_HS_CONN_HANDLE_NONE || s_status_handle == 0) {
    return;
  }
  // Nine bytes fit any ATT MTU. The JSON status stays available by reading
  // the characteristic for details and error text.
  const ota::Info status = ota::info();
  uint8_t frame[9];
  frame[0] = static_cast<uint8_t>(status.state);
  frame[1] = static_cast<uint8_t>(status.offset & 0xff);
  frame[2] = static_cast<uint8_t>((status.offset >> 8) & 0xff);
  frame[3] = static_cast<uint8_t>((status.offset >> 16) & 0xff);
  frame[4] = static_cast<uint8_t>((status.offset >> 24) & 0xff);
  frame[5] = static_cast<uint8_t>(status.total & 0xff);
  frame[6] = static_cast<uint8_t>((status.total >> 8) & 0xff);
  frame[7] = static_cast<uint8_t>((status.total >> 16) & 0xff);
  frame[8] = static_cast<uint8_t>((status.total >> 24) & 0xff);
  os_mbuf *om = ble_hs_mbuf_from_flat(frame, sizeof(frame));
  if (om == nullptr) {
    return;
  }
  const int rc = ble_gatts_notify_custom(s_conn, s_status_handle, om);
  if (rc != 0) {
    ESP_LOGD(TAG, "notify failed: %d", rc);
  }
}

uint32_t read_u32le(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void notify_if_due(uint32_t before, uint32_t after) {
  if (after < before || after - s_last_notify_offset >= kNotifyEvery ||
      (ota::total() != 0 && after == ota::total())) {
    s_last_notify_offset = after;
    notify_status();
  }
}

void handle_control(const uint8_t *data, uint16_t length) {
  if (length == 0) {
    return;
  }

  switch (data[0]) {
  case kOpBegin:
    if (length < 5) {
      return;
    }
    s_pending_total = read_u32le(data + 1);
    s_pending_restart = length >= 6 && (data[5] & 0x01) != 0;
    s_pending_sha.clear();
    notify_status();
    break;

  case kOpSha:
    for (uint16_t i = 1; i < length && s_pending_sha.size() < ota::kSha256Hex;
         ++i) {
      s_pending_sha.push_back(static_cast<char>(data[i]));
    }
    break;

  case kOpCommit: {
    std::string error;
    uint32_t offset = 0;
    if (!ota::begin(s_pending_total, s_pending_sha, s_pending_restart, offset,
                    error)) {
      ESP_LOGW(TAG, "begin rejected: %s", error.c_str());
    } else {
      ESP_LOGI(TAG, "begin %u bytes at offset %u", (unsigned)s_pending_total,
               (unsigned)offset);
      s_last_notify_offset = offset;
    }
    s_pending_sha.clear();
    notify_status();
    break;
  }

  case kOpFinish: {
    std::string error;
    if (!ota::finish(error)) {
      ESP_LOGW(TAG, "finish failed: %s", error.c_str());
    }
    notify_status();
    break;
  }

  case kOpAbort: {
    std::string error;
    ota::abort(error);
    s_last_notify_offset = 0;
    notify_status();
    break;
  }

  case kOpStatus:
    notify_status();
    break;

  case kOpReboot:
    if (ota::info().state == ota::State::Done) {
      ESP_LOGW(TAG, "reboot requested over BLE");
      esp_restart();
    } else {
      notify_status();
    }
    break;

  default:
    ESP_LOGW(TAG, "unknown control opcode 0x%02x", data[0]);
    break;
  }
}

int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg) {
  (void)conn_handle;
  (void)arg;

  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    const uint16_t length = OS_MBUF_PKTLEN(ctxt->om);
    if (length == 0 || length > kMaxWrite) {
      return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buffer[kMaxWrite];
    uint16_t copied = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buffer, sizeof(buffer), &copied) != 0) {
      return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == s_control_handle) {
      handle_control(buffer, copied);
      return 0;
    }

    if (attr_handle == s_data_handle) {
      const uint32_t before = ota::offset();
      std::string error;
      if (!ota::append_next(buffer, copied, error)) {
        ESP_LOGW(TAG, "chunk at %u rejected: %s", (unsigned)before,
                 error.c_str());
        notify_status();
        // Keep the ATT write successful: the status notification carries the
        // current offset, and the client can resume from there.
        return 0;
      }
      notify_if_due(before, ota::offset());
      return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
  }

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    if (attr_handle != s_status_handle) {
      return BLE_ATT_ERR_UNLIKELY;
    }
    const std::string text = status_text();
    const int rc = os_mbuf_append(ctxt->om, text.data(), text.size());
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  return BLE_ATT_ERR_UNLIKELY;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
const struct ble_gatt_svc_def kServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &kControlUuid.u,
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = &s_control_handle,
                },
                {
                    .uuid = &kDataUuid.u,
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = &s_data_handle,
                },
                {
                    .uuid = &kStatusUuid.u,
                    .access_cb = gatt_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &s_status_handle,
                },
                {0},
            },
    },
    {0},
};

int gap_event(struct ble_gap_event *event, void *arg);

void advertise() {
  struct ble_hs_adv_fields fields = {};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.name = reinterpret_cast<const uint8_t *>(kDeviceName);
  fields.name_len = std::strlen(kDeviceName);
  fields.name_is_complete = 1;

  struct ble_hs_adv_fields response = {};
  response.uuids128 = &kServiceUuid;
  response.num_uuids128 = 1;
  response.uuids128_is_complete = 1;

  int rc = ble_gap_adv_set_fields(&fields);
  if (rc == 0) {
    rc = ble_gap_adv_rsp_set_fields(&response);
  }
  if (rc != 0) {
    ESP_LOGW(TAG, "advertising data rejected: %d", rc);
  }

  struct ble_gap_adv_params params = {};
  params.conn_mode = BLE_GAP_CONN_MODE_UND;
  params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGW(TAG, "no BLE address: %d", rc);
    return;
  }

  rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &params,
                         gap_event, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    ESP_LOGW(TAG, "advertise failed: %d", rc);
  }
}

int gap_event(struct ble_gap_event *event, void *arg) {
  (void)arg;

  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
      s_conn = event->connect.conn_handle;
      ESP_LOGI(TAG, "connected (handle %u)", s_conn);
    } else {
      advertise();
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "disconnected (reason %d)", event->disconnect.reason);
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    advertise();
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    advertise();
    return 0;

  case BLE_GAP_EVENT_MTU:
    s_mtu = event->mtu.value;
    ESP_LOGI(TAG, "mtu %u", s_mtu);
    return 0;

  case BLE_GAP_EVENT_SUBSCRIBE:
    ESP_LOGI(TAG, "notify subscription %s",
             event->subscribe.cur_notify ? "on" : "off");
    return 0;

  default:
    return 0;
  }
}

void on_sync() {
  if (ble_hs_util_ensure_addr(0) != 0) {
    ESP_LOGE(TAG, "no usable BLE address");
    return;
  }
  advertise();
}

void on_reset(int reason) { ESP_LOGW(TAG, "host reset: %d", reason); }

void host_task(void *) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

} // namespace

void start() {
  if (s_started) {
    return;
  }

  const esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
    return;
  }

  ble_svc_gap_init();
  ble_svc_gatt_init();

  int rc = ble_gatts_count_cfg(kServices);
  if (rc == 0) {
    rc = ble_gatts_add_svcs(kServices);
  }
  if (rc != 0) {
    ESP_LOGE(TAG, "GATT registration failed: %d", rc);
    return;
  }

  ble_svc_gap_device_name_set(kDeviceName);
  ble_hs_cfg.sync_cb = on_sync;
  ble_hs_cfg.reset_cb = on_reset;

  nimble_port_freertos_init(host_task);
  s_started = true;
  ESP_LOGI(TAG, "advertising as %s", kDeviceName);
}

Info info() {
  Info out;
  out.enabled = s_started;
  out.connected = s_conn != BLE_HS_CONN_HANDLE_NONE;
  out.mtu = s_mtu;
  return out;
}

} // namespace ota::ble

#else

namespace ota::ble {

void start() {}

Info info() { return Info{}; }

} // namespace ota::ble

#endif
