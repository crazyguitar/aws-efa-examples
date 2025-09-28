#pragma once

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/event.h"
#include "common/utils.h"

/**
 * @brief Event selector for polling completion queues
 */
class Selector {
 public:
  /**
   * @brief Poll completion queues for events
   * @return Vector of ready events
   * @throws std::runtime_error on fatal CQ errors
   */
  inline std::vector<Event> Select() {
    std::vector<Event> ret;
    struct fi_cq_data_entry cq_entries[kMaxCQEntries];
    for (auto cq : cqs_) {
      auto rc = fi_cq_read(cq, cq_entries, kMaxCQEntries);
      if (rc > 0) {
        HandleCompletion(cq_entries, rc, ret);
      } else if (rc == -FI_EAVAIL) {
        HandleError(cq);
      } else if (rc == -FI_EAGAIN) {
        continue;
      } else {
        auto msg = fmt::format("fatal error. error({}): {}", rc, fi_strerror(-rc));
        throw std::runtime_error(msg);
      }
    }
    return ret;
  }

  /**
   * @brief Register completion queue for polling
   * @param cq Completion queue to register
   */
  inline void Register(struct fid_cq *cq) { cqs_.emplace(cq); }

  /**
   * @brief Unregister completion queue from polling
   * @param cq Completion queue to unregister
   */
  inline void UnRegister(struct fid_cq *cq) { cqs_.erase(cq); }

  inline void Register(uint64_t id, Context *context) { imm_data_contexts_.emplace(id, context); }

  inline void UnRegister(uint64_t id) { imm_data_contexts_.erase(id); }

  /**
   * @brief Check if selector has no registered queues
   * @return true if no completion queues registered
   */
  inline bool Stopped() const noexcept { return cqs_.empty(); }

 private:
  inline void HandleCompletion(struct fi_cq_data_entry *cq_entries, size_t n, std::vector<Event> &ret) {
    for (size_t i = 0; i < n; ++i) {
      auto &entry = cq_entries[i];
      auto flags = entry.flags;
      if (flags & FI_REMOTE_WRITE) {
        uint32_t imm_data = entry.data;
        if (!imm_data) continue;
        if (!imm_data_contexts_.contains(imm_data)) continue;
        auto context = imm_data_contexts_[imm_data];
        context->entry = entry;
        Handle *handle = context->handle;
        ret.emplace_back(Event{flags, handle});
      } else {
        Context *context = reinterpret_cast<Context *>(entry.op_context);
        if (!context) continue;
        context->entry = entry;
        Handle *handle = context->handle;
        ret.emplace_back(Event{flags, handle});
      }
    }
  }

  inline static void HandleError(struct fid_cq *cq) {
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
  std::unordered_map<uint64_t, Context *> imm_data_contexts_;
};
