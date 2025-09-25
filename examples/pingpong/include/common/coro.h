#pragma once
#include <coroutine>
#include <exception>
#include <utility>

#include "common/handle.h"
#include "common/io.h"
#include "common/result.h"
#include "common/utils.h"

struct Oneway {};
inline constexpr Oneway oneway;

template <typename T = void>
struct Coro : private NoCopy {
  struct promise_type;
  using coro = std::coroutine_handle<promise_type>;

  template <typename C>
  friend class Future;

  explicit Coro(coro h) noexcept : handle_{h} {}
  Coro(Coro&& c) noexcept : handle_(std::exchange(c.handle_, {})) {}
  ~Coro() { Destroy(); }

  struct awaiter_base {
    coro h;
    constexpr bool await_ready() {
      if (h) return h.done();
      return true;
    }

    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> coroutine) const noexcept {
      coroutine.promise().SetState(Handle::kSuspend);
      h.promise().next = &coroutine.promise();
      h.promise().schedule();
    }
  };

  auto operator co_await() const& noexcept {
    struct awaiter : awaiter_base {
      decltype(auto) await_resume() const {
        if (!awaiter_base::h) throw std::runtime_error("invalid coro handler");
        return awaiter_base::h.promise().result();
      }
    };
    return awaiter{handle_};
  }

  auto operator co_await() const&& noexcept {
    struct awaiter : awaiter_base {
      decltype(auto) await_resume() const {
        if (!awaiter_base::h) throw std::runtime_error("invalid coro handler");
        return std::move(awaiter_base::h.promise()).result();
      }
    };
    return awaiter{handle_};
  }

  struct promise_type : Handle, Result<T> {
    promise_type() = default;

    template <typename... Args>
    promise_type(Oneway, Args&&...) : oneway_{true} {}

    auto initial_suspend() noexcept {
      struct init_awaiter {
        constexpr bool await_ready() const noexcept { return oneway_; }
        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
        constexpr void await_resume() const noexcept {}
        const bool oneway_{false};
      };
      return init_awaiter{oneway_};
    }

    struct final_awaiter {
      constexpr bool await_ready() const noexcept { return false; }
      constexpr void await_resume() const noexcept {}

      template <typename Promise>
      constexpr void await_suspend(std::coroutine_handle<Promise> h) const noexcept {
        if (auto next = h.promise().next) {
          IO::Get().Call(*next);
        }
      }
    };

    auto final_suspend() noexcept { return final_awaiter{}; };

    Coro get_return_object() noexcept { return Coro{coro::from_promise(*this)}; }
    void run() final { coro::from_promise(*this).resume(); }

    const bool oneway_{false};
    Handle* next{nullptr};
  };  // promise_type
      //
  bool valid() const { return handle_ != nullptr; }
  bool done() const { return handle_.done(); }

  decltype(auto) result() & { return handle_.promise().result(); }

  decltype(auto) result() && { return std::move(handle_.promise()).result(); }

 private:
  void Destroy() {
    if (auto handle = std::exchange(handle_, nullptr)) {
      handle.promise().cancel();
      handle.destroy();
    }
  }

 private:
  coro handle_;
};  // Coro
