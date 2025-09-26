#pragma once

#include <hwloc.h>
#include <spdlog/spdlog.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define CHECK(exp)                                                      \
  do {                                                                  \
    if ((exp)) {                                                        \
      auto msg = fmt::format(#exp " fail. error: {}", strerror(errno)); \
      SPDLOG_ERROR(msg);                                                \
      throw std::runtime_error(msg);                                    \
    }                                                                   \
  } while (0)

#define ASSERT(exp)                                    \
  do {                                                 \
    if (!(exp)) {                                      \
      auto msg = fmt::format(#exp " assertion fail."); \
      SPDLOG_ERROR(msg);                               \
      throw std::runtime_error(msg);                   \
    }                                                  \
  } while (0)

constexpr uint16_t NVIDIA_VENDOR_ID = 0x10de;
constexpr uint16_t AMD_VENDOR_ID = 0x1002;

using pci_type = std::unordered_set<hwloc_obj_t>;

struct Numanode {
  hwloc_obj_t numanode;
  std::unordered_set<hwloc_obj_t> cores;
  std::unordered_map<hwloc_obj_t, pci_type> bridge;
};

class Hwloc {
 public:
  Hwloc() {
    CHECK(hwloc_topology_init(&topology_));
    CHECK(hwloc_topology_set_all_types_filter(topology_, HWLOC_TYPE_FILTER_KEEP_ALL));
    CHECK(hwloc_topology_set_io_types_filter(topology_, HWLOC_TYPE_FILTER_KEEP_IMPORTANT));
    CHECK(hwloc_topology_set_flags(topology_, HWLOC_TOPOLOGY_FLAG_IMPORT_SUPPORT));
    CHECK(hwloc_topology_load(topology_));
    Traverse(hwloc_get_root_obj(topology_), nullptr, numanodes_);
  }

  ~Hwloc() { hwloc_topology_destroy(topology_); }
  const std::vector<Numanode> &GetNumaNodes() const noexcept { return numanodes_; }

  inline static bool IsPackage(hwloc_obj_t l) { return l->type == HWLOC_OBJ_PACKAGE; }
  inline static bool IsNumaNode(hwloc_obj_t l) { return l->type == HWLOC_OBJ_NUMANODE; }
  inline static bool IsCore(hwloc_obj_t l) { return l->type == HWLOC_OBJ_CORE; }
  inline static bool IsPCI(hwloc_obj_t l) { return l->type == HWLOC_OBJ_PCI_DEVICE; }

  inline static bool IsHostBridge(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_BRIDGE) return false;
    return l->attr->bridge.upstream_type != HWLOC_OBJ_BRIDGE_PCI;
  }

  inline static bool IsEFA(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_PCI_DEVICE) return false;
    return IsOSDevType(HWLOC_OBJ_OSDEV_OPENFABRICS, l);
  }

  inline static bool IsGPU(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_PCI_DEVICE) return false;
    auto class_id = l->attr->pcidev.class_id >> 8;
    if (class_id != 0x03) return false;
    auto vendor_id = l->attr->pcidev.vendor_id;
    if (vendor_id != NVIDIA_VENDOR_ID) return false;
    return true;
  }

  static bool IsOSDevType(hwloc_obj_osdev_type_e type, hwloc_obj_t l) {
    if (!l) return false;
    if (l->attr->osdev.type == type) return true;
    for (hwloc_obj_t child = l->memory_first_child; !!child; child = child->next_sibling) {
      if (child->type != HWLOC_OBJ_PU and IsOSDevType(type, child)) return true;
    }
    for (hwloc_obj_t child = l->first_child; !!child; child = child->next_sibling) {
      if (child->type != HWLOC_OBJ_PU and IsOSDevType(type, child)) return true;
    }
    for (hwloc_obj_t child = l->io_first_child; !!child; child = child->next_sibling) {
      if (IsOSDevType(type, child)) return true;
    }
    for (hwloc_obj_t child = l->misc_first_child; !!child; child = child->next_sibling) {
      if (IsOSDevType(type, child)) return true;
    }
    return false;
  }

  static void Traverse(hwloc_obj_t l, hwloc_obj_t bridge, std::vector<Numanode> &numanodes) {
    if (IsPackage(l)) {
      numanodes.emplace_back(Numanode{});
    } else if (IsNumaNode(l)) {
      auto &numa = numanodes.back();
      numa.numanode = l;
    } else if (IsHostBridge(l)) {
      auto &numa = numanodes.back();
      numa.bridge.emplace(l, pci_type{});
      bridge = l;
    } else if (IsCore(l)) {
      auto &numa = numanodes.back();
      numa.cores.emplace(l);
    } else if (IsPCI(l)) {
      assert(!!bridge);
      auto &numa = numanodes.back();
      numa.bridge[bridge].emplace(l);
    }

    for (hwloc_obj_t child = l->memory_first_child; !!child; child = child->next_sibling) {
      if (child->type != HWLOC_OBJ_PU) Traverse(child, bridge, numanodes);
    }
    for (hwloc_obj_t child = l->first_child; !!child; child = child->next_sibling) {
      if (child->type != HWLOC_OBJ_PU) Traverse(child, bridge, numanodes);
    }
    for (hwloc_obj_t child = l->io_first_child; !!child; child = child->next_sibling) {
      Traverse(child, bridge, numanodes);
    }
    for (hwloc_obj_t child = l->misc_first_child; !!child; child = child->next_sibling) {
      Traverse(child, bridge, numanodes);
    }
  }

 private:
  hwloc_topology_t topology_;
  std::vector<Numanode> numanodes_;
};

