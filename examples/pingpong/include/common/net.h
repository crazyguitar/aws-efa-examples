#pragma once
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <unordered_map>

#include "common/conn.h"
#include "common/utils.h"

class Net {
 public:
  Net() = default;
  ~Net();

  void Open(struct fi_info *info);
  void Connect(const char *remote);
  const char *GetAddr() { return addr_; }
  struct fid_cq *GetCQ() { return cq_; }

  inline static std::string Addr2Str(const char *addr) {
    std::string out;
    for (size_t i = 0; i < kAddrSize; ++i) out += fmt::format("{:02x}", addr[i]);
    return out;
  }

  inline static void Str2Addr(const std::string &addr, char *bytes) {
    for (size_t i = 0; i < kAddrSize; ++i) sscanf(addr.c_str() + 2 * i, "%02hhx", &bytes[i]);
  }

 private:
  struct fid_fabric *fabric_ = nullptr;
  struct fid_domain *domain_ = nullptr;
  struct fid_ep *ep_ = nullptr;
  struct fid_cq *cq_ = nullptr;
  struct fid_av *av_ = nullptr;
  char addr_[kMaxAddrSize] = {0};
  std::unordered_map<std::string, std::unique_ptr<Conn>> conns_;
};
