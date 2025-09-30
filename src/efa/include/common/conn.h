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
  Conn(struct fid_ep *ep, struct fid_domain *domain, fi_addr_t remote) : ep_{ep}, domain_{domain}, remote_{remote} {}

  /**
   * @brief Awaiter for asynchronous receive operations
   * Suspends coroutine until RDMA receive completes
   */
  struct recv_awaiter {
    Buffer &buffer;
    Context context{0};
    size_t size{0};
    struct fid_ep *ep{0};
    struct fid_domain *domain{0};
    recv_awaiter(Buffer &b, size_t sz, struct fid_ep *e, struct fid_domain *d) : buffer{b}, size{sz}, ep{e}, domain{d} {}
    constexpr bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> coroutine) {
      coroutine.promise().SetState(Handle::kSuspend);
      context.handle = &coroutine.promise();
      struct iovec iov{0};
      struct fi_msg msg{0};
      iov.iov_base = buffer.GetData();
      iov.iov_len = size;
      msg.msg_iov = &iov;
      msg.desc = &buffer.GetMR(domain)->mem_desc;
      msg.iov_count = 1;
      msg.addr = FI_ADDR_UNSPEC;
      msg.context = &context;
      CHECK(fi_recvmsg(ep, &msg, 0));
      return true;
    }

    size_t await_resume() {
      auto &entry = context.entry;
      auto flags = entry.flags;
      bool is_recv = (flags & FI_RECV);
      if (!is_recv) throw std::runtime_error(fmt::format("Invalid cq recv flags."));
      auto len = entry.len;
      return len;
    }
  };

  /**
   * @brief Awaiter for asynchronous send operations
   * Suspends coroutine until RDMA send completes
   */
  struct send_awaiter {
    Buffer &buffer;
    Context context{0};
    size_t size{0};
    struct fid_ep *ep{0};
    struct fid_domain *domain{0};
    fi_addr_t remote;
    send_awaiter(Buffer &b, size_t sz, struct fid_ep *e, struct fid_domain *d, fi_addr_t r) : buffer{b}, size{sz}, ep{e}, domain{d}, remote{r} {}
    constexpr bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> coroutine) {
      coroutine.promise().SetState(Handle::kSuspend);
      context.handle = &coroutine.promise();
      struct iovec iov{0};
      struct fi_msg msg{0};
      iov.iov_base = buffer.GetData();
      iov.iov_len = size;
      msg.msg_iov = &iov;
      msg.desc = &buffer.GetMR(domain)->mem_desc;
      msg.iov_count = 1;
      msg.addr = remote;
      msg.context = &context;
      CHECK(fi_sendmsg(ep, &msg, 0));
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

  Coro<size_t> Recv(Buffer &buffer, size_t sz = kBufferSize) { return Recv(oneway, buffer, sz); }
  Coro<size_t> Send(Buffer &buffer, size_t sz) { return Send(oneway, buffer, sz); }
  struct fid_domain *GetDomain() { return domain_; }

 private:
  Coro<size_t> Recv(Oneway, Buffer &buffer, size_t sz = kBufferSize) {
    if (sz <= 0) throw std::invalid_argument("Recv buffer size should be greater than 0");
    co_return co_await recv_awaiter(buffer, sz, ep_, domain_);
  }

  Coro<size_t> Send(Oneway, Buffer &buffer, size_t sz) {
    if (sz <= 0) throw std::invalid_argument("Send buffer size should be greater than 0");
    co_return co_await send_awaiter(buffer, sz, ep_, domain_, remote_);
  }

 private:
  struct fid_ep *ep_ = nullptr;
  struct fid_domain *domain_ = nullptr;
  fi_addr_t remote_;
};
