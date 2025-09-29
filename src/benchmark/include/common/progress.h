#pragma once
#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>

class Progress {
 public:
  using nanoseconds = std::chrono::nanoseconds;
  using seconds = std::chrono::seconds;

  inline constexpr static double Gb = 8.0 / 1e9;

  Progress() = default;
  Progress(size_t total_ops, size_t total_bw) : total_ops_{total_ops}, total_bw_{total_bw} {}

  void Print(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end, size_t size, uint64_t ops) {
    PrintProgress(start, end, size, ops, total_ops_, total_bw_);
  }

 public:
  // clang-format off
  inline static void PrintProgress(
    std::chrono::high_resolution_clock::time_point start,
    std::chrono::high_resolution_clock::time_point end,
    size_t size,
    uint64_t ops,
    uint64_t total_ops,
    size_t total_bw
  ) {
    auto elapse = duration_cast<nanoseconds>(end - start).count() / 1e9;
    auto bytes = size * ops;
    auto total_bytes = size * total_ops;
    auto gbps = bytes * Gb / elapse;
    auto bw = gbps / (total_bw * 1e9);
    auto p = gbps / (total_bw * 1e-9);
    std::cout << fmt::format("\r[{:.3f}] ops={}/{} bytes={}/{} bw={:.3f}Gbps({:.1f})\033[K", elapse, ops, total_ops, bytes, total_bytes, bw, p);
  }
  // clang-format on

 private:
  size_t total_ops_ = 0;
  size_t total_bw_ = 0;
};
