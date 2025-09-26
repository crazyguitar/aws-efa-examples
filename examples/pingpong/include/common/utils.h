#pragma once

#include <rdma/fabric.h>
#include <spdlog/spdlog.h>

/**
 * @brief Check fabric operation return code and throw on error
 * @param exp Expression that returns fabric error code
 * @throws std::runtime_error with error message on failure
 */
#define CHECK(exp)                                                               \
  do {                                                                           \
    auto rc = exp;                                                               \
    if (rc) {                                                                    \
      auto msg = fmt::format(#exp " fail. error({}): {}", rc, fi_strerror(-rc)); \
      SPDLOG_ERROR(msg);                                                         \
      throw std::runtime_error(msg);                                             \
    }                                                                            \
  } while (0)

/**
 * @brief Verify fabric operation returns expected value
 * @param exp Expression to evaluate
 * @param expect Expected return value
 * @throws std::runtime_error on mismatch
 */
#define EXPECT(exp, expect)                                                      \
  do {                                                                           \
    auto rc = (exp);                                                             \
    if (rc != expect) {                                                          \
      auto msg = fmt::format(#exp " fail. error({}): {}", rc, fi_strerror(-rc)); \
      SPDLOG_ERROR(msg);                                                         \
      throw std::runtime_error(msg);                                             \
    }                                                                            \
  } while (0)

/**
 * @brief Assert condition and throw on failure
 * @param exp Boolean expression to verify
 * @throws std::runtime_error on assertion failure
 */
#define ASSERT(exp)                                    \
  do {                                                 \
    if (!(exp)) {                                      \
      auto msg = fmt::format(#exp " assertion fail."); \
      SPDLOG_ERROR(msg);                               \
      throw std::runtime_error(msg);                   \
    }                                                  \
  } while (0)

/** @brief Calculate endpoint index from rank */
#define ENDPOINT_IDX(rank) (rank * kMaxAddrSize)

/** @brief Maximum address buffer size */
constexpr size_t kMaxAddrSize = 64;
/** @brief Standard address size */
constexpr size_t kAddrSize = 32;
/** @brief Memory alignment boundary */
constexpr size_t kAlign = 128;
/** @brief Default buffer size */
constexpr size_t kBufferSize = 8129;
/** @brief Maximum completion queue entries */
constexpr size_t kMaxCQEntries = 16;

/**
 * @brief Base class preventing copy operations
 */
struct NoCopy {
 protected:
  NoCopy() = default;
  ~NoCopy() = default;
  NoCopy(NoCopy&&) = default;
  NoCopy& operator=(NoCopy&&) = default;
  NoCopy(const NoCopy&) = delete;
  NoCopy& operator=(const NoCopy&) = delete;
};
