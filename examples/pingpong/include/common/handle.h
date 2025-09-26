#pragma once
#include <spdlog/spdlog.h>

#include <source_location>

/**
 * @brief Base class for asynchronous task handles with state management
 */
struct Handle {
  /** @brief Handle execution states */
  enum State : uint8_t { kUnschedule, kScheduled, kSuspend };

  Handle() : id_{seq_++} {}
  virtual ~Handle() = default;

  /**
   * @brief Execute the handle's task
   */
  virtual void run() = 0;

  /**
   * @brief Set handle execution state
   * @param state New state to set
   */
  inline void SetState(State state) { state_ = state; }

  /**
   * @brief Get current execution state
   * @return Current state
   */
  inline State GetState() noexcept { return state_; }

  /**
   * @brief Get unique handle identifier
   * @return Handle ID
   */
  inline uint64_t GetId() noexcept { return id_; }

  /**
   * @brief Schedule handle for execution
   */
  void schedule();

  /**
   * @brief Cancel handle execution
   */
  void cancel();

 private:
  static inline uint64_t seq_{0};
  uint64_t id_;
  State state_ = Handle::kUnschedule;
};
