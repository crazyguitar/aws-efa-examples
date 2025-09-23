#pragma once
#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <spdlog/spdlog.h>

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
