#include "ui/component.hpp"

#include <cstdio>

namespace ui {

Component::Component(std::string id, ParamSet params)
    : id_(std::move(id)), params_(std::move(params)) {}

Component &Component::set(std::string key, Param value) {
  params_.set(std::move(key), std::move(value));
  ++generation_;
  return *this;
}

Component &Component::set(const ParamSet &values) {
  for (size_t i = 0; i < values.size(); ++i) {
    params_.set(values.key_at(i), values.value_at(i));
  }
  ++generation_;
  return *this;
}

const Param *Component::param(const std::string &key) const {
  return params_.find(key);
}

bool Component::has_param(const std::string &key) const {
  return params_.has(key);
}

Size Component::measure(const Size &) const {
  return {params_.get_int(kW, 0), params_.get_int(kH, 0)};
}

void Component::layout(const Rect &parent) {
  int w = params_.get_int(kW, 0);
  int h = params_.get_int(kH, 0);

  if (w <= 0 || h <= 0) {
    const Size content = measure({parent.w, parent.h});
    if (w <= 0) {
      w = content.w;
    }
    if (h <= 0) {
      h = content.h;
    }
  }

  const Align align = params_.get_align(kAnchor, Align::TopLeft);
  bounds_ = anchor_rect(parent, {w, h}, align, params_.get_int(kX, 0),
                        params_.get_int(kY, 0));

  on_layout();

  for (auto &child : children_) {
    child->layout(bounds_);
  }
}

void Component::update(uint32_t now_ms) {
  if (!visible()) {
    return;
  }
  update_self(now_ms);
  for (auto &child : children_) {
    child->update(now_ms);
  }
}

void Component::draw(Canvas &canvas) const {
  if (!visible()) {
    return;
  }
  draw_self(canvas);
  for (const auto &child : children_) {
    child->draw(canvas);
  }
}

bool Component::add_child(std::unique_ptr<Component>) { return false; }

std::vector<ParamSpec> Component::schema() const {
  return {
      {kX, ParamType::Int, false, Param(0), -8192, 8192},
      {kY, ParamType::Int, false, Param(0), -8192, 8192},
      {kW, ParamType::Int, false, Param(0), 0, 8192},
      {kH, ParamType::Int, false, Param(0), 0, 8192},
      {kAnchor, ParamType::Align, false, Param(Align::TopLeft)},
      {kVisible, ParamType::Bool, false, Param(true)},
  };
}

std::vector<ParamError> Component::validate() const {
  return validate_params(params_, schema());
}

std::string Component::describe() const {
  char head[96];
  std::snprintf(head, sizeof(head), "%s#%s %s", type_name(), id_.c_str(),
                bounds_.to_string().c_str());

  std::string out = head;
  if (params_.size()) {
    out += " ";
    out += params_.dump();
  }
  return out;
}

std::string Component::describe_tree(int indent) const {
  std::string out(static_cast<size_t>(indent) * 2, ' ');
  out += describe();
  out += "\n";
  for (const auto &child : children_) {
    out += child->describe_tree(indent + 1);
  }
  return out;
}

std::vector<ParamSpec> Container::schema() const {
  std::vector<ParamSpec> specs = Component::schema();
  specs.push_back({kBg, ParamType::Color, false, Param(Color::White)});
  specs.push_back({kBorder, ParamType::Int, false, Param(0), 0, 64});
  specs.push_back({kBorderColor, ParamType::Color, false, Param(Color::Black)});
  return specs;
}

bool Container::add_child(std::unique_ptr<Component> child) {
  if (!child) {
    return false;
  }
  children_.push_back(std::move(child));
  return true;
}

void Container::draw_self(Canvas &canvas) const {
  if (bounds_.empty()) {
    return;
  }
  const Param *bg = param(kBg);
  if (bg && !bg->empty()) {
    canvas.fill_rect(bounds_, bg->as_color(Color::White));
  }
  const int border = params_.get_int(kBorder, 0);
  if (border > 0) {
    canvas.stroke_rect(bounds_, params_.get_color(kBorderColor, Color::Black),
                       border);
  }
}

} // namespace ui
