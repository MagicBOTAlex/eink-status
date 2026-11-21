#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ui/types.hpp"

// Refresh policy and coordinate conversion, kept free of ESP-IDF so the host
// tests can cover the decisions that used to live in main.cpp.
namespace panel {

// Inclusive pixel bounds, which is what the UC8179 window commands take.
struct Window {
  int x1 = 0;
  int y1 = 0;
  int x2 = 0;
  int y2 = 0;

  bool operator==(const Window &o) const {
    return x1 == o.x1 && y1 == o.y1 && x2 == o.x2 && y2 == o.y2;
  }
};

// ui::Rect is (x, y, w, h); the panel wants inclusive edges. Getting this
// wrong makes every partial window invalid, which is silent on the wire.
inline Window to_window(const ui::Rect &rect) {
  if (rect.empty()) {
    // Unambiguous "nothing to refresh": valid() rejects it.
    return {0, 0, -1, -1};
  }
  return {rect.x, rect.y, rect.right() - 1, rect.bottom() - 1};
}

inline bool valid(const Window &w) { return w.x1 <= w.x2 && w.y1 <= w.y2; }

enum class Action { None, Partial, Full };

// Decides between doing nothing, a handful of partial refreshes and one
// full-screen refresh.
inline Action choose(const std::vector<ui::Rect> &dirty, size_t max_regions,
                     long max_area) {
  if (dirty.empty()) {
    return Action::None;
  }
  if (dirty.size() > max_regions) {
    return Action::Full;
  }
  long area = 0;
  for (const ui::Rect &r : dirty) {
    area += r.area();
    if (area > max_area) {
      return Action::Full;
    }
  }
  return Action::Partial;
}

// Wrap-safe "period elapsed" test for the periodic ghost cleanup.
inline bool due(uint32_t now, uint32_t last, uint32_t period) {
  return static_cast<uint32_t>(now - last) >= period;
}

} // namespace panel
