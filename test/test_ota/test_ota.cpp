#include <unity.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ota/session.hpp"

namespace {

constexpr uint32_t kCapacity = 3 * 1024 * 1024;
constexpr uint32_t kSector = 4096;

std::string digest(const uint8_t *data, uint32_t length) {
  uint64_t state = 1469598103934665603ull;
  for (uint32_t i = 0; i < length; ++i) {
    state ^= data[i];
    state *= 1099511628211ull;
  }
  char out[65];
  std::snprintf(out, sizeof(out), "%016llx%016llx%016llx%016llx",
                (unsigned long long)state, (unsigned long long)(state ^ 0xa5a5a5a5a5a5a5a5ull),
                (unsigned long long)(state + 0x123456789abcdefull),
                (unsigned long long)(state * 3));
  return out;
}

std::string digest(const std::vector<uint8_t> &bytes) {
  return digest(bytes.data(), static_cast<uint32_t>(bytes.size()));
}

class FakeBackend : public ota::Backend {
public:
  FakeBackend() : bytes_(kCapacity, 0xff), erased_(kCapacity, false) {}

  uint32_t capacity() override { return kCapacity; }
  uint32_t sector_size() override { return kSector; }

  bool erase(uint32_t offset, uint32_t length) override {
    ++erase_calls;
    last_erase = {offset, length};
    if (offset % kSector != 0 || length % kSector != 0 ||
        offset + length > kCapacity) {
      bad_erase = true;
      return false;
    }
    for (uint32_t i = offset; i < offset + length; ++i) {
      bytes_[i] = 0xff;
      erased_[i] = true;
    }
    return true;
  }

  bool write(uint32_t offset, const uint8_t *data, uint32_t length) override {
    ++write_calls;
    if (offset + length > kCapacity) {
      return false;
    }
    for (uint32_t i = 0; i < length; ++i) {
      if (!erased_[offset + i]) {
        unerased_write = true;
        return false;
      }
      bytes_[offset + i] = data[i];
    }
    return true;
  }

  bool verify(uint32_t total, const char *sha256, std::string &error) override {
    ++verify_calls;
    if (corrupt) {
      error = "sha256 mismatch: got 0000";
      return false;
    }
    const std::string actual = digest(bytes_.data(), total);
    if (actual != sha256) {
      error = "sha256 mismatch: got " + actual;
      return false;
    }
    return true;
  }

  bool activate(std::string &error) override {
    (void)error;
    activated = true;
    return true;
  }

  void store(const ota::Meta &meta) override {
    saved = meta;
    ++store_calls;
  }

  void clear() override {
    saved = ota::Meta{};
    ++clear_calls;
  }

  std::vector<uint8_t> bytes_;
  std::vector<bool> erased_;
  ota::Meta saved;
  bool corrupt = false;
  bool activated = false;
  bool bad_erase = false;
  bool unerased_write = false;
  int erase_calls = 0;
  int write_calls = 0;
  int verify_calls = 0;
  int store_calls = 0;
  int clear_calls = 0;
  struct {
    uint32_t offset;
    uint32_t length;
  } last_erase = {0, 0};
};

std::vector<uint8_t> payload(uint32_t size) {
  std::vector<uint8_t> out(size);
  for (uint32_t i = 0; i < size; ++i) {
    out[i] = static_cast<uint8_t>((i * 37 + 11) & 0xff);
  }
  return out;
}

// Writes [from, to) in chunks of the requested size.
void append_range(ota::Session &session, const std::vector<uint8_t> &data,
                  uint32_t from, uint32_t to, uint32_t chunk) {
  std::string error;
  for (uint32_t at = from; at < to;) {
    const uint32_t length = std::min(chunk, to - at);
    TEST_ASSERT_TRUE_MESSAGE(
        session.append(at, data.data() + at, length, error), error.c_str());
    at += length;
  }
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_fresh_upload_and_activate(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  const uint32_t offset = session.begin(static_cast<uint32_t>(data.size()),
                                       digest(data), false, error);
  TEST_ASSERT_EQUAL_STRING("", error.c_str());
  TEST_ASSERT_EQUAL_UINT32(0, offset);

  append_range(session, data, 0, 100000, 997);
  TEST_ASSERT_EQUAL_UINT32(100000, session.meta().offset);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Active, (int)session.meta().state);

  // 100000 bytes need ceil(100000/4096) sector erases, not one per chunk.
  TEST_ASSERT_EQUAL_INT(25, backend.erase_calls);
  TEST_ASSERT_FALSE(backend.bad_erase);
  TEST_ASSERT_FALSE(backend.unerased_write);

  TEST_ASSERT_TRUE(session.finish(error));
  TEST_ASSERT_EQUAL_STRING("", error.c_str());
  TEST_ASSERT_TRUE(backend.activated);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Done, (int)session.meta().state);
  TEST_ASSERT_EQUAL_INT(100000, backend.saved.offset);
  TEST_ASSERT_EQUAL_MEMORY(data.data(), backend.bytes_.data(), data.size());
}

