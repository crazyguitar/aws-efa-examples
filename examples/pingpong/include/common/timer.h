#pragma once

#include "common/io.h"
#include "common/task.h"

class Timer {
 public:
  struct sleep_awaiter {
    IO &io_;
    std::chrono::milliseconds timeout_;
    sleep_awaiter(std::chrono::milliseconds timeout, IO &io) : io_{io}, timeout_{timeout} {}
    bool await_ready() const noexcept { return false; }
    void await_resume() noexcept {}
    bool await_suspend(std::coroutine_handle<> coroutine) {
      auto task = std::make_shared<Task>(timeout_);
      task->Suspend(coroutine);
      io_.Schedule(task);
      return true;
    }
  };

  inline static sleep_awaiter Sleep(std::chrono::milliseconds duration, IO &io) { return sleep_awaiter(duration, io); }
};
