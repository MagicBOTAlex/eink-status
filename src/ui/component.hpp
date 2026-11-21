#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ui/canvas.hpp"
#include "ui/param.hpp"

namespace ui {

// Base class for every drawable thing on a page.
//
// Parameters are the only configuration channel: the same `Component::set`
// calls configure a label on the panel and in a host test. Subclasses publish
// the parameters they accept through `schema()`, and `validate()` reports
// unknown keys, missing required keys, wrong types and range errors.
//
// A component is laid out inside a parent rect using the common geometry
// parameters (x, y, w, h, anchor). With w/h unset (or <= 0) the component asks
// `measure()` for its content size, so labels size themselves from their text.
class Component {
public:
  Component(std::string id, ParamSet params = {});
  virtual ~Component() = default;

  Component(const Component &) = delete;
  Component &operator=(const Component &) = delete;

  const std::string &id() const { return id_; }
  virtual const char *type_name() const = 0;

  const ParamSet &params() const { return params_; }
  ParamSet &params() { return params_; }
  Component &set(std::string key, Param value);
  Component &set(const ParamSet &values);
  const Param *param(const std::string &key) const;
  bool has_param(const std::string &key) const;

  bool visible() const { return params_.get_bool(kVisible, true); }
  Component &set_visible(bool value) { return set(kVisible, Param(value)); }

  // Content size when w/h are not given by parameters.
  virtual Size measure(const Size &available) const;

  // Resolves geometry inside `parent`, then lays out children.
  void layout(const Rect &parent);
  const Rect &bounds() const { return bounds_; }
  void set_bounds(const Rect &bounds) { bounds_ = bounds; }

  // Bounds at the previous present(). The app diffs the union of this and the
  // current bounds so a component that shrinks or disappears still reports the
  // pixels it used to cover.
  const Rect &previous_bounds() const { return previous_bounds_; }
  void commit_bounds() { previous_bounds_ = bounds_; }

  // Bumped whenever a parameter changes, so the app can report a container
  // whose background or border changed even though its children did not.
  uint32_t generation() const { return generation_; }
  uint32_t presented_generation() const { return presented_generation_; }
  void commit_generation() { presented_generation_ = generation_; }

  // Runs the component's time-based behaviour, then the children's.
  void update(uint32_t now_ms);

  // Draws the component and, if visible, its children.
  void draw(Canvas &canvas) const;

  const std::vector<std::unique_ptr<Component>> &children() const {
    return children_;
  }

  virtual std::vector<ParamSpec> schema() const;
  std::vector<ParamError> validate() const;

  // One-line description including parameters, for logs and failing tests.
  std::string describe() const;
  std::string describe_tree(int indent = 0) const;

  // Visits every component in the tree, visible or not.
  template <typename Fn> void for_each(Fn &&visitor) {
    visitor(*this);
    for (auto &child : children_) {
      child->for_each(visitor);
    }
  }
  template <typename Fn> void for_each(Fn &&visitor) const {
    visitor(*this);
    for (const auto &child : children_) {
      child->for_each(visitor);
    }
  }

  // Visits this component and every visible child (used by tests to inspect a
  // tree).
  template <typename Fn> void for_each_visible(Fn &&visitor) const {
    if (!visible()) {
      return;
    }
    visitor(*this);
    for (const auto &child : children_) {
      child->for_each_visible(visitor);
    }
  }

  // Common parameter names.
  static constexpr const char *kX = "x";
  static constexpr const char *kY = "y";
  static constexpr const char *kW = "w";
  static constexpr const char *kH = "h";
  static constexpr const char *kAnchor = "anchor";
  static constexpr const char *kVisible = "visible";

protected:
  // Adds `child` to this component. Leaf components reject children and
  // return false; containers accept them.
  virtual bool add_child(std::unique_ptr<Component> child);

  virtual void on_layout() {}
  virtual void draw_self(Canvas &) const {}
  virtual void update_self(uint32_t) {}

  std::string id_;
  ParamSet params_;
  Rect bounds_{};
  Rect previous_bounds_{};
  uint32_t generation_ = 0;
  uint32_t presented_generation_ = 0;
  std::vector<std::unique_ptr<Component>> children_;
};

// A component that owns children and paints an optional background and border.
class Container : public Component {
public:
  using Component::Component;

  const char *type_name() const override { return "Container"; }

  bool add_child(std::unique_ptr<Component> child) override;

  template <typename T, typename... Args> T &add(Args &&...args) {
    auto owned = std::make_unique<T>(std::forward<Args>(args)...);
    T &ref = *owned;
    children_.push_back(std::move(owned));
    return ref;
  }

  std::vector<ParamSpec> schema() const override;

  static constexpr const char *kBg = "bg";
  static constexpr const char *kBorder = "border";
  static constexpr const char *kBorderColor = "border_color";

protected:
  void draw_self(Canvas &canvas) const override;
};

} // namespace ui
