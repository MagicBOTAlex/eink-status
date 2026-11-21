#include "ui/components.hpp"

namespace ui {

Label::Label(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

Size Label::measure(const Size &) const {
  const Font *f = font();
  if (f == nullptr) {
    return {0, 0};
  }
  return {font_text_width(*f, text()), f->line_height};
}

std::vector<ParamSpec> Label::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kText, ParamType::Text, true, Param("")});
  specs.push_back({kFont, ParamType::Font, true, Param(static_cast<const Font *>(nullptr))});
  specs.push_back({kColor, ParamType::Color, false, Param(Color::Black)});
  specs.push_back({kHAlign, ParamType::HAlign, false, Param(HAlign::Left)});
  specs.push_back({kVAlign, ParamType::VAlign, false, Param(VAlign::Top)});
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
  specs.push_back({kColor, ParamType::Color, false, Param()});
  specs.push_back({kBorder, ParamType::Int, false, Param(0), 0, 64});
  specs.push_back({kBorderColor, ParamType::Color, false, Param(Color::Black)});
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

Rule::Rule(std::string id, ParamSet params)
    : Component(std::move(id), std::move(params)) {}

std::vector<ParamSpec> Rule::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kColor, ParamType::Color, false, Param(Color::Black)});
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
