#pragma once

#include "ui/font.hpp"
#include "ui/fonts/cozette_16.hpp"
#include "ui/fonts/cozette_24.hpp"
#include "ui/fonts/cozette_32.hpp"
#include "ui/fonts/cozette_96.hpp"
#include "ui/types.hpp"

namespace app::theme {

// Logical UI size. The panel is 648x480; it is mounted sideways, so the UI
// lays out in 480x648 and rotation.cpp turns the frame before streaming.
inline constexpr int kWidth = 480;
inline constexpr int kHeight = 648;
inline constexpr int kMargin = 16;

inline const ui::Font &small_font() { return ui::fonts::kCozette16; }
inline const ui::Font &label_font() { return ui::fonts::kCozette24; }
inline const ui::Font &row_font() { return ui::fonts::kCozette32; }
inline const ui::Font &headline_font() { return ui::fonts::kCozette96; }

inline constexpr ui::Color kInk = ui::Color::Black;
inline constexpr ui::Color kPaper = ui::Color::White;

} // namespace app::theme
