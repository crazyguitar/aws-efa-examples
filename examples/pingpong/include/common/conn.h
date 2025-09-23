#pragma once

#include "common/buffer.h"
#include "common/coro.h"

class Conn {
 public:
  Conn() = delete;
  Conn(struct fid_ep *ep, struct fid_domain *domain) : ep_{ep}, buffer_{std::make_unique<Buffer>(domain, kBufferSize)} {}

  struct recv_awaiter {
    Conn *conn;
    recv_awaiter(Conn *c) : conn{c} {}
    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> coroutine) {
      conn->Suspend(coroutine);
      struct iovec iov{0};
      struct fi_msg msg{0};
      iov.iov_base = conn->buffer_->GetData();
      iov.iov_len = conn->buffer_->GetSize();
      msg.msg_iov = &iov;
      msg.desc = &conn->buffer_->GetMR()->mem_desc;
      msg.iov_count = 1;
      msg.addr = FI_ADDR_UNSPEC;
      msg.context = conn;
      CHECK(fi_recvmsg(conn->ep_, &msg, 0));
    }

    Buffer *await_resume() { return conn->buffer_.get(); }
  };

  void Suspend(std::coroutine_handle<> coroutine) { coroutine_ = coroutine; }
  void Resume() {
    if (!coroutine_) return;
    coroutine_.resume();
  }

 private:
  struct fid_ep *ep_ = nullptr;
  std::unique_ptr<Buffer> buffer_ = nullptr;
  std::coroutine_handle<> coroutine_ = nullptr;
  struct fi_cq_data_entry *entry_ = nullptr;
};
