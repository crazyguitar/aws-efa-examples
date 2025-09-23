#include <mpi.h>
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <iostream>
#include <string>

#include "utils.h"

#define CHECK(exp)                                                               \
  do {                                                                           \
    auto rc = exp;                                                               \
    if (rc) {                                                                    \
      auto msg = fmt::format(#exp " fail. error({}): {}", rc, fi_strerror(-rc)); \
      SPDLOG_ERROR(msg);                                                         \
      throw std::runtime_error(msg);                                             \
    }                                                                            \
  } while (0)

#define ASSERT(exp)                                    \
  do {                                                 \
    if (!(exp)) {                                      \
      auto msg = fmt::format(#exp " assertion fail."); \
      SPDLOG_ERROR(msg);                               \
      throw std::runtime_error(msg);                   \
    }                                                  \
  } while (0)

#define ENDPOINT_IDX(rank) (rank * kMaxAddrSize)

constexpr size_t kMaxAddrSize = 64;

class MPI {
 public:
  inline static MPI &Get() {
    static MPI mpi;
    return mpi;
  }

  MPI(const MPI &) = delete;
  MPI(MPI &&) = delete;
  MPI &operator=(const MPI &) = delete;
  MPI &operator=(MPI &&) = delete;

  int GetWorldSize() const noexcept { return world_size_; }
  int GetWorldRank() const noexcept { return world_rank_; }
  int GetLocalSize() const noexcept { return local_size_; }
  int GetLocalRank() const noexcept { return local_rank_; }
  int GetNumNodes() const noexcept { return num_nodes_; }
  int GetNodeIndex() const noexcept { return node_; };
  const char *GetProcessName() const noexcept { return processor_name_; }

 private:
  MPI() {
    MPI_Init(nullptr, nullptr);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank_);
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &local_comm_);
    MPI_Comm_rank(local_comm_, &local_rank_);
    MPI_Comm_size(local_comm_, &local_size_);

    int len;
    MPI_Get_processor_name(processor_name_, &len);
    num_nodes_ = world_size_ / local_size_;
    node_ = world_rank_ / local_size_;
  }

  ~MPI() { MPI_Finalize(); }

 private:
  friend std::ostream &operator<<(std::ostream &os, MPI &mpi) {
    os << "world_size: " << mpi.GetWorldSize();
    os << " world_rank: " << mpi.GetWorldRank();
    os << " local_size: " << mpi.GetLocalSize();
    os << " local_rank: " << mpi.GetLocalRank();
    os << " num_nodes: " << mpi.GetNumNodes();
    os << " node_index: " << mpi.GetNodeIndex();
    os << " process_name: " << mpi.GetProcessName();
    return os;
  }

 private:
  int world_size_;
  int world_rank_;
  int local_size_;
  int local_rank_;
  int num_nodes_;
  int node_;
  char processor_name_[MPI_MAX_PROCESSOR_NAME] = {0};
  MPI_Comm local_comm_;
};

class EFA {
 public:
  inline static EFA &Get() {
    static EFA efa;
    return efa;
  }

  EFA(const EFA &) = delete;
  EFA(EFA &&) = delete;
  EFA &operator=(const EFA &) = delete;
  EFA &operator=(EFA &&) = delete;
  struct fi_info *GetEFAInfo() { return info_; }

 private:
  EFA() : info_{GetInfo()} { ASSERT(info_); }
  ~EFA() {
    if (info_) {
      fi_freeinfo(info_);
      info_ = nullptr;
    }
  }

 private:
  friend std::ostream &operator<<(std::ostream &os, const EFA &efa) {
    for (auto cur = efa.info_; !!cur; cur = cur->next) {
      os << fmt::format("provider: {}\n", cur->fabric_attr->prov_name);
      os << fmt::format("    fabric: {}\n", cur->fabric_attr->name);
      os << fmt::format("    domain: {}\n", cur->domain_attr->name);
      os << fmt::format("    version: {}.{}\n", FI_MAJOR(cur->fabric_attr->prov_version), FI_MINOR(cur->fabric_attr->prov_version));
      os << fmt::format("    type: {}\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
      os << fmt::format("    protocol: {}\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
    }
    return os;
  }

 private:
  struct fi_info *info_ = nullptr;
};

class Net {
 public:
  Net() = default;
  ~Net() {
    if (cq_) {
      fi_close((fid_t)cq_);
      cq_ = nullptr;
    }
    if (av_) {
      fi_close((fid_t)av_);
      av_ = nullptr;
    }
    if (ep_) {
      fi_close((fid_t)ep_);
      ep_ = nullptr;
    }
    if (domain_) {
      fi_close((fid_t)domain_);
      domain_ = nullptr;
    }
    if (fabric_) {
      fi_close((fid_t)fabric_);
      fabric_ = nullptr;
    }
  }

  void Open(struct fi_info *info) {
    struct fi_av_attr av_attr{};
    struct fi_cq_attr cq_attr{};

    CHECK(fi_fabric(info->fabric_attr, &fabric_, nullptr));
    CHECK(fi_domain(fabric_, info, &domain_, nullptr));

    cq_attr.format = FI_CQ_FORMAT_DATA;
    CHECK(fi_cq_open(domain_, &cq_attr, &cq_, nullptr));
    CHECK(fi_av_open(domain_, &av_attr, &av_, nullptr));
    CHECK(fi_endpoint(domain_, info, &ep_, nullptr));
    CHECK(fi_ep_bind(ep_, &cq_->fid, FI_SEND | FI_RECV));
    CHECK(fi_ep_bind(ep_, &av_->fid, 0));
    CHECK(fi_enable(ep_));

    size_t len = sizeof(addr_);
    CHECK(fi_getname(&ep_->fid, addr_, &len));
  }

  const char *GetAddr() { return addr_; }

 private:
  struct fid_fabric *fabric_ = nullptr;
  struct fid_domain *domain_ = nullptr;
  struct fid_ep *ep_ = nullptr;
  struct fid_cq *cq_ = nullptr;
  struct fid_av *av_ = nullptr;
  char addr_[kMaxAddrSize] = {0};
};

void AllGatherAddr(const char *addr, int rank, std::string &endpoints) {
  std::memcpy(endpoints.data() + ENDPOINT_IDX(rank), addr, kMaxAddrSize);
  MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, endpoints.data(), kMaxAddrSize, MPI_BYTE, MPI_COMM_WORLD);
}

int main(int argc, char *argv[]) {
  auto &mpi = MPI::Get();
  auto &efa = EFA::Get();
  auto net = Net();
  auto rank = mpi.GetWorldRank();
  std::string endpoints(mpi.GetWorldSize() * kMaxAddrSize, 0);

  net.Open(efa.GetEFAInfo());
  AllGatherAddr(net.GetAddr(), rank, endpoints);
}
