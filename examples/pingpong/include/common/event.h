#pragma once

#include <rdma/fi_domain.h>

#include "common/handle.h"

struct Context {
  struct fi_cq_data_entry entry;
  Handle *handle;
};

struct Event {
  uint64_t flags;
  Handle *handle;
};
