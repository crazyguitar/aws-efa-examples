#pragma once
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <spdlog/spdlog.h>

#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>

#include "common/conn.h"
#include "common/io.h"
#include "common/utils.h"

class Net {
 public:
  Net() = default;
  ~Net();

  void Open(struct fi_info *info);
  Conn *Connect(const char *remote);
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
  inline void Register() {
    if (!cq_) return;
    auto &io = IO::Get();
    io.Register(cq_);
  }

  friend std::ostream &operator<<(std::ostream &os, const Net &net) {
    os << "device addr:\n" << "  " << Addr2Str(net.addr_) << "\n";
    os << "remote addr:\n";
    for (auto &c : net.conns_) os << "  " << c.first << "\n";
    return os;
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
