#include "ui/components.hpp"

namespace ui {

Label::Label(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

Size Label::measure(const Size &) const {
  const Font *f = font();
  if (f == nullptr) {
    return {0, 0};
  }
  // Ink extent, not advance sum: a glyph can overhang its advance.
  return {font_text_ink_width(*f, text()), f->line_height};
}

std::vector<ParamSpec> Label::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kText, ParamType::Text, true});
  specs.push_back({kFont, ParamType::Font, true});
  specs.push_back({kColor, ParamType::Color, false});
  specs.push_back({kHAlign, ParamType::HAlign, false});
  specs.push_back({kVAlign, ParamType::VAlign, false});
  return specs;
}

void Label::draw_self(Canvas &canvas) const {
  const Font *f = font();
  if (f == nullptr || bounds_.empty()) {
    return;
  }
  canvas.draw_text(*f, bounds_, text(), params_.get_halign(kHAlign, HAlign::Left),
                   params_.get_valign(kVAlign, VAlign::Top),
                   params_.get_color(kColor, Color::Black));
}

Box::Box(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

std::vector<ParamSpec> Box::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kColor, ParamType::Color, false});
  specs.push_back({kBorder, ParamType::Int, false, 0, 64});
  specs.push_back({kBorderColor, ParamType::Color, false});
  return specs;
}

void Box::draw_self(Canvas &canvas) const {
  if (bounds_.empty()) {
    return;
  }

  const Param *fill = param(kColor);
  if (fill && !fill->empty()) {
    canvas.fill_rect(bounds_, fill->as_color(Color::Black));
  }

  const int border = params_.get_int(kBorder, 0);
  if (border > 0) {
    canvas.stroke_rect(bounds_, params_.get_color(kBorderColor, Color::Black),
                       border);
  }
}

Icon::Icon(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

std::vector<ParamSpec> Icon::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kIcon, ParamType::Icon, true});
  specs.push_back({kColor, ParamType::Color, false});
  specs.push_back({kThickness, ParamType::Int, false, 1, 64});
  return specs;
}

Rect Icon::icon_area() const {
  const int side = bounds_.w < bounds_.h ? bounds_.w : bounds_.h;
  const int margin = side / 8;
  const int size = side - 2 * margin;
  return {bounds_.x + (bounds_.w - size) / 2,
          bounds_.y + (bounds_.h - size) / 2, size, size};
}

void Icon::draw_self(Canvas &canvas) const {
  const Rect area = icon_area();
  if (area.w < 4 || area.h < 4) {
    return;
  }

  const Color color = params_.get_color(kColor, Color::Black);
  int thickness = params_.get_int(kThickness, 0);
  if (thickness <= 0) {
    thickness = area.w / 6;
  }
  if (thickness < 1) {
    thickness = 1;
  }

  const int x = area.x;
  const int y = area.y;
  const int w = area.w;
  const int h = area.h;

  switch (kind()) {
  case IconKind::Check:
    canvas.line(x + w / 8, y + h * 5 / 8, x + w * 2 / 5, y + h * 7 / 8, color,
                thickness);
    canvas.line(x + w * 2 / 5, y + h * 7 / 8, x + w * 7 / 8, y + h / 8, color,
                thickness);
    break;
  case IconKind::Cross:
    canvas.line(x + w / 6, y + h / 6, x + w * 5 / 6, y + h * 5 / 6, color,
                thickness);
    canvas.line(x + w * 5 / 6, y + h / 6, x + w / 6, y + h * 5 / 6, color,
                thickness);
    break;
  case IconKind::Dash:
    canvas.line(x + w / 5, y + h / 2, x + w * 4 / 5, y + h / 2, color,
                thickness);
    break;
  }
}

Rule::Rule(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

Size Rule::measure(const Size &) const {
  return {params_.get_int(kW, 0), params_.get_int(kH, 1)};
}

std::vector<ParamSpec> Rule::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kColor, ParamType::Color, false});
  return specs;
}

void Rule::draw_self(Canvas &canvas) const {
  if (bounds_.empty()) {
    return;
  }
  canvas.fill_rect({bounds_.x, bounds_.y, bounds_.w, 1},
                   params_.get_color(kColor, Color::Black));
}

} // namespace ui
