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

      // Hidden components only matter when their parameters just changed, and
      // unchanged containers defer to their children, which own the pixels.
      if (!params_changed && !component.visible()) {
        return;
      }
      if (!params_changed && !component.children().empty()) {
        return;
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
      if (!params_changed && previous.region_equal(canvas, region)) {
        return;
      }
      changed.push_back(region);
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
