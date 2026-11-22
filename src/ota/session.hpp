#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Platform-free OTA session: offset accounting, lazy sector erase and resume
// decisions. The ESP backend does the flash writes, hashing and boot switch,
// so the state machine can be exercised by the host tests.
namespace ota {

inline constexpr size_t kSha256Hex = 64;
// An application image is far larger than this; the check catches a client
// that posted a size in the wrong units.
inline constexpr uint32_t kMinImageBytes = 64 * 1024;

enum class State : uint8_t {
  Idle = 0,
  Active = 1,
  Verifying = 2,
  Done = 3,
  Error = 4,
};

const char *state_name(State state);

struct Meta {
  State state = State::Idle;
  uint32_t total = 0;
  uint32_t offset = 0;
  uint32_t target = 0; // flash address of the update partition
  char sha256[kSha256Hex + 1] = {};
  char error[128] = {};
};

// Flash operations the session needs. erase() must be sector aligned and is
// allowed to cover a little more than the current chunk; write() may only
// touch erased sectors (the fake backend in the tests enforces that).
class Backend {
public:
  virtual ~Backend() = default;
  virtual uint32_t capacity() = 0;
  virtual uint32_t sector_size() = 0;
  virtual bool erase(uint32_t offset, uint32_t length) = 0;
  virtual bool write(uint32_t offset, const uint8_t *data, uint32_t length) = 0;
  virtual bool verify(uint32_t total, const char *sha256,
                      std::string &error) = 0;
  virtual bool activate(std::string &error) = 0;
  virtual void store(const Meta &meta) = 0;
  virtual void clear() = 0;
};

class Session {
public:
  // `stored` is the metadata from NVS. It is only resumed when it belongs to
  // this target partition; anything else starts clean.
  Session(Backend &backend, uint32_t target, const Meta &stored);

  // Starts a new image or resumes an identical one. Returns the offset the
  // caller should continue from.
  uint32_t begin(uint32_t total, const std::string &sha256, bool restart,
                 std::string &error);

  bool append(uint32_t offset, const uint8_t *data, uint32_t length,
              std::string &error);
  bool finish(std::string &error);
  void abort();

  const Meta &meta() const { return meta_; }

  static bool valid_sha(const std::string &sha256);
  static std::string normalize_sha(const std::string &sha256);

private:
  void reset(uint32_t total, const std::string &sha256);
  uint32_t align_up(uint32_t value, uint32_t alignment) const;

  Backend &backend_;
  uint32_t target_;
  Meta meta_;
  uint32_t erase_cursor_ = 0;
};

} // namespace ota
