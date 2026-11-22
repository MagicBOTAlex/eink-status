#include "control/commands.hpp"

#include <utility>

namespace control {

void Queue::post(const Command &command) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_ = command;
}

Command Queue::take() {
  std::lock_guard<std::mutex> lock(mutex_);
  Command out = std::move(pending_);
  pending_ = Command{};
  return out;
}

bool Queue::pending() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return pending_.action != Action::None;
}

Queue &queue() {
  static Queue instance;
  return instance;
}

} // namespace control
