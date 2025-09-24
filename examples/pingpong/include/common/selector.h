#pragma once
#include <spdlog/spdlog.h>

#include "common/conn.h"
#include "common/efa.h"
#include "common/utils.h"

class Selector {
 public:
  void Select() {
    struct fi_cq_data_entry cq_entries[kMaxCQEntries];
    for (auto cq : cqs_) {
      auto rc = fi_cq_read(cq, cq_entries, kMaxCQEntries);
      if (rc > 0) {
        HandleCompletion(cq_entries, rc);
      } else if (rc == -FI_EAVAIL) {
        HandleError(cq);
      } else if (rc == -FI_EAGAIN) {
        continue;
      } else {
        auto msg = fmt::format("fatal error. error({}): {}", rc, fi_strerror(-rc));
        throw std::runtime_error(msg);
      }
    }
  }

  void Register(struct fid_cq *cq) { cqs_.emplace(cq); }

 private:
  void HandleCompletion(struct fi_cq_data_entry *cq_entries, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      auto &entry = cq_entries[i];
      auto flags = entry.flags;
      auto conn = reinterpret_cast<Conn *>(entry.op_context);
      if (flags & FI_RECV) conn->HandleRecv(entry);
      if (flags & FI_SEND) conn->HandleSend(entry);
    }
  }

  void HandleError(struct fid_cq *cq) {
    struct fi_cq_err_entry err_entry;
    auto rc = fi_cq_readerr(cq, &err_entry, 0);
    if (rc < 0) {
      auto msg = fmt::format("fatal error. error({}): {}", rc, fi_strerror(-rc));
      throw std::runtime_error(msg);
    }
    if (rc > 0) {
      auto err = fi_cq_strerror(cq, err_entry.prov_errno, err_entry.err_data, nullptr, 0);
      auto msg = fmt::format("libfabric operation fail. error: {}", err);
      throw std::runtime_error(msg);
    } else {
      auto msg = fmt::format("unknown error");
      throw std::runtime_error(msg);
    }
  }

 private:
  std::unordered_set<struct fid_cq *> cqs_;
};
