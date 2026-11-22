#pragma once

#include <mutex>
#include <string>

// Commands from the HTTP task to the UI task. The web server never touches the
// UI directly; it posts here and the main loop drains it once per iteration.
namespace control {

enum class Action {
  None,
  Refresh,   // force a full-screen repaint
  ProbeNow,  // wake the monitor prober
  Reboot,    // esp_restart, flushed through the main loop
  ShowPage,  // switch to Command::page
};

struct Command {
  Action action = Action::None;
  std::string page;
};

// A single pending slot: for a control panel the latest request wins, and a
// second press of the same button must not queue up a backlog.
class Queue {
public:
  void post(const Command &command);
  Command take();
  bool pending() const;

private:
  mutable std::mutex mutex_;
  Command pending_;
};

// Process-wide queue shared by the HTTP handlers and the main loop.
Queue &queue();

} // namespace control
