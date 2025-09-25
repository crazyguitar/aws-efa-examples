#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <queue>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/handle.h"
#include "common/utils.h"

class IO : private NoCopy {
 public:
  using milliseconds = std::chrono::milliseconds;
  using task_type = std::tuple<milliseconds, uint64_t, Handle *>;
  using priority_queue = std::priority_queue<task_type, std::vector<task_type>, std::greater<task_type> >;

  inline static IO &Get() {
    static IO io;
    return io;
  }

  milliseconds Time() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
  }

  void Cancel(Handle &) { /* TODO */ }
  void Call(Handle &handle) {
    handle.SetState(Handle::kScheduled);
    ready_.emplace_back(std::addressof(handle));
  }

  template <typename Rep, typename Period>
  void Call(std::chrono::duration<Rep, Period> delay, Handle &handle) {
    handle.SetState(Handle::kScheduled);
    auto when = Time() + duration_cast<milliseconds>(delay);
    schedule_.push(task_type{when, handle.GetId(), std::addressof(handle)});
  }

  inline void Run() {
    while (!Stopped()) {
      Runone();
    }
  }

  void Runone() {
    auto now = Time();
    while (!schedule_.empty()) {
      auto &task = schedule_.top();
      auto &when = std::get<0>(task);
      auto handle = std::get<2>(task);
      if (when > now) break;
      ready_.emplace_back(handle);
      schedule_.pop();
    }

    for (size_t n = ready_.size(), i = 0; i < n; ++i) {
      auto handle = ready_.front();
      ready_.pop_front();
      handle->SetState(Handle::kUnschedule);
      handle->run();
    }
  }

  bool Stopped() { return schedule_.empty() and ready_.empty(); }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_;

  priority_queue schedule_;
  std::deque<Handle *> ready_;
};
