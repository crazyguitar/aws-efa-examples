#include "common/net.h"

Conn *Net::Connect(const char *remote) {
  fi_addr_t addr = FI_ADDR_UNSPEC;
  EXPECT(fi_av_insert(av_, remote, 1, &addr, 0, nullptr), 1);
  auto key = Addr2Str(remote);
  auto conn = std::make_unique<Conn>(ep_, domain_, addr);
  auto raw_conn = conn.get();
  conns_.emplace(key, std::move(conn));
  return raw_conn;
}

void Net::Open(struct fi_info *info) {
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
  Register();
}

Net::~Net() {
  if (cq_) {
    // unregister
    IO::Get().UnRegister(cq_);
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
