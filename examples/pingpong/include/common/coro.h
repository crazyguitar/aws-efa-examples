#pragma once
#include <coroutine>
#include <exception>
#include <utility>

namespace coro {

struct final_awaiter {
  bool await_ready() const noexcept { return false; }
  void await_resume() noexcept {}

  template <typename P>
  auto await_suspend(std::coroutine_handle<P> handle) noexcept {
    auto next = handle.promise().next;
    return !!next ? next : std::noop_coroutine();
  }
};

template <typename T>
struct promise_base {
  T ret;
  using coro = std::coroutine_handle<promise_base<T>>;
  std::exception_ptr exception;
  std::coroutine_handle<> next;
  auto get_return_object() { return coro::from_promise(*this); }
  auto initial_suspend() { return std::suspend_always(); }
  auto final_suspend() noexcept { return final_awaiter{}; }
  auto unhandled_exception() { exception = std::current_exception(); }
  auto return_value(T &&value) { ret = std::move(value); }
  auto result() {
    if (exception != nullptr) {
      std::rethrow_exception(exception);
    }
    return ret;
  }
};

template <>
struct promise_base<void> {
  using coro = std::coroutine_handle<promise_base<void>>;
  std::exception_ptr exception;
  std::coroutine_handle<> next;
  auto get_return_object() { return coro::from_promise(*this); }
  auto initial_suspend() { return std::suspend_always(); }
  auto final_suspend() noexcept { return final_awaiter{}; }
  auto unhandled_exception() { exception = std::current_exception(); }
  void return_void() {}
  void result() {
    if (exception != nullptr) {
      std::rethrow_exception(exception);
    }
  }
};

template <typename T = void>
class Coro {
 public:
  using promise_type = promise_base<T>;
  using coro = std::coroutine_handle<promise_type>;

  struct Awaiter {
    coro handle;
    Awaiter(coro h) : handle{h} {}
    bool await_ready() const noexcept { return !handle or handle.done(); }
    auto await_resume() const { return handle.promise().result(); }
    auto await_suspend(std::coroutine_handle<> coroutine) noexcept {
      handle.promise().next = coroutine;
      return handle;
    }
  };

  Coro(coro h) : handle_{h} {}
  ~Coro() {
    if (handle_) {
      handle_.destroy();
    }
  }

  auto operator co_await() const { return Awaiter{handle_}; }
  explicit operator bool() { return !handle_.done(); }
  void resume() {
    if (!handle_ or handle_.done()) return;
    handle_.resume();
  }

 private:
  coro handle_;
};

namespace oneway {

struct Coro {
  struct promise_type {
    auto get_return_object() { return Coro{}; }
    auto initial_suspend() noexcept { return std::suspend_never(); }
    auto final_suspend() noexcept { return std::suspend_never(); }
    void unhandled_exception() {}
    void return_void() noexcept {}
  };
};

}  // namespace oneway

}  // namespace coro
