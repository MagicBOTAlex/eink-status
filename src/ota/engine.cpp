#include "ota/engine.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

namespace ota {
namespace {

constexpr const char *TAG = "OTA";
constexpr const char *kNvsNamespace = "ota";
// NVS is written at most once per 8 KB. A reboot can therefore roll the
// reported offset back by less than one chunk; the client then resends bytes
// that are already on flash, which is harmless because they are identical.
constexpr uint32_t kPersistEvery = 8192;

void hex_digest(const uint8_t *digest, char *out) {
  static const char *digits = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = digits[digest[i] >> 4];
    out[i * 2 + 1] = digits[digest[i] & 0x0f];
  }
  out[64] = '\0';
}

class EspBackend : public Backend {
public:
  explicit EspBackend(const esp_partition_t *partition)
      : partition_(partition) {}

  uint32_t capacity() override {
    return partition_ != nullptr ? partition_->size : 0;
  }

  uint32_t sector_size() override { return 4096; }

  bool erase(uint32_t offset, uint32_t length) override {
    if (partition_ == nullptr || length == 0) {
      return false;
    }
    if ((offset % sector_size()) != 0 || (length % sector_size()) != 0) {
      ESP_LOGE(TAG, "unaligned erase at %u len %u", (unsigned)offset,
               (unsigned)length);
      return false;
    }
    return esp_partition_erase_range(partition_, offset, length) == ESP_OK;
  }

  bool write(uint32_t offset, const uint8_t *data, uint32_t length) override {
    if (partition_ == nullptr) {
      return false;
    }
    return esp_partition_write(partition_, offset, data, length) == ESP_OK;
  }

  bool verify(uint32_t total, const char *expected, std::string &error) override {
    if (partition_ == nullptr) {
      error = "no update partition";
      return false;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    constexpr uint32_t kBlock = 4096;
    std::unique_ptr<uint8_t[]> block(new (std::nothrow) uint8_t[kBlock]);
    if (block == nullptr) {
      mbedtls_sha256_free(&ctx);
      error = "out of memory while hashing";
      return false;
    }

    for (uint32_t at = 0; at < total;) {
      const uint32_t chunk = std::min(kBlock, total - at);
      if (esp_partition_read(partition_, at, block.get(), chunk) != ESP_OK) {
        mbedtls_sha256_free(&ctx);
        error = "flash read failed while hashing";
        return false;
      }
      mbedtls_sha256_update(&ctx, block.get(), chunk);
      at += chunk;
    }

    uint8_t digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    char actual[kSha256Hex + 1];
    hex_digest(digest, actual);
    if (std::strcmp(actual, expected) != 0) {
      error = std::string("sha256 mismatch: got ") + actual;
      return false;
    }
    return true;
  }

  bool activate(std::string &error) override {
    if (partition_ == nullptr) {
      error = "no update partition";
      return false;
    }
    // Also verifies the ESP image header/checksum, so a truncated or
    // non-firmware file can never be selected for the next boot.
    const esp_err_t err = esp_ota_set_boot_partition(partition_);
    if (err != ESP_OK) {
      error = std::string("boot partition rejected: ") + esp_err_to_name(err);
      return false;
    }
    return true;
  }

  void store(const Meta &meta) override {
    // Finish/abort/begin must always land in NVS; steady-state chunk writes
    // are batched to keep flash wear and BLE latency down.
    if (meta.state == State::Active && meta.offset != 0 &&
        meta.offset - last_saved_ < kPersistEvery) {
      return;
    }
    persist(meta);
  }

  void clear() override {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == ESP_OK) {
      nvs_erase_all(handle);
      nvs_commit(handle);
      nvs_close(handle);
    }
    last_saved_ = 0;
  }

  Meta load() {
    Meta meta;
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
      return meta;
    }

    uint8_t state = 0;
    if (nvs_get_u8(handle, "state", &state) == ESP_OK && state <= 4) {
      meta.state = static_cast<State>(state);
    }
    nvs_get_u32(handle, "total", &meta.total);
    nvs_get_u32(handle, "offset", &meta.offset);
    nvs_get_u32(handle, "target", &meta.target);

    size_t length = sizeof(meta.sha256);
    if (nvs_get_str(handle, "sha", meta.sha256, &length) != ESP_OK) {
      meta.sha256[0] = '\0';
    }
    length = sizeof(meta.error);
    if (nvs_get_str(handle, "error", meta.error, &length) != ESP_OK) {
      meta.error[0] = '\0';
    }
    nvs_close(handle);
    last_saved_ = meta.offset;
    return meta;
  }

private:
  void persist(const Meta &meta) {
    nvs_handle_t handle;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
      ESP_LOGW(TAG, "could not open NVS");
      return;
    }
    nvs_set_u8(handle, "state", static_cast<uint8_t>(meta.state));
    nvs_set_u32(handle, "total", meta.total);
    nvs_set_u32(handle, "offset", meta.offset);
    nvs_set_u32(handle, "target", meta.target);
    nvs_set_str(handle, "sha", meta.sha256);
    nvs_set_str(handle, "error", meta.error);
    const esp_err_t err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "NVS commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
    last_saved_ = meta.offset;
  }

  const esp_partition_t *partition_ = nullptr;
  uint32_t last_saved_ = 0;
};

