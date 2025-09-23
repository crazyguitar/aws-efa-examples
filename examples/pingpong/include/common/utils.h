#pragma once

#include <rdma/fabric.h>
#include <spdlog/spdlog.h>

#define CHECK(exp)                                                               \
  do {                                                                           \
    auto rc = exp;                                                               \
    if (rc) {                                                                    \
      auto msg = fmt::format(#exp " fail. error({}): {}", rc, fi_strerror(-rc)); \
      SPDLOG_ERROR(msg);                                                         \
      throw std::runtime_error(msg);                                             \
    }                                                                            \
  } while (0)

#define ASSERT(exp)                                    \
  do {                                                 \
    if (!(exp)) {                                      \
      auto msg = fmt::format(#exp " assertion fail."); \
      SPDLOG_ERROR(msg);                               \
      throw std::runtime_error(msg);                   \
    }                                                  \
  } while (0)

#define ENDPOINT_IDX(rank) (rank * kMaxAddrSize)

constexpr size_t kMaxAddrSize = 64;

struct fi_info *GetInfo();
