#pragma once

#include <rdma/fabric.h>
#include <spdlog/spdlog.h>

#include <iostream>

#include "common/utils.h"

/**
 * @brief Singleton class for EFA (Elastic Fabric Adapter) initialization and management
 */
class EFA : private NoCopy {
 public:
  /**
   * @brief Get singleton EFA instance
   * @return Reference to the EFA singleton
   */
  inline static EFA &Get() {
    static EFA efa;
    return efa;
  }

  /**
   * @brief Get EFA fabric information
   * @return Pointer to fabric info structure
   */
  struct fi_info *GetEFAInfo() { return info_; }

 private:
  EFA() : info_{GetInfo()} { ASSERT(info_); }
  ~EFA() {
    if (info_) {
      fi_freeinfo(info_);
      info_ = nullptr;
    }
  }

  inline static struct fi_info *GetInfo() {
    int rc = 0;
    struct fi_info *hints = nullptr;
    struct fi_info *info = nullptr;
    hints = fi_allocinfo();
    if (!hints) {
      SPDLOG_ERROR("fi_allocinfo fail.");
      goto end;
    }

    hints->caps = FI_MSG | FI_RMA | FI_HMEM | FI_LOCAL_COMM | FI_REMOTE_COMM;
    hints->ep_attr->type = FI_EP_RDM;
    hints->fabric_attr->prov_name = strdup("efa");
    hints->domain_attr->mr_mode = FI_MR_LOCAL | FI_MR_HMEM | FI_MR_VIRT_ADDR | FI_MR_ALLOCATED | FI_MR_PROV_KEY;
    hints->domain_attr->threading = FI_THREAD_SAFE;

    rc = fi_getinfo(FI_VERSION(1, 20), NULL, NULL, 0, hints, &info);
    if (rc != 0) {
      SPDLOG_ERROR("fi_getinfo fail. error({}): {}", rc, fi_strerror(-rc));
      goto error;
    } else {
      goto end;
    }

  error:
    if (info) {
      fi_freeinfo(info);
      info = nullptr;
    }

  end:
    if (hints) {
      fi_freeinfo(hints);
      hints = nullptr;
    }
    return info;
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
