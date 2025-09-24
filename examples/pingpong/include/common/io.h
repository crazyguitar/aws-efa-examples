#pragma once
#include <atomic>
#include <memory>
#include <unordered_set>
#include <vector>

#include "common/selector.h"
#include "common/task.h"

class IO {
 public:
  inline static IO &Get() {
    static IO io;
    return io;
  }

  IO(const IO &) = delete;
  IO(IO &&) = delete;
  IO &operator=(const IO &) = delete;
  IO &operator=(IO &&) = delete;

  inline void Run() {
    running_ = true;
    while (running_) {
      Select();
      Execute();
    }
  }

  inline void Stop() { running_ = false; }
  inline void Register(struct fid_cq *cq) { selector_->Register(cq); }
  inline void Schedule(std::shared_ptr<Task> task) { tasks_.emplace(task); }
  inline void Select() { selector_->Select(); }
  inline void Execute() {
    std::vector<std::shared_ptr<Task>> expired;
    auto now = std::chrono::system_clock::now();
    for (auto &t : tasks_) {
      if (not t->Expired(now)) continue;
      t->Resume();
      expired.emplace_back(t);
    }

    // remove expired tasks
    for (auto &t : expired) {
      tasks_.erase(t);
    }
  }

  inline void Spawn(auto &&coroutine) {
    [](auto &&c, IO *io) -> coro::oneway::Coro { co_await c; }(std::move(coroutine), this);
  }

 private:
  IO() : selector_{std::make_unique<Selector>()} {}
  ~IO() {}

 private:
  std::atomic<bool> running_ = false;
  std::unique_ptr<Selector> selector_;
  std::unordered_set<std::shared_ptr<Task>> tasks_;
};