class Engine {
public:
  Engine() {
    target_ = esp_ota_get_next_update_partition(nullptr);
    if (target_ == nullptr) {
      ESP_LOGE(TAG, "no OTA update partition; check partitions.csv");
      return;
    }

    backend_.reset(new EspBackend(target_));
    available_ = true;

    Meta stored = backend_->load();
    if (stored.state == State::Done) {
      // The image was selected and the device has rebooted into it.
      backend_->clear();
      stored = Meta{};
    } else if (stored.target != target_->address) {
      // Partition table or running slot changed since the interrupted upload.
      backend_->clear();
      stored = Meta{};
    }

    session_.reset(new Session(*backend_, target_->address, stored));
    ESP_LOGI(TAG, "slot %s at 0x%08x, %u KiB%s", target_->label,
             (unsigned)target_->address, (unsigned)(target_->size / 1024),
             stored.state == State::Active ? " (resuming)" : "");
  }

  Info info() {
    std::lock_guard<std::mutex> lock(mutex_);
    Info out;
    out.available = available_;
    out.capacity = backend_ != nullptr ? backend_->capacity() : 0;
    if (session_ != nullptr) {
      const Meta &meta = session_->meta();
      out.state = meta.state;
      out.offset = meta.offset;
      out.total = meta.total;
      std::snprintf(out.sha256, sizeof(out.sha256), "%s", meta.sha256);
      std::snprintf(out.error, sizeof(out.error), "%s", meta.error);
    }
    if (target_ != nullptr) {
      std::snprintf(out.partition, sizeof(out.partition), "%s", target_->label);
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != nullptr) {
      std::snprintf(out.running, sizeof(out.running), "%s", running->label);
    }
    return out;
  }

  bool begin(uint32_t total, const std::string &sha256, bool restart,
             uint32_t &offset, std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available_ || session_ == nullptr) {
      error = "no OTA partition";
      return false;
    }
    offset = session_->begin(total, sha256, restart, error);
    if (!error.empty()) {
      return false;
    }
    ESP_LOGI(TAG, "begin %u bytes sha=%s at offset %u",
             (unsigned)total, sha256.c_str(), (unsigned)offset);
    return true;
  }

  bool append(uint32_t at, const uint8_t *data, uint32_t length,
              std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available_ || session_ == nullptr) {
      error = "no OTA partition";
      return false;
    }
    return session_->append(at, data, length, error);
  }

  bool append_next(const uint8_t *data, uint32_t length, std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available_ || session_ == nullptr) {
      error = "no OTA partition";
      return false;
    }
    return session_->append(session_->meta().offset, data, length, error);
  }

  bool finish(std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available_ || session_ == nullptr) {
      error = "no OTA partition";
      return false;
    }
    const bool ok = session_->finish(error);
    if (ok) {
      ESP_LOGI(TAG, "image verified; next boot uses %s", target_->label);
    } else {
      ESP_LOGE(TAG, "finish failed: %s", error.c_str());
    }
    return ok;
  }

  bool abort(std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_ == nullptr) {
      error = "no OTA partition";
      return false;
    }
    session_->abort();
    ESP_LOGW(TAG, "upload aborted");
    return true;
  }

  uint32_t offset() {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_ != nullptr ? session_->meta().offset : 0;
  }

  uint32_t total() {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_ != nullptr ? session_->meta().total : 0;
  }

  bool active() {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_ != nullptr && session_->meta().state == State::Active;
  }

private:
  std::mutex mutex_;
  bool available_ = false;
  const esp_partition_t *target_ = nullptr;
  std::unique_ptr<EspBackend> backend_;
  std::unique_ptr<Session> session_;
};

Engine &engine() {
  static Engine instance;
  return instance;
}

} // namespace

void init() { (void)engine(); }

Info info() { return engine().info(); }

bool begin(uint32_t total, const std::string &sha256, bool restart,
           uint32_t &offset, std::string &error) {
  return engine().begin(total, sha256, restart, offset, error);
}

bool append(uint32_t offset, const uint8_t *data, uint32_t length,
            std::string &error) {
  return engine().append(offset, data, length, error);
}

bool append_next(const uint8_t *data, uint32_t length, std::string &error) {
  return engine().append_next(data, length, error);
}

bool finish(std::string &error) { return engine().finish(error); }

bool abort(std::string &error) { return engine().abort(error); }

uint32_t offset() { return engine().offset(); }

uint32_t total() { return engine().total(); }

bool active() { return engine().active(); }

} // namespace ota
