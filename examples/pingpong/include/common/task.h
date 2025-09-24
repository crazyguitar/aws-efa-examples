#pragma once
#include <atomic>
#include <chrono>

#include "common/coro.h"

enum class TaskState : uint8_t { kUnschedule, kSuspending, kRunning };

class Task {
 public:
  Task() : start_{std::chrono::system_clock::now()}, timeout_{std::chrono::milliseconds(0)} {}
  Task(std::chrono::milliseconds timeout) : start_{std::chrono::system_clock::now()}, timeout_{timeout} {}

  ~Task() {
    if (!coroutine_) return;
    coroutine_.destroy();
  }

  inline void Suspend(std::coroutine_handle<> coroutine) {
    state_ = TaskState::kSuspending;
    coroutine_ = coroutine;
  }

  inline void Resume() {
    if (Done()) return;
    state_ = TaskState::kRunning;
    coroutine_.resume();
  }

  inline bool Unschedule() noexcept { return state_ == TaskState::kUnschedule; }
  inline bool Suspending() noexcept { return state_ == TaskState::kSuspending; }
  inline bool Running() noexcept { return state_ == TaskState::kRunning; }
  inline bool Done() { return !coroutine_ or coroutine_.done(); }
  inline bool Expired(std::chrono::time_point<std::chrono::system_clock> now) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_) > timeout_;
  }

 private:
  std::atomic<TaskState> state_ = TaskState::kUnschedule;
  std::coroutine_handle<> coroutine_ = nullptr;
  std::chrono::time_point<std::chrono::system_clock> start_;
  std::chrono::milliseconds timeout_;
};
