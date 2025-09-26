#pragma once

#include "common/utils.h"

template <typename C>
class Future : private NoCopy {
 public:
  explicit Future(C &&coro) : coro_{std::forward<C>(coro)} {
    if (coro_.valid() and !coro_.done()) {
      coro_.handle_.promise().schedule();
    }
  }

  inline void Cancel() { coro_.destroy(); }
  decltype(auto) operator co_await() const & noexcept { return coro_.operator co_await(); }
  auto operator co_await() const && noexcept { return coro_.operator co_await(); }
  decltype(auto) result() & { return coro_.result(); }
  decltype(auto) result() && { return std::move(coro_).result(); }
  inline bool valid() const { return coro_.valid(); }
  inline bool done() const { return coro_.done(); }

 private:
  C coro_;
};
