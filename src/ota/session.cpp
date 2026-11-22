#include "ota/session.hpp"

#include <cstdio>
#include <cstring>

namespace ota {
namespace {

bool is_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

char lower(char c) {
  return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 'a') : c;
}

void set_error(Meta &meta, const std::string &text) {
  std::snprintf(meta.error, sizeof(meta.error), "%s", text.c_str());
}

} // namespace

const char *state_name(State state) {
  switch (state) {
  case State::Idle:
    return "idle";
  case State::Active:
    return "active";
  case State::Verifying:
    return "verifying";
  case State::Done:
    return "done";
  case State::Error:
    return "error";
  }
  return "unknown";
}

bool Session::valid_sha(const std::string &sha256) {
  if (sha256.size() != kSha256Hex) {
    return false;
  }
  for (const char c : sha256) {
    if (!is_hex(c)) {
      return false;
    }
  }
  return true;
}

std::string Session::normalize_sha(const std::string &sha256) {
  std::string out;
  out.reserve(sha256.size());
  for (const char c : sha256) {
    out.push_back(lower(c));
  }
  return out;
}

Session::Session(Backend &backend, uint32_t target, const Meta &stored)
    : backend_(backend), target_(target) {
  meta_.target = target;

  if (stored.state != State::Active || stored.target != target ||
      stored.total < kMinImageBytes || stored.total > backend_.capacity() ||
      stored.offset > stored.total || !valid_sha(stored.sha256)) {
    return;
  }

  meta_ = stored;
  std::snprintf(meta_.sha256, sizeof(meta_.sha256), "%s",
                normalize_sha(stored.sha256).c_str());
  // The sector holding the resume offset was erased before the data below the
  // offset was written (erase runs ahead of the write cursor), so only whole
  // sectors after it need erasing again.
  erase_cursor_ = align_up(meta_.offset, backend_.sector_size());
}

uint32_t Session::align_up(uint32_t value, uint32_t alignment) const {
  if (alignment == 0) {
    return value;
  }
  return (value + alignment - 1) / alignment * alignment;
}

void Session::reset(uint32_t total, const std::string &sha256) {
  meta_ = Meta{};
  meta_.state = State::Active;
  meta_.total = total;
  meta_.target = target_;
  std::snprintf(meta_.sha256, sizeof(meta_.sha256), "%s", sha256.c_str());
  erase_cursor_ = 0;
}

uint32_t Session::begin(uint32_t total, const std::string &sha256,
                        bool restart, std::string &error) {
  error.clear();
  const std::string normalized = normalize_sha(sha256);

  if (!valid_sha(normalized)) {
    error = "sha256 must be 64 hex characters";
    return meta_.offset;
  }
  if (total < kMinImageBytes) {
    error = "image is smaller than " + std::to_string(kMinImageBytes) + " bytes";
    return meta_.offset;
  }
  if (total > backend_.capacity()) {
    error = "image is larger than the update partition";
    return meta_.offset;
  }

  if (!restart && meta_.state == State::Active && meta_.total == total &&
      normalized == meta_.sha256) {
    return meta_.offset;
  }

  if (meta_.state != State::Idle) {
    backend_.clear();
  }
  reset(total, normalized);
  backend_.store(meta_);
  return 0;
}

bool Session::append(uint32_t offset, const uint8_t *data, uint32_t length,
                     std::string &error) {
  error.clear();
  if (meta_.state != State::Active) {
    error = "no active upload";
    return false;
  }
  if (length == 0) {
    error = "empty chunk";
    return false;
  }
  if (offset != meta_.offset) {
    error = "offset mismatch: expected " + std::to_string(meta_.offset);
    return false;
  }
  if (length > meta_.total - offset) {
    error = "chunk runs past the end of the image";
    return false;
  }

  // Erase whole sectors ahead of the write cursor. The extra bytes at the end
  // of a sector are reused by the next chunk, so a 1.2 MB image does not turn
  // into one erase request per BLE packet.
  const uint32_t sector = backend_.sector_size();
  const uint32_t erase_end = align_up(offset + length, sector);
  if (erase_end > erase_cursor_) {
    if (!backend_.erase(erase_cursor_, erase_end - erase_cursor_)) {
      error = "flash erase failed";
      return false;
    }
    erase_cursor_ = erase_end;
  }

  if (!backend_.write(offset, data, length)) {
    error = "flash write failed";
    return false;
  }

  meta_.offset = offset + length;
  meta_.error[0] = '\0';
  backend_.store(meta_);
  return true;
}

bool Session::finish(std::string &error) {
  error.clear();
  if (meta_.state != State::Active) {
    error = "no active upload";
    return false;
  }
  if (meta_.offset != meta_.total) {
    error = "incomplete image: " + std::to_string(meta_.offset) + " of " +
            std::to_string(meta_.total) + " bytes";
    return false;
  }

  meta_.state = State::Verifying;
  backend_.store(meta_);

  if (!backend_.verify(meta_.total, meta_.sha256, error)) {
    meta_.state = State::Error;
    set_error(meta_, error);
    backend_.store(meta_);
    return false;
  }

  if (!backend_.activate(error)) {
    meta_.state = State::Error;
    set_error(meta_, error);
    backend_.store(meta_);
    return false;
  }

  meta_.state = State::Done;
  meta_.error[0] = '\0';
  backend_.store(meta_);
  return true;
}

void Session::abort() {
  backend_.clear();
  meta_ = Meta{};
  meta_.target = target_;
  erase_cursor_ = 0;
}

} // namespace ota
