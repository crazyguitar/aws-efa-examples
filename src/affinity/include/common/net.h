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

/**
 * @brief Network abstraction for EFA fabric operations
 */
class Net {
 public:
  Net() = default;
  ~Net();

  /**
   * @brief Initialize network with fabric info
   * @param info Fabric information structure
   * @throws std::runtime_error on fabric initialization failure
   */
  void Open(struct fi_info *info);

  /**
   * @brief Establish connection to remote endpoint
   * @param remote Remote endpoint address string
   * @return Pointer to connection object
   * @throws std::runtime_error on connection failure
   */
  Conn *Connect(const char *remote);

  /**
   * @brief Get local endpoint address
   * @return Local address buffer
   */
  const char *GetAddr() { return addr_; }

  /**
   * @brief Get completion queue handle
   * @return Completion queue file descriptor
   */
  struct fid_cq *GetCQ() { return cq_; }

  /**
   * @brief Convert binary address to hex string
   * @param addr Binary address buffer
   * @return Hex string representation
   */
  inline static std::string Addr2Str(const char *addr) {
    std::string out;
    for (size_t i = 0; i < kAddrSize; ++i) out += fmt::format("{:02x}", addr[i]);
    return out;
  }

  /**
   * @brief Convert hex string to binary address
   * @param addr Hex string address
   * @param bytes Output binary buffer
   */
  inline static void Str2Addr(const std::string &addr, char *bytes) {
    for (size_t i = 0; i < kAddrSize; ++i) sscanf(addr.c_str() + 2 * i, "%02hhx", &bytes[i]);
  }

 private:
  inline void Register() {
    if (!cq_) return;
    auto &io = IO::Get();
    io.Register(cq_);
  }

  inline void UnRegister() {
    if (!cq_) return;
    auto &io = IO::Get();
    io.UnRegister(cq_);
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
