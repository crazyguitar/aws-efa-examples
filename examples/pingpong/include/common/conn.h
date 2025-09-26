#pragma once
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>
#include <utility>

#include "common/buffer.h"
#include "common/coro.h"
#include "common/event.h"
#include "common/utils.h"

/**
 * @brief RDMA connection with coroutine-based async I/O
 */
class Conn : private NoCopy {
 public:
  /**
   * @brief Create connection with endpoint and buffers
   * @param ep Fabric endpoint handle
   * @param domain RDMA domain for buffer registration
   * @param remote Remote endpoint address
   */
  Conn(struct fid_ep *ep, struct fid_domain *domain, fi_addr_t remote)
      : ep_{ep}, remote_{remote}, recv_buffer_{Buffer(domain, kBufferSize)}, send_buffer_{Buffer(domain, kBufferSize)} {}

  struct recv_awaiter {
    Conn *conn{0};
    Context context{0};
    size_t size{0};
    recv_awaiter(Conn *c, size_t sz) : conn{c}, size{sz} {}
    constexpr bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> coroutine) {
      coroutine.promise().SetState(Handle::kSuspend);
      context.handle = &coroutine.promise();
      struct iovec iov{0};
      struct fi_msg msg{0};
      auto &buffer = conn->recv_buffer_;
      iov.iov_base = buffer.GetData();
      iov.iov_len = size;
      msg.msg_iov = &iov;
      msg.desc = &buffer.GetMR()->mem_desc;
      msg.iov_count = 1;
      msg.addr = FI_ADDR_UNSPEC;
      msg.context = &context;
      CHECK(fi_recvmsg(conn->ep_, &msg, 0));
      return true;
    }

    std::pair<char *, size_t> await_resume() {
      auto &entry = context.entry;
      auto flags = entry.flags;
      bool is_recv = (flags & FI_RECV);
      if (!is_recv) throw std::runtime_error(fmt::format("Invalid cq recv flags."));
      char *buf = (char *)conn->recv_buffer_.GetData();
      auto len = entry.len;
      return {buf, len};
    }
  };

  struct send_awaiter {
    Conn *conn{0};
    Context context{0};
    size_t size{0};
    send_awaiter(Conn *c, size_t sz) : conn{c}, size{sz} {}
    constexpr bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> coroutine) {
      coroutine.promise().SetState(Handle::kSuspend);
      context.handle = &coroutine.promise();
      auto &buffer = conn->send_buffer_;
      struct iovec iov{0};
      struct fi_msg msg{0};
      iov.iov_base = buffer.GetData();
      iov.iov_len = size;
      msg.msg_iov = &iov;
      msg.desc = &buffer.GetMR()->mem_desc;
      msg.iov_count = 1;
      msg.addr = conn->remote_;
      msg.context = &context;
      CHECK(fi_sendmsg(conn->ep_, &msg, 0));
      return true;
    }

    size_t await_resume() {
      auto &entry = context.entry;
      auto flags = entry.flags;
      bool is_send = (flags & FI_SEND);
      if (!is_send) throw std::runtime_error(fmt::format("Invalid cq send flags."));
      return entry.len;
    }
  };

  /**
   * @brief Asynchronously receive data
   * @param sz Maximum bytes to receive (default: kBufferSize)
   * @return Coroutine yielding {buffer_ptr, actual_size}
   * @throws std::invalid_argument if sz <= 0
   */
  Coro<std::pair<char *, size_t>> Recv(size_t sz = kBufferSize) {
    if (sz <= 0) throw std::invalid_argument("Recv buffer size should be greater than 0");
    co_return co_await recv_awaiter(this, sz);
  }

  /**
   * @brief Asynchronously send data
   * @param data Data buffer to send
   * @param sz Number of bytes to send
   * @return Coroutine yielding bytes sent
   * @throws std::invalid_argument if data is NULL or sz <= 0
   */
  Coro<size_t> Send(const char *data, size_t sz) {
    if (!data) throw std::invalid_argument("Send data is NULL");
    if (sz <= 0) throw std::invalid_argument("Send buffer size should be greater than 0");
    auto buffer = send_buffer_.GetData();
    std::memcpy(buffer, data, sz);
    co_return co_await send_awaiter(this, sz);
  }

  /** @brief Get send buffer reference */
  inline Buffer &GetSendBuffer() noexcept { return send_buffer_; }
  /** @brief Get receive buffer reference */
  inline Buffer &GetRecvBuffer() noexcept { return recv_buffer_; }

 private:
  struct fid_ep *ep_ = nullptr;
  fi_addr_t remote_;
  Buffer recv_buffer_;
  Buffer send_buffer_;
};
