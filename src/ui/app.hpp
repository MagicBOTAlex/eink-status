#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ui/canvas.hpp"
#include "ui/page.hpp"

namespace ui {

// Owns the page stack and turns "what the UI looks like" into dirty regions.
//
// The device and the tests drive the same object:
//   app.tick(now);                        // run time-based updates
//   auto regions = app.present(canvas);   // draw + diff against last frame
// and the driver only has to refresh `regions`.
class UiApp {
public:
  UiApp(int width, int height);
  ~UiApp();

  UiApp(const UiApp &) = delete;
  UiApp &operator=(const UiApp &) = delete;

  Page &add_page(std::unique_ptr<Page> page);
  bool show_page(const std::string &name);

  Page *current_page() { return current_; }
  const Page *current_page() const { return current_; }
  const std::string &page_name() const;

  size_t page_count() const { return pages_.size(); }
  Page *page_at(size_t index) { return pages_[index].get(); }

  // Runs update() on the current page with a monotonic millisecond clock.
  void tick(uint32_t now_ms);

  // Draws the current page into `canvas` and returns the changed regions.
  // The first call, a page switch and invalidate() report the whole screen.
  std::vector<Rect> present(Canvas &canvas);

  // Forces the next present() to report the whole screen.
  void invalidate() { full_ = true; }

  // True when the page changed since the last present().
  bool page_changed() const { return page_changed_; }

  const std::vector<Rect> &dirty() const { return dirty_; }

  // True when the visible tree changed since the last present(), so the device
  // can skip redrawing an idle screen.
  bool needs_present() const;

  // Parameter problems in every registered page, computed on demand so
  // components added after add_page() are still covered. Reported by the device
  // log; tests assert the list is empty for real pages.
  struct ValidationIssue {
    std::string component;
    ParamError error;
  };
  std::vector<ValidationIssue> validation_errors() const;

  int width() const { return width_; }
  int height() const { return height_; }

  std::string describe() const;

private:
  int width_;
  int height_;
  std::vector<std::unique_ptr<Page>> pages_;
  Page *current_ = nullptr;
  std::vector<uint8_t> previous_;
  int previous_stride_ = 0;
  bool full_ = true;
  bool page_changed_ = false;
  std::vector<Rect> dirty_;
};

} // namespace ui
