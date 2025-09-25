#pragma once

#include <exception>
#include <optional>
#include <variant>

template <typename T>
struct Result {
  constexpr bool has_value() const noexcept { return std::get_if<std::monostate>(&result_) == nullptr; }
  template <typename R>
  constexpr void set_value(R&& value) noexcept {
    result_.template emplace<T>(std::forward<R>(value));
  }

  template <typename R>
  constexpr void return_value(R&& value) noexcept {
    return set_value(std::forward<R>(value));
  }

  constexpr T result() & {
    if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
      std::rethrow_exception(*exception);
    }
    if (auto res = std::get_if<T>(&result_)) {
      return *res;
    }
    throw std::runtime_error("result not set");
  }
  constexpr T result() && {
    if (auto exception = std::get_if<std::exception_ptr>(&result_)) {
      std::rethrow_exception(*exception);
    }
    if (auto res = std::get_if<T>(&result_)) {
      return std::move(*res);
    }
    throw std::runtime_error("result not set");
  }

  void set_exception(std::exception_ptr exception) noexcept { result_ = exception; }
  void unhandled_exception() noexcept { result_ = std::current_exception(); }

 private:
  std::variant<std::monostate, T, std::exception_ptr> result_;
};

template <>
struct Result<void> {
  constexpr bool has_value() const noexcept { return result_.has_value(); }
  void return_void() noexcept { result_.emplace(nullptr); }
  void result() {
    if (result_.has_value() && *result_ != nullptr) {
      std::rethrow_exception(*result_);
    }
  }

  void set_exception(std::exception_ptr exception) noexcept { result_ = exception; }
  void unhandled_exception() noexcept { result_ = std::current_exception(); }

 private:
  std::optional<std::exception_ptr> result_;
};
