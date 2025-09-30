#pragma once
#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>

class Progress {
 public:
  using nanoseconds = std::chrono::nanoseconds;
  using seconds = std::chrono::seconds;

  inline constexpr static double Gb = 8.0f / 1e9;

  Progress() = default;
  Progress(size_t total_ops, size_t total_bw) : total_ops_{total_ops}, total_bw_{total_bw} {}

  void Print(std::chrono::high_resolution_clock::time_point now, size_t size, uint64_t ops) {
    PrintProgress(start_, now, size, ops, total_ops_, total_bw_);
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
    auto bw_gbps = bytes * Gb / elapse;
    auto total_bw_gbs = total_bw * 1e-9;
    auto percent = 100.0 * bw_gbps / (total_bw_gbs);
    std::cout << fmt::format("\r[{:.3f}s] ops={}/{} bytes={}/{} bw={:.3f}Gbps({:.1f})\033[K", elapse, ops, total_ops, bytes, total_bytes, bw_gbps, percent) << std::flush;
  }
  // clang-format on

 private:
  size_t total_ops_ = 0;
  size_t total_bw_ = 0;
  std::chrono::high_resolution_clock::time_point start_{std::chrono::high_resolution_clock::now()};
};
