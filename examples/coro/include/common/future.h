#pragma once

#include "common/utils.h"

/**
 * @brief Future wrapper for coroutines with automatic scheduling
 * @tparam C Coroutine type
 */
template <typename C>
class Future : private NoCopy {
 public:
  /**
   * @brief Construct future from coroutine and schedule if needed
   * @param coro Coroutine to wrap
   */
  explicit Future(C &&coro) : coro_{std::forward<C>(coro)} {
    if (coro_.valid() and !coro_.done()) {
      coro_.handle_.promise().schedule();
    }
  }

  /** @brief Cancel the underlying coroutine */
  inline void Cancel() { coro_.destroy(); }

  /** @brief Make future awaitable (lvalue) */
  decltype(auto) operator co_await() const & noexcept { return coro_.operator co_await(); }

  /** @brief Make future awaitable (rvalue) */
  auto operator co_await() const && noexcept { return coro_.operator co_await(); }

  /** @brief Get result (lvalue) */
  decltype(auto) result() & { return coro_.result(); }

  /** @brief Get result (rvalue) */
  decltype(auto) result() && { return std::move(coro_).result(); }

  /** @brief Check if coroutine is valid */
  inline bool valid() const { return coro_.valid(); }

  /** @brief Check if coroutine is done */
  inline bool done() const { return coro_.done(); }

 private:
  C coro_;
};
