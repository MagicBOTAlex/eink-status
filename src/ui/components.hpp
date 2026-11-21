#pragma once

#include <string>

#include "ui/component.hpp"
#include "ui/font.hpp"

namespace ui {

// Text component. Sizes itself from the font when w/h are not supplied.
class Label : public Component {
public:
  Label(std::string id, ParamSet params);

  const char *type_name() const override { return "Label"; }

  std::string text() const { return params_.get_text(kText); }
  Label &set_text(std::string text) { return static_cast<Label &>(set(kText, Param(std::move(text)))); }
  Label &set_color(Color color) { return static_cast<Label &>(set(kColor, Param(color))); }

  const Font *font() const { return params_.get_font(kFont); }

  Size measure(const Size &available) const override;
  std::vector<ParamSpec> schema() const override;

  static constexpr const char *kText = "text";
  static constexpr const char *kFont = "font";
  static constexpr const char *kColor = "color";
  static constexpr const char *kHAlign = "halign";
  static constexpr const char *kVAlign = "valign";

protected:
  void draw_self(Canvas &canvas) const override;
};

// Filled and/or outlined rectangle. A `Box` without a `color` parameter only
// draws its border, so it can be used as a frame around other content.
class Box : public Component {
public:
  Box(std::string id, ParamSet params);

  const char *type_name() const override { return "Box"; }

  Box &set_color(Color color) { return static_cast<Box &>(set(kColor, Param(color))); }

  std::vector<ParamSpec> schema() const override;

  static constexpr const char *kColor = "color";
  static constexpr const char *kBorder = "border";
  static constexpr const char *kBorderColor = "border_color";

protected:
  void draw_self(Canvas &canvas) const override;
};

// Horizontal rule: a one-pixel-high box by default.
class Rule : public Component {
public:
  Rule(std::string id, ParamSet params);

  const char *type_name() const override { return "Rule"; }

  std::vector<ParamSpec> schema() const override;

  static constexpr const char *kColor = "color";

protected:
  void draw_self(Canvas &canvas) const override;
};

} // namespace ui
