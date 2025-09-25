#pragma once

struct Handle {
  enum State : uint8_t { kUnschedule, kScheduled, kSuspend };

  Handle() : id_{seq_++} {}
  virtual ~Handle() = default;
  virtual void run() = 0;

  void SetState(State state) { state_ = state; }
  State GetState() noexcept { return state_; }
  uint64_t GetId() noexcept { return id_; }

 private:
  static inline uint64_t seq_{0};
  uint64_t id_;
  State state_ = Handle::kUnschedule;
};