void test_resume_after_reboot(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  const std::string sha = digest(data);

  ota::Session first(backend, 0x310000, ota::Meta{});
  std::string error;
  TEST_ASSERT_EQUAL_UINT32(0, first.begin(100000, sha, false, error));
  append_range(first, data, 0, 7000, 512);
  TEST_ASSERT_EQUAL_UINT32(7000, first.meta().offset);

  // Simulate a reboot: a new Session starts from the NVS metadata.
  ota::Session resumed(backend, 0x310000, backend.saved);
  TEST_ASSERT_EQUAL_UINT32(7000, resumed.begin(100000, sha, false, error));
  TEST_ASSERT_EQUAL_INT((int)ota::State::Active, (int)resumed.meta().state);

  append_range(resumed, data, 7000, 100000, 512);
  TEST_ASSERT_TRUE(resumed.finish(error));
  TEST_ASSERT_TRUE(backend.activated);
  TEST_ASSERT_EQUAL_MEMORY(data.data(), backend.bytes_.data(), data.size());
}

void test_offset_mismatch_is_rejected(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  session.begin(100000, digest(data), false, error);
  append_range(session, data, 0, 4096, 1024);

  TEST_ASSERT_FALSE(session.append(0, data.data(), 1024, error));
  TEST_ASSERT_TRUE(error.find("offset mismatch") != std::string::npos);
  TEST_ASSERT_TRUE(error.find("4096") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT32(4096, session.meta().offset);
}

void test_begin_resumes_only_the_same_image(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  session.begin(100000, digest(data), false, error);
  append_range(session, data, 0, 5000, 1000);

  // Same size and hash: resume at 5000 without clearing the metadata.
  TEST_ASSERT_EQUAL_UINT32(5000, session.begin(100000, digest(data), false, error));
  TEST_ASSERT_EQUAL_UINT32(5000, session.meta().offset);

  // A different image restarts at zero.
  const std::vector<uint8_t> other = payload(131000);
  TEST_ASSERT_EQUAL_UINT32(0, session.begin(131000, digest(other), false, error));
  TEST_ASSERT_EQUAL_UINT32(0, session.meta().offset);
  TEST_ASSERT_EQUAL_STRING("", error.c_str());

  // A forced restart does the same even for the same image.
  append_range(session, other, 0, 3000, 1000);
  TEST_ASSERT_EQUAL_UINT32(
      0, session.begin(131000, digest(other), true, error));
}

void test_finish_requires_the_whole_image(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  session.begin(100000, digest(data), false, error);
  append_range(session, data, 0, 5000, 1000);

  TEST_ASSERT_FALSE(session.finish(error));
  TEST_ASSERT_TRUE(error.find("incomplete") != std::string::npos);
  TEST_ASSERT_TRUE(error.find("5000") != std::string::npos);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Active, (int)session.meta().state);
  TEST_ASSERT_FALSE(backend.activated);
}

void test_bad_hash_fails_verification(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  session.begin(100000, digest(data), false, error);
  append_range(session, data, 0, 100000, 2000);
  backend.corrupt = true;

  TEST_ASSERT_FALSE(session.finish(error));
  TEST_ASSERT_TRUE(error.find("sha256 mismatch") != std::string::npos);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Error, (int)session.meta().state);
  TEST_ASSERT_FALSE(backend.activated);
  TEST_ASSERT_EQUAL_STRING(error.c_str(), backend.saved.error);
}

