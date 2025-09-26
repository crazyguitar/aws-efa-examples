#pragma once

#include <rdma/fi_domain.h>

#include "common/handle.h"

/**
 * @brief Context structure for completion queue operations
 */
struct Context {
  struct fi_cq_data_entry entry; /**< Completion queue entry data */
  Handle *handle;                /**< Associated handle for the operation */
};

/**
 * @brief Event structure for I/O notifications
 */
struct Event {
  uint64_t flags; /**< Event flags indicating operation type */
  Handle *handle; /**< Handle to be notified of the event */
};
