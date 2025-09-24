#pragma once
#include <chrono>

#include "common/coro.h"

class Task {
 public:
  Task() : start_{std::chrono::system_clock::now()}, timeout_{std::chrono::milliseconds(0)} {}
  Task(std::chrono::milliseconds timeout) : start_{std::chrono::system_clock::now()}, timeout_{timeout} {}
  void Suspend(std::coroutine_handle<> coroutine) { coroutine_ = coroutine; }
  void Resume() {
    if (!coroutine_ or coroutine_.done()) return;
    coroutine_.resume();
  }

  bool Expired(std::chrono::time_point<std::chrono::system_clock> now) { return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_) > timeout_; }

 private:
  std::coroutine_handle<> coroutine_ = nullptr;
  std::chrono::time_point<std::chrono::system_clock> start_;
  std::chrono::milliseconds timeout_;
};
