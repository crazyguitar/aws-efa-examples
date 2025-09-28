#pragma once
#include <cuda.h>
#include <cuda_runtime.h>
#include <errno.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
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
        size_{std::exchange(other.size_, 0)},
        mr_{std::exchange(other.mr_, nullptr)} {}

  Buffer &operator=(Buffer &&other) {
    raw_ = std::exchange(other.raw_, nullptr);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    mr_ = std::exchange(other.mr_, nullptr);
    return *this;
  }

  virtual ~Buffer() {
    if (mr_) {
      fi_close((fid_t)mr_);
      mr_ = nullptr;
    }
    if (raw_) {
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
  /**
   * @brief Align pointer to specified boundary
   * @param ptr Pointer to align
   * @param align Alignment boundary
   * @return Aligned pointer
   */
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

 private:
  /**
   * @brief Register host buffer with RDMA domain
   * @param domain RDMA domain for registration
   * @param data Buffer data pointer
   * @param size Buffer size in bytes
   * @return Memory region handle
   * @throws std::runtime_error on registration failure
   */
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

class CUDABuffer : public Buffer {
 public:
  CUDABuffer() = default;

  /**
   * @brief Create CUDA buffer with DMA-BUF registration
   * @param domain RDMA domain for memory registration
   * @param size Buffer size in bytes
   * @param align Memory alignment (default: kAlign)
   * @throws std::runtime_error on CUDA allocation or registration failure
   */
  CUDABuffer(struct fid_domain *domain, size_t size, size_t align = kAlign) {
    struct cudaPointerAttributes attrs = {};
    CUDA_CHECK(cudaMalloc(&raw_, size));
    CUDA_CHECK(cudaPointerGetAttributes(&attrs, raw_));
    ASSERT(attrs.type == cudaMemoryTypeDevice);
    CU_CHECK(cuMemGetHandleForAddressRange(&dmabuf_fd_, (CUdeviceptr)Align(raw_, align), size, CU_MEM_RANGE_HANDLE_TYPE_DMA_BUF_FD, 0));
    ASSERT(dmabuf_fd_ != -1);
    data_ = Align(raw_, align);
    device_ = attrs.device;
    size_ = (size_t)((uintptr_t)raw_ + size - (uintptr_t)data_);
    mr_ = Bind(domain, data_, size_, dmabuf_fd_, device_);
  }

  /**
   * @brief Move constructor for CUDABuffer
   * @param other CUDABuffer to move from
   */
  CUDABuffer(CUDABuffer &&other)
      : Buffer(std::move(other)), dmabuf_fd_{std::exchange(other.dmabuf_fd_, -1)}, device_{std::exchange(other.device_, -1)} {}

  /**
   * @brief Move assignment operator for CUDABuffer
   * @param other CUDABuffer to move from
   * @return Reference to this object
   */
  CUDABuffer &operator=(CUDABuffer &&other) {
    raw_ = std::exchange(other.raw_, nullptr);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    mr_ = std::exchange(other.mr_, nullptr);
    dmabuf_fd_ = std::exchange(other.dmabuf_fd_, -1);
    device_ = std::exchange(other.device_, -1);
    return *this;
  }

  /**
   * @brief Destructor - cleans up CUDA memory and DMA-BUF resources
   */
  ~CUDABuffer() {
    if (mr_) {
      fi_close((fid_t)mr_);
      mr_ = nullptr;
    }
    if (raw_) {
      cudaFree(raw_);
      raw_ = nullptr;
    }
    data_ = nullptr;
    size_ = 0;
    dmabuf_fd_ = -1;
    device_ = -1;
  }

 private:
  /**
   * @brief Register CUDA buffer with RDMA domain using DMA-BUF
   * @param domain RDMA domain for registration
   * @param data Buffer data pointer
   * @param size Buffer size in bytes
   * @param dmabuf_fd DMA-BUF file descriptor
   * @param device CUDA device ID
   * @return Memory region handle
   * @throws std::runtime_error on registration failure
   */
  inline static struct fid_mr *Bind(struct fid_domain *domain, void *data, size_t size, int dmabuf_fd, int device) {
    struct fid_mr *mr;
    struct fi_mr_attr mr_attr = {};
    struct fi_mr_dmabuf dmabuf = {};
    uint64_t flags = 0;

    dmabuf.fd = dmabuf_fd;
    dmabuf.offset = 0;
    dmabuf.len = size;
    dmabuf.base_addr = data;

    mr_attr.iov_count = 1;
    mr_attr.access = FI_SEND | FI_RECV | FI_REMOTE_WRITE | FI_REMOTE_READ | FI_WRITE | FI_READ;
    mr_attr.iface = FI_HMEM_CUDA;
    mr_attr.device.cuda = device;
    mr_attr.dmabuf = &dmabuf;

    flags = FI_MR_DMABUF;
    CHECK(fi_mr_regattr(domain, &mr_attr, flags, &mr));
    return mr;
  }

 private:
  int dmabuf_fd_ = -1;
  int device_ = -1;
};
