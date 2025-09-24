#pragma once
#include <atomic>
#include <memory>

#include "common/selector.h"

class IO {
 public:
  IO() : selector_{std::make_unique<Selector>()} {}

  void Run() {
    running_ = true;
    while (running_) {
      selector_->Selector();
    }
  }

  void Stop() { running_ = false; }
  void Register(struct fid_cq *cq) { selector_->Register(cq); }

 private:
  std::atomic<bool> running_ = false;
  std::unique_ptr<Selector> selector_;
};
