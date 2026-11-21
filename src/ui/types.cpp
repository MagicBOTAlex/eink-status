#include "ui/types.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ui {

Rect Rect::intersect(const Rect &other) const {
  if (!intersects(other)) {
    return {};
  }
  const int x1 = std::max(x, other.x);
  const int y1 = std::max(y, other.y);
  const int x2 = std::min(right(), other.right());
  const int y2 = std::min(bottom(), other.bottom());
  return {x1, y1, x2 - x1, y2 - y1};
}

Rect Rect::unite(const Rect &other) const {
  if (empty()) {
    return other;
  }
  if (other.empty()) {
    return *this;
  }
  const int x1 = std::min(x, other.x);
  const int y1 = std::min(y, other.y);
  const int x2 = std::max(right(), other.right());
  const int y2 = std::max(bottom(), other.bottom());
  return {x1, y1, x2 - x1, y2 - y1};
}

std::string Rect::to_string() const {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%d,%d %dx%d", x, y, w, h);
  return buf;
}

Rect anchor_rect(const Rect &parent, Size size, Align align, int dx, int dy) {
  int x = parent.x;
  int y = parent.y;

  switch (align) {
  case Align::TopLeft:
  case Align::CenterLeft:
  case Align::BottomLeft:
    x = parent.x;
    break;
  case Align::TopCenter:
  case Align::Center:
  case Align::BottomCenter:
    x = parent.x + (parent.w - size.w) / 2;
    break;
  case Align::TopRight:
  case Align::CenterRight:
  case Align::BottomRight:
    x = parent.x + parent.w - size.w;
    break;
  }

  switch (align) {
  case Align::TopLeft:
  case Align::TopCenter:
  case Align::TopRight:
    y = parent.y;
    break;
  case Align::CenterLeft:
  case Align::Center:
  case Align::CenterRight:
    y = parent.y + (parent.h - size.h) / 2;
    break;
  case Align::BottomLeft:
  case Align::BottomCenter:
  case Align::BottomRight:
    y = parent.y + parent.h - size.h;
    break;
  }

  return {x + dx, y + dy, size.w, size.h};
}

const char *to_string(Align align) {
  switch (align) {
  case Align::TopLeft:
    return "top-left";
  case Align::TopCenter:
    return "top-center";
  case Align::TopRight:
    return "top-right";
  case Align::CenterLeft:
    return "center-left";
  case Align::Center:
    return "center";
  case Align::CenterRight:
    return "center-right";
  case Align::BottomLeft:
    return "bottom-left";
  case Align::BottomCenter:
    return "bottom-center";
  case Align::BottomRight:
    return "bottom-right";
  }
  return "top-left";
}

const char *to_string(HAlign align) {
  switch (align) {
  case HAlign::Left:
    return "left";
  case HAlign::Center:
    return "center";
  case HAlign::Right:
    return "right";
  }
  return "left";
}

const char *to_string(VAlign align) {
  switch (align) {
  case VAlign::Top:
    return "top";
  case VAlign::Middle:
    return "middle";
  case VAlign::Bottom:
    return "bottom";
  }
  return "top";
}

bool align_from_string(const std::string &text, Align *out) {
  static const Align values[] = {
      Align::TopLeft,     Align::TopCenter,     Align::TopRight,
      Align::CenterLeft,  Align::Center,        Align::CenterRight,
      Align::BottomLeft,  Align::BottomCenter,  Align::BottomRight,
  };
  for (Align a : values) {
    if (text == to_string(a)) {
      if (out) {
        *out = a;
      }
      return true;
    }
  }
  return false;
}

bool halign_from_string(const std::string &text, HAlign *out) {
  if (text == "left") {
    if (out) {
      *out = HAlign::Left;
    }
    return true;
  }
  if (text == "center") {
    if (out) {
      *out = HAlign::Center;
    }
    return true;
  }
  if (text == "right") {
    if (out) {
      *out = HAlign::Right;
    }
    return true;
  }
  return false;
}

bool valign_from_string(const std::string &text, VAlign *out) {
  if (text == "top") {
    if (out) {
      *out = VAlign::Top;
    }
    return true;
  }
  if (text == "middle") {
    if (out) {
      *out = VAlign::Middle;
    }
    return true;
  }
  if (text == "bottom") {
    if (out) {
      *out = VAlign::Bottom;
    }
    return true;
  }
  return false;
}

namespace {

bool near(const Rect &a, const Rect &b, int gap) {
  // Touching or closer than `gap` on both axes.
  return a.x - gap <= b.right() && b.x - gap <= a.right() &&
         a.y - gap <= b.bottom() && b.y - gap <= a.bottom();
}

} // namespace

std::vector<Rect> merge_rects(std::vector<Rect> rects, int gap) {
  std::vector<Rect> out;
  for (const Rect &r : rects) {
    if (!r.empty()) {
      out.push_back(r);
    }
  }

  bool merged = true;
  while (merged) {
    merged = false;
    for (size_t i = 0; i < out.size() && !merged; ++i) {
      for (size_t j = i + 1; j < out.size(); ++j) {
        if (near(out[i], out[j], gap)) {
          out[i] = out[i].unite(out[j]);
          out.erase(out.begin() + j);
          merged = true;
          break;
        }
      }
    }
  }

  std::sort(out.begin(), out.end(), [](const Rect &a, const Rect &b) {
    if (a.y != b.y) {
      return a.y < b.y;
    }
    if (a.x != b.x) {
      return a.x < b.x;
    }
    return a.area() < b.area();
  });
  return out;
}

} // namespace ui
