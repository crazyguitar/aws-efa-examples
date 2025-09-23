#pragma once

#include <rdma/fabric.h>
#include <spdlog/spdlog.h>

#include <iostream>

#include "common/utils.h"

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
