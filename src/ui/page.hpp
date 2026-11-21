#pragma once

#include <string>

#include "ui/component.hpp"

namespace ui {

// What the panel is doing, published to the current page so it can show state
// without the driver knowing anything about pages.
enum class DisplayMode { Partial, Full, Error };

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

  // Called when the panel starts/finishes a full refresh or fails one.
  virtual void on_display_mode(DisplayMode mode) { (void)mode; }

  std::vector<ParamSpec> schema() const override;

private:
  std::string name_;
};

} // namespace ui