void test_abort_discards_the_session(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});

  std::string error;
  session.begin(100000, digest(data), false, error);
  append_range(session, data, 0, 6000, 1000);
  session.abort();

  TEST_ASSERT_EQUAL_INT((int)ota::State::Idle, (int)session.meta().state);
  TEST_ASSERT_EQUAL_UINT32(0, session.meta().offset);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Idle, (int)backend.saved.state);
  TEST_ASSERT_EQUAL_INT(1, backend.clear_calls);

  TEST_ASSERT_EQUAL_UINT32(0, session.begin(100000, digest(data), false, error));
}

void test_invalid_session_input_is_rejected(void) {
  FakeBackend backend;
  ota::Session session(backend, 0x310000, ota::Meta{});
  std::string error;

  TEST_ASSERT_EQUAL_UINT32(0, session.begin(20000, "not-a-hash", false, error));
  TEST_ASSERT_TRUE(error.find("64 hex") != std::string::npos);

  TEST_ASSERT_EQUAL_UINT32(0, session.begin(1024, digest(payload(20000)),
                                            false, error));
  TEST_ASSERT_TRUE(error.find("smaller") != std::string::npos);

  TEST_ASSERT_EQUAL_UINT32(
      0, session.begin(kCapacity + 1, digest(payload(20000)), false, error));
  TEST_ASSERT_TRUE(error.find("larger") != std::string::npos);
}

void test_erase_runs_ahead_of_writes(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Session session(backend, 0x310000, ota::Meta{});
  std::string error;

  session.begin(100000, digest(data), false, error);
  TEST_ASSERT_TRUE(session.append(0, data.data(), 1, error));
  TEST_ASSERT_EQUAL_INT(1, backend.erase_calls);
  TEST_ASSERT_EQUAL_UINT32(4096, backend.last_erase.length);
  TEST_ASSERT_EQUAL_UINT32(0, backend.last_erase.offset);

  // Still the same sector: no additional erase.
  TEST_ASSERT_TRUE(session.append(1, data.data() + 1, 4095, error));
  TEST_ASSERT_EQUAL_INT(1, backend.erase_calls);

  // Crossing the boundary erases the next sector.
  TEST_ASSERT_TRUE(session.append(4096, data.data() + 4096, 1, error));
  TEST_ASSERT_EQUAL_INT(2, backend.erase_calls);
  TEST_ASSERT_EQUAL_UINT32(4096, backend.last_erase.offset);
  TEST_ASSERT_FALSE(backend.unerased_write);
}

void test_stored_session_for_another_slot_is_ignored(void) {
  FakeBackend backend;
  const std::vector<uint8_t> data = payload(100000);
  ota::Meta stored;
  stored.state = ota::State::Active;
  stored.total = 100000;
  stored.offset = 7000;
  stored.target = 0xdeadbeef; // different partition
  std::snprintf(stored.sha256, sizeof(stored.sha256), "%s", digest(data).c_str());

  ota::Session session(backend, 0x310000, stored);
  TEST_ASSERT_EQUAL_UINT32(0, session.meta().offset);
  TEST_ASSERT_EQUAL_INT((int)ota::State::Idle, (int)session.meta().state);
}

void test_sha_helpers(void) {
  const std::string upper =
      "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789";
  TEST_ASSERT_TRUE(ota::Session::valid_sha(upper));
  TEST_ASSERT_EQUAL_STRING(
      "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789",
      ota::Session::normalize_sha(upper).c_str());
  TEST_ASSERT_FALSE(ota::Session::valid_sha("abc"));
  TEST_ASSERT_FALSE(ota::Session::valid_sha(std::string(64, 'z')));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_upload_and_activate);
  RUN_TEST(test_resume_after_reboot);
  RUN_TEST(test_offset_mismatch_is_rejected);
  RUN_TEST(test_begin_resumes_only_the_same_image);
  RUN_TEST(test_finish_requires_the_whole_image);
  RUN_TEST(test_bad_hash_fails_verification);
  RUN_TEST(test_abort_discards_the_session);
  RUN_TEST(test_invalid_session_input_is_rejected);
  RUN_TEST(test_erase_runs_ahead_of_writes);
  RUN_TEST(test_stored_session_for_another_slot_is_ignored);
  RUN_TEST(test_sha_helpers);
  return UNITY_END();
}
