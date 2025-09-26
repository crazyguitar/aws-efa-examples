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
#include "common/selector.h"
#include "common/utils.h"

/**
 * @brief Asynchronous I/O event loop with task scheduling
 */
class IO : private NoCopy {
 public:
  using milliseconds = std::chrono::milliseconds;
  using task_type = std::tuple<milliseconds, uint64_t, Handle *>;
  using priority_queue = std::priority_queue<task_type, std::vector<task_type>, std::greater<task_type> >;

  IO() : start_{std::chrono::system_clock::now()} {}

  /**
   * @brief Get singleton IO instance
   * @return Reference to the IO singleton
   */
  inline static IO &Get() {
    static IO io;
    return io;
  }

  /**
   * @brief Get current time since IO start
   * @return Time in milliseconds
   */
  milliseconds Time() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
  }

  /**
   * @brief Cancel a scheduled handle (TODO: implementation)
   * @param handle Handle to cancel
   */
  void Cancel(Handle &) { /* TODO */ }

  /**
   * @brief Schedule handle for immediate execution
   * @param handle Handle to execute
   */
  void Call(Handle &handle) {
    handle.SetState(Handle::kScheduled);
    ready_.emplace_back(std::addressof(handle));
  }

  /**
   * @brief Schedule handle for delayed execution
   * @param delay Time delay before execution
   * @param handle Handle to execute
   */
  template <typename Rep, typename Period>
  void Call(std::chrono::duration<Rep, Period> delay, Handle &handle) {
    handle.SetState(Handle::kScheduled);
    auto when = Time() + duration_cast<milliseconds>(delay);
    schedule_.push(task_type{when, handle.GetId(), std::addressof(handle)});
  }

  /**
   * @brief Run the event loop until stopped
   */
  inline void Run() {
    while (!Stopped()) {
      Select();
      Runone();
    }
  }

  /**
   * @brief Poll for I/O events and schedule ready handles
   */
  inline void Select() {
    auto events = selector_.Select();
    for (auto &e : events) {
      Call(*e.handle);
    }
  }

  /**
   * @brief Execute one iteration of scheduled tasks
   */
  inline void Runone() {
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

  /**
   * @brief Check if event loop should stop
   * @return true if no pending tasks or events
   */
  inline bool Stopped() const noexcept { return schedule_.empty() and ready_.empty() and selector_.Stopped(); }

  /**
   * @brief Register event source with selector
   * @param event Event source to register
   */
  template <typename T>
  inline void Register(T &&event) {
    selector_.Register(event);
  }

  /**
   * @brief Unregister event source from selector
   * @param event Event source to unregister
   */
  template <typename T>
  inline void UnRegister(T &&event) {
    selector_.UnRegister(event);
  }

 private:
  std::chrono::time_point<std::chrono::system_clock> start_;
  Selector selector_;
  priority_queue schedule_;
  std::deque<Handle *> ready_;
};
