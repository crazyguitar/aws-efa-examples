#pragma once

#include <spdlog/spdlog.h>

struct NoCopy {
 protected:
  NoCopy() = default;
  ~NoCopy() = default;
  NoCopy(NoCopy&&) = default;
  NoCopy& operator=(NoCopy&&) = default;
  NoCopy(const NoCopy&) = delete;
  NoCopy& operator=(const NoCopy&) = delete;
};
