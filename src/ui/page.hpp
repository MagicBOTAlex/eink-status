#pragma once

#include <string>

#include "ui/component.hpp"

namespace ui {

// A named screen. Pages own their component tree and know nothing about the
// panel, so a page can be rendered into any Canvas: the device's frame buffer,
// a host test buffer, or the preview tool.
class Page : public Container {
public:
  explicit Page(std::string name, ParamSet params = {});

  const char *type_name() const override { return "Page"; }
  const std::string &name() const { return name_; }

  // The page always covers the whole screen; children are laid out inside it.
  void layout(int width, int height);

  std::vector<ParamSpec> schema() const override;

private:
  std::string name_;
};

} // namespace ui
