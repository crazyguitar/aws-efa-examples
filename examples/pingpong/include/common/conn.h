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
      return true;
    }

    Buffer *await_resume() { return conn->buffer_.get(); }
  };

  struct send_awaiter {
    Conn *conn;
    send_awaiter(Conn *c) : conn{c} {}
    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> coroutine) {
      conn->Suspend(coroutine);
      auto &buffer = conn->buffer_;
      struct iovec iov{0};
      struct fi_msg msg{0};
      iov.iov_base = buffer->GetData();
      iov.iov_len = buffer->GetSize();
      msg.msg_iov = &iov;
      msg.desc = &buffer->GetMR()->mem_desc;
      msg.iov_count = 1;
      msg.addr = FI_ADDR_UNSPEC;
      msg.context = conn;
      CHECK(fi_sendmsg(conn->ep_, &msg, 0));
      return true;
    }

    size_t await_resume() { return conn->buffer_->GetBytes(); }
  };

  void Suspend(std::coroutine_handle<> coroutine) { coroutine_ = coroutine; }
  void Resume() {
    if (!coroutine_) return;
    coroutine_.resume();
  }

  send_awaiter Send() { return send_awaiter(this); }
  recv_awaiter Recv() { return recv_awaiter(this); }

  void HandleRecv(const struct fi_cq_data_entry &entry) {
    buffer_->SetBytes(entry.len);
    Resume();
  }

  void HandleSend(const struct fi_cq_data_entry &entry) {
    buffer_->SetBytes(entry.len);
    Resume();
  }

 private:
  struct fid_ep *ep_ = nullptr;
  std::unique_ptr<Buffer> buffer_ = nullptr;
  std::coroutine_handle<> coroutine_ = nullptr;
};
