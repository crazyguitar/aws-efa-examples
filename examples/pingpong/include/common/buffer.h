#pragma once
#include <errno.h>
#include <stdlib.h>

#include <utility>

#include "common/efa.h"
#include "common/utils.h"

class Buffer {
 public:
  Buffer() = default;
  Buffer(struct fid_domain *domain, size_t size, size_t align = kAlign) {
    raw_ = malloc(size);
    if (!raw_) {
      auto msg = fmt::format("malloc fail. error: {}", strerror(errno));
      throw std::runtime_error(msg);
    }
    data_ = Align(raw_, align);
    size_ = (size_t)((uintptr_t)raw_ + size - (uintptr_t)data_);
    mr_ = Bind(domain, data_, size_);
  }

  Buffer(Buffer &&other) : raw_{std::exchange(other.raw_, nullptr)}, data_{std::exchange(other.data_, nullptr)}, size_{std::exchange(other.size_, 0)}, mr_{std::exchange(other.mr_, nullptr)} {}

  Buffer &operator=(Buffer &&other) {
    raw_ = std::exchange(other.raw_, nullptr);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    mr_ = std::exchange(other.mr_, nullptr);
    return *this;
  }

  ~Buffer() {
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
    bytes_ = 0;
  }

  void *GetData() const { return data_; }
  struct fid_mr *GetMR() const { return mr_; }
  size_t GetSize() const { return size_; }
  size_t GetBytes() const { return bytes_; }

 private:
  inline static void *Align(void *ptr, size_t align) {
    uintptr_t addr = (uintptr_t)(ptr);
    return (void *)((addr + align - 1) & ~(align - 1));
  }

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

 private:
  void *raw_ = nullptr;   // raw memory
  void *data_ = nullptr;  // aligned memory
  size_t size_ = 0;       // total memory size
  size_t bytes_ = 0;      // number of receive data
  struct fid_mr *mr_ = nullptr;
};
