#include "ui/page.hpp"

namespace ui {

Page::Page(std::string name, ParamSet params)
    : Container("page:" + name, std::move(params)), name_(std::move(name)) {
  // Pages are paper: repaint the whole screen unless told otherwise. This is
  // what makes a component that shrinks or disappears erase cleanly.
  if (!params_.has(Container::kBg)) {
    params_.set(Container::kBg, Param(Color::White));
  }
}

void Page::layout(int width, int height) {
  bounds_ = {0, 0, width, height};
  on_layout();
  for (auto &child : children_) {
    child->layout(bounds_);
  }
}

std::vector<ParamSpec> Page::schema() const {
  std::vector<ParamSpec> specs = Container::schema();
  // A page is never positioned or sized by parameters.
  std::vector<ParamSpec> out;
  for (const auto &spec : specs) {
    if (spec.name == kX || spec.name == kY || spec.name == kW ||
        spec.name == kH || spec.name == kAnchor) {
      continue;
    }
    out.push_back(spec);
  }
  return out;
}

} // namespace ui
