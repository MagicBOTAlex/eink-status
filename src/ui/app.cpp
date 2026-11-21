#include "ui/app.hpp"

#include <cstring>

namespace ui {

UiApp::UiApp(int width, int height) : width_(width), height_(height) {
  previous_stride_ = (width + 7) / 8;
}

UiApp::~UiApp() = default;

Page &UiApp::add_page(std::unique_ptr<Page> page) {
  Page *raw = page.get();
  pages_.push_back(std::move(page));
  if (current_ == nullptr) {
    current_ = raw;
    full_ = true;
    page_changed_ = true;
  }
  return *raw;
}

bool UiApp::show_page(const std::string &name) {
  for (auto &page : pages_) {
    if (page->name() == name) {
      if (current_ != page.get()) {
        current_ = page.get();
        full_ = true;
        page_changed_ = true;
      }
      return true;
    }
  }
  return false;
}

const std::string &UiApp::page_name() const {
  static const std::string empty;
  return current_ ? current_->name() : empty;
}

void UiApp::tick(uint32_t now_ms) {
  if (current_) {
    current_->update(now_ms);
  }
}

std::vector<Rect> UiApp::present(Canvas &canvas) {
  dirty_.clear();
  if (current_ == nullptr) {
    return dirty_;
  }

  // A mismatch here would read past the caller's buffer while diffing.
  if (canvas.width() != width_ || canvas.height() != height_ ||
      canvas.stride() < previous_stride_) {
    return dirty_;
  }

  current_->layout(width_, height_);
  current_->draw(canvas);

  if (previous_.empty()) {
    previous_.assign(static_cast<size_t>(previous_stride_) * height_, 0xFF);
  }
  Canvas previous(previous_.data(), width_, height_, previous_stride_);

  if (full_) {
    dirty_.push_back({0, 0, width_, height_});
  } else {
    std::vector<Rect> changed;
    current_->for_each([&](const auto &component) {
      const bool params_changed =
          component.generation() != component.presented_generation();

      // An unchanged hidden component has nothing to say. An unchanged
      // container defers to its children, which own the pixels.
      if (!params_changed) {
        if (!component.visible()) {
          return;
        }
        if (!component.children().empty()) {
          return;
        }
      }

      // A component that shrank or was hidden still owns the pixels it used
      // to cover, so compare the union of the old and the new bounds.
      Rect region = component.previous_bounds();
      if (component.visible()) {
        region = region.unite(component.bounds());
      }
      region = region.intersect(canvas.bounds());
      if (region.empty()) {
        return;
      }
      // Pixels decide. A no-op set(), or a background change hidden behind a
      // child, must not trigger a refresh.
      if (!previous.region_equal(canvas, region)) {
        changed.push_back(region);
      }
    });
    dirty_ = merge_rects(std::move(changed), 8);
  }

  for (int y = 0; y < height_; ++y) {
    std::memcpy(previous_.data() + static_cast<size_t>(y) * previous_stride_,
                canvas.pixels() + static_cast<size_t>(y) * canvas.stride(),
                static_cast<size_t>(previous_stride_));
  }

  current_->for_each([](auto &component) {
    component.commit_bounds();
    component.commit_generation();
  });

  full_ = false;
  page_changed_ = false;
  return dirty_;
}

std::vector<UiApp::ValidationIssue> UiApp::validation_errors() const {
  std::vector<ValidationIssue> issues;
  for (const auto &page : pages_) {
    page->layout(width_, height_);
    page->for_each([&issues](const Component &component) {
      for (const ParamError &error : component.validate()) {
        issues.push_back(
            {std::string(component.type_name()) + "#" + component.id(), error});
      }
    });
  }
  return issues;
}

bool UiApp::needs_present() const {
  if (current_ == nullptr) {
    return false;
  }
  if (full_) {
    return true;
  }
  bool dirty = false;
  current_->for_each([&](const auto &component) {
    if (!dirty && component.generation() != component.presented_generation()) {
      dirty = true;
    }
  });
  return dirty;
}

std::string UiApp::describe() const {
  if (current_) {
    current_->layout(width_, height_);
  }
  std::string out = "UiApp " + std::to_string(width_) + "x" +
                    std::to_string(height_) + " pages=" +
                    std::to_string(pages_.size()) + "\n";
  for (const auto &page : pages_) {
    out += (page.get() == current_ ? "* " : "  ") + page->describe_tree(1);
  }
  return out;
}

} // namespace ui