struct GPUAffinity {
  hwloc_obj_t gpu;
  hwloc_obj_t numanode;
  std::vector<hwloc_obj_t> cores;
  std::vector<hwloc_obj_t> efas;
};

class GPUloc {
 public:
  using affinity_type = std::unordered_map<hwloc_obj_t, GPUAffinity>;

  GPUloc() : hwloc_{Hwloc()}, affinity_{GetAffinity(hwloc_)} {}
  const affinity_type &GetGPUAffinity() const noexcept { return affinity_; }

 private:
  static affinity_type GetAffinity(Hwloc &hwloc) {
    std::unordered_map<hwloc_obj_t, GPUAffinity> affinity;
    for (auto &numa : hwloc.GetNumaNodes()) {
      for (auto &bridge : numa.bridge) {
        std::vector<hwloc_obj_t> gpus;
        std::vector<hwloc_obj_t> efas;
        for (auto pci : bridge.second) {
          if (Hwloc::IsGPU(pci)) {
            gpus.emplace_back(pci);
          } else if (Hwloc::IsEFA(pci)) {
            efas.emplace_back(pci);
          }
        }
        std::vector<hwloc_obj_t> cores(numa.cores.begin(), numa.cores.end());
        std::sort(cores.begin(), cores.end(), [](auto &&x, auto &&y) { return x->logical_index < y->logical_index; });
        for (auto &gpu : gpus) affinity[gpu] = GPUAffinity{gpu, numa.numanode, cores, efas};
      }
    }
    return affinity;
  }

  friend std::ostream &operator<<(std::ostream &os, const GPUloc &loc) {
    for (auto &x : loc.affinity_) {
      auto &affinity = x.second;
      auto numanode = affinity.numanode;
      auto gpu = affinity.gpu;
      auto &cores = affinity.cores;
      auto &efas = affinity.efas;
      os << fmt::format("GPU ({:02x}:{:02x}.{:01x})", gpu->attr->pcidev.bus, gpu->attr->pcidev.dev, gpu->attr->pcidev.func);
      os << fmt::format(" NUMA{}", numanode->logical_index);
      os << fmt::format(" Core{:>2}-Core{:>2}", cores.front()->logical_index, cores.back()->logical_index);
      os << "\n";
      for (auto efa : efas) {
        os << fmt::format("  EFA ({:02x}:{:02x}.{:01x})\n", efa->attr->pcidev.bus, efa->attr->pcidev.dev, efa->attr->pcidev.func);
      }
    }
    return os;
  }

 private:
  Hwloc hwloc_;
  affinity_type affinity_;
};
