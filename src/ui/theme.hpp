#pragma once

#include "ui/font.hpp"
#include "ui/fonts/cozette_16.hpp"
#include "ui/fonts/cozette_96.hpp"
#include "ui/types.hpp"

namespace app::theme {

inline constexpr int kWidth = 648;
inline constexpr int kHeight = 480;
inline constexpr int kMargin = 16;

inline const ui::Font &small_font() { return ui::fonts::kCozette16; }
inline const ui::Font &headline_font() { return ui::fonts::kCozette96; }

inline constexpr ui::Color kInk = ui::Color::Black;
inline constexpr ui::Color kPaper = ui::Color::White;

} // namespace app::theme
