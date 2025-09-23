#include "common/utils.h"

struct fi_info *GetInfo() {
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
