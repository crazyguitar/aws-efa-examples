#pragma once
#include <errno.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <cuda.h>
#include <cuda_runtime.h>

#include <stdlib.h>
#include <utility>

#include "common/utils.h"

#define BUFFER_ASSERT(exp)                                              \
  do {                                                                  \
    if (!(exp)) {                                                       \
      auto msg = fmt::format(#exp " fail. error: {}", strerror(errno)); \
      SPDLOG_ERROR(msg);                                                \
      throw std::runtime_error(msg);                                    \
    }                                                                   \
  } while (0)

/**
 * @brief RDMA memory buffer with automatic registration
 */
class Buffer {
 public:
  Buffer() = default;

  Buffer(Buffer &&other)
      : raw_{std::exchange(other.raw_, nullptr)},
        data_{std::exchange(other.data_, nullptr)},
        size_{std::exchange(other.size_, 0)} {}

  Buffer &operator=(Buffer &&other) {
    raw_ = std::exchange(other.raw_, nullptr);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    return *this;
  }

  virtual ~Buffer() {
    if (mr_) {
      fi_close((fid_t)mr_);
      mr_ = nullptr;
    }
    if (!raw_) {
      free(raw_);
      raw_ = nullptr;
    }
    raw_ = nullptr;
    data_ = nullptr;
    size_ = 0;
  }

  /**
   * @brief Get buffer data pointer
   * @return Aligned data pointer
   */
  void *GetData() const { return data_; }

  /**
   * @brief Get usable buffer size
   * @return Size in bytes
   */
  size_t GetSize() const { return size_; }

  /**
   * @brief Get memory region handle
   * @return RDMA memory region descriptor
   */
  struct fid_mr *GetMR() const { return mr_; }

 protected:
  inline static void *Align(void *ptr, size_t align) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void *)((addr + align - 1) & ~(align - 1));
  }

 protected:
  void *raw_ = nullptr;   // raw memory
  void *data_ = nullptr;  // aligned memory
  size_t size_ = 0;       // total memory size
  struct fid_mr *mr_ = nullptr;
};

class HostBuffer : public Buffer {
 public:
  HostBuffer() = default;

  /**
   * @brief Create aligned buffer and register with domain
   * @param domain RDMA domain for memory registration
   * @param size Buffer size in bytes
   * @param align Memory alignment (default: kAlign)
   * @throws std::runtime_error on allocation or registration failure
   */
  HostBuffer(struct fid_domain *domain, size_t size, size_t align = kAlign) {
    ASSERT(!!domain);
    raw_ = malloc(size);
    BUFFER_ASSERT(raw_);
    data_ = Align(raw_, align);
    size_ = (size_t)((uintptr_t)raw_ + size - (uintptr_t)data_);
    mr_ = Bind(domain, data_, size_);
  }

 protected:
  inline static struct fid_mr *Bind(struct fid_domain *domain, void *data, size_t size) {
    struct fid_mr *mr;
    struct fi_mr_attr mr_attr = {};
    struct iovec iov = {.iov_base = data, .iov_len = size};
    mr_attr.mr_iov = &iov;
    mr_attr.iov_count = 1;
    mr_attr.access = FI_SEND | FI_RECV;
    uint64_t flags = 0;
    CHECK(fi_mr_regattr(domain, &mr_attr, flags, &mr));
    return mr;
  }
};
