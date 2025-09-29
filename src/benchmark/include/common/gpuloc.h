#pragma once

#include <hwloc.h>
#include <nvml.h>
#include <spdlog/spdlog.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/efa.h"
#include "common/utils.h"

/**
 * @brief Error checking macro that throws runtime_error on failure
 * @param exp Expression to evaluate
 */
#define GPULOC_CHECK(exp)                                               \
  do {                                                                  \
    if ((exp)) {                                                        \
      auto msg = fmt::format(#exp " fail. error: {}", strerror(errno)); \
      SPDLOG_ERROR(msg);                                                \
      throw std::runtime_error(msg);                                    \
    }                                                                   \
  } while (0)

/**
 * @brief Assertion macro that throws runtime_error on failure
 * @param exp Boolean expression to verify
 */
#define GPULOC_ASSERT(exp)                             \
  do {                                                 \
    if (!(exp)) {                                      \
      auto msg = fmt::format(#exp " assertion fail."); \
      SPDLOG_ERROR(msg);                               \
      throw std::runtime_error(msg);                   \
    }                                                  \
  } while (0)

/**
 * @brief Check NVML operation return code and throw on error
 * @param exp Expression that returns NVML result code
 * @throws std::runtime_error with error message on failure
 */
#define NVML_CHECK(exp)                                     \
  do {                                                      \
    nvmlReturn_t res = exp;                                 \
    if (res != NVML_SUCCESS) {                              \
      const char *err = nvmlErrorString(res);               \
      auto msg = fmt::format(#exp " fail. error: {}", err); \
      SPDLOG_ERROR(msg);                                    \
      throw std::runtime_error(msg);                        \
    }                                                       \
  } while (0)

/** @brief NVIDIA PCI vendor ID */
constexpr uint16_t NVIDIA_VENDOR_ID = 0x10de;
/** @brief AMD PCI vendor ID */
constexpr uint16_t AMD_VENDOR_ID = 0x1002;

using pci_type = std::unordered_set<hwloc_obj_t>;

/**
 * @brief Represents a NUMA node with its associated cores and PCI bridges
 */
struct Numanode {
  hwloc_obj_t numanode;                              ///< NUMA node object
  std::unordered_set<hwloc_obj_t> cores;             ///< CPU cores in this NUMA node
  std::unordered_map<hwloc_obj_t, pci_type> bridge;  ///< PCI bridges and their devices
};

/**
 * @brief Hardware locality wrapper class for topology discovery
 */
class Hwloc : private NoCopy {
 public:
  /**
   * @brief Constructor - initializes hwloc topology and discovers hardware
   */
  Hwloc() {
    GPULOC_CHECK(hwloc_topology_init(&topology_));
    GPULOC_CHECK(hwloc_topology_set_all_types_filter(topology_, HWLOC_TYPE_FILTER_KEEP_ALL));
    GPULOC_CHECK(hwloc_topology_set_io_types_filter(topology_, HWLOC_TYPE_FILTER_KEEP_IMPORTANT));
    GPULOC_CHECK(hwloc_topology_set_flags(topology_, HWLOC_TOPOLOGY_FLAG_IMPORT_SUPPORT));
    GPULOC_CHECK(hwloc_topology_load(topology_));
    Traverse(hwloc_get_root_obj(topology_), nullptr, numanodes_);
  }

  /**
   * @brief Destructor - cleans up hwloc topology
   */
  ~Hwloc() { hwloc_topology_destroy(topology_); }

  /**
   * @brief Get discovered NUMA nodes
   * @return Reference to vector of NUMA nodes
   */
  const std::vector<Numanode> &GetNumaNodes() const noexcept { return numanodes_; }

  /**
   * @brief Check if object is a CPU package
   * @param l hwloc object to check
   * @return true if object is a package
   */
  inline static bool IsPackage(hwloc_obj_t l) { return l->type == HWLOC_OBJ_PACKAGE; }

  /**
   * @brief Check if object is a NUMA node
   * @param l hwloc object to check
   * @return true if object is a NUMA node
   */
  inline static bool IsNumaNode(hwloc_obj_t l) { return l->type == HWLOC_OBJ_NUMANODE; }

  /**
   * @brief Check if object is a CPU core
   * @param l hwloc object to check
   * @return true if object is a core
   */
  inline static bool IsCore(hwloc_obj_t l) { return l->type == HWLOC_OBJ_CORE; }

  /**
   * @brief Check if object is a PCI device
   * @param l hwloc object to check
   * @return true if object is a PCI device
   */
  inline static bool IsPCI(hwloc_obj_t l) { return l->type == HWLOC_OBJ_PCI_DEVICE; }

  /**
   * @brief Check if object is a host bridge
   * @param l hwloc object to check
   * @return true if object is a host bridge
   */
  inline static bool IsHostBridge(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_BRIDGE) return false;
    return l->attr->bridge.upstream_type != HWLOC_OBJ_BRIDGE_PCI;
  }

  /**
   * @brief Check if PCI device is an EFA adapter
   * @param l hwloc object to check
   * @return true if object is an EFA device
   */
  inline static bool IsEFA(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_PCI_DEVICE) return false;
    return IsOSDevType(HWLOC_OBJ_OSDEV_OPENFABRICS, l);
  }

  /**
   * @brief Check if PCI device is an NVIDIA GPU
   * @param l hwloc object to check
   * @return true if object is an NVIDIA GPU
   */
  inline static bool IsGPU(hwloc_obj_t l) {
    if (l->type != HWLOC_OBJ_PCI_DEVICE) return false;
    auto class_id = l->attr->pcidev.class_id >> 8;
    if (class_id != 0x03) return false;
    auto vendor_id = l->attr->pcidev.vendor_id;
    if (vendor_id != NVIDIA_VENDOR_ID) return false;
    return true;
  }

  /**
   * @brief Check if object has OS device of specified type
   * @param type OS device type to check for
   * @param l hwloc object to check
   * @return true if object has the specified OS device type
   */
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

  /**
   * @brief Recursively traverse hwloc topology to build NUMA node structure
   * @param l Current hwloc object
   * @param bridge Current PCI bridge
   * @param numanodes Vector to populate with discovered NUMA nodes
   */
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

/**
 * @brief GPU affinity information including associated NUMA node, cores, and EFA devices
 */
struct GPUAffinity {
  hwloc_obj_t gpu;                                             ///< GPU device object
  hwloc_obj_t numanode;                                        ///< Associated NUMA node
  std::vector<hwloc_obj_t> cores;                              ///< CPU cores in the same NUMA node
  std::vector<std::pair<hwloc_obj_t, struct fi_info *>> efas;  ///< EFA devices on the same PCI bridge
};

/**
 * @brief GPU locality analyzer that maps GPUs to their optimal CPU and network resources
 */
class GPUloc : private NoCopy {
 public:
  using affinity_type = std::vector<GPUAffinity>;
  using pci_type = std::tuple<unsigned, unsigned, unsigned, unsigned>;
  using pci_info_map_type = std::map<pci_type, struct fi_info *>;

  inline static GPUloc &Get() {
    static GPUloc loc;
    return loc;
  }

  /**
   * @brief Constructor - discovers hardware topology and builds GPU affinity map
   */
  GPUloc() : hwloc_{Hwloc()}, pci_info_map_{GetPCIInfoMap()} {
    NVML_CHECK(nvmlInit());
    affinity_ = GetAffinity(hwloc_, pci_info_map_);
  }

  /**
   * @brief Destructor - shuts down NVML
   */
  ~GPUloc() { nvmlShutdown(); }

  /**
   * @brief Get GPU affinity mapping
   * @return Reference to GPU affinity map
   */
  const affinity_type &GetGPUAffinity() const noexcept { return affinity_; }

 private:
  /**
   * @brief Build GPU affinity mapping from hardware topology
   * @param hwloc Hardware topology object
   * @param pci_info_map Map of PCI devices to fabric info
   * @return GPU affinity mapping
   */
  static affinity_type GetAffinity(Hwloc &hwloc, const pci_info_map_type &pci_info_map) {
    std::unordered_map<hwloc_obj_t, GPUAffinity> gpuloc;
    for (auto &numa : hwloc.GetNumaNodes()) {
      for (auto &bridge : numa.bridge) {
        std::vector<hwloc_obj_t> gpus;
        std::vector<std::pair<hwloc_obj_t, struct fi_info *>> efas;
        for (auto pci : bridge.second) {
          if (Hwloc::IsGPU(pci)) {
            gpus.emplace_back(pci);
          } else if (Hwloc::IsEFA(pci)) {
            auto info = GetFiInfo(pci, pci_info_map);
            efas.emplace_back(std::pair<hwloc_obj_t, struct fi_info *>{pci, info});
          }
        }
        std::vector<hwloc_obj_t> cores(numa.cores.begin(), numa.cores.end());
        std::sort(cores.begin(), cores.end(), [](auto &&x, auto &&y) { return x->logical_index < y->logical_index; });
        for (auto &gpu : gpus) gpuloc[gpu] = GPUAffinity{gpu, numa.numanode, cores, efas};
      }
    }

    // create an affinity by GPU index
    unsigned count = 0;
    NVML_CHECK(nvmlDeviceGetCount(&count));
    GPULOC_ASSERT(count == gpuloc.size());
    affinity_type affinity;
    for (unsigned i = 0; i < count; ++i) {
      nvmlDevice_t device;
      nvmlPciInfo_t pci;
      NVML_CHECK(nvmlDeviceGetHandleByIndex(i, &device));
      NVML_CHECK(nvmlDeviceGetPciInfo(device, &pci));
      for (auto &[gpu, loc] : gpuloc) {
        if (gpu->attr->pcidev.domain == pci.domain and gpu->attr->pcidev.bus == pci.bus and gpu->attr->pcidev.dev == pci.device and
            gpu->attr->pcidev.func == 0) {
          affinity.emplace_back(loc);
        }
      }
    }
    GPULOC_ASSERT(gpuloc.size() == affinity.size());
    return affinity;
  }

  /**
   * @brief Build PCI device to fabric info mapping
   * @return Map of PCI device tuples to fabric info structures
   */
  static pci_info_map_type GetPCIInfoMap() {
    pci_info_map_type pci_info_map;
    auto &efa = EFA::Get();
    struct fi_info *info = efa.GetEFAInfo();
    for (auto p = info; !!p; p = p->next) {
      struct fid_nic *nic = p->nic;
      ASSERT(!!nic);
      ASSERT(nic->bus_attr and nic->bus_attr->bus_type == FI_BUS_PCI);
      auto attr = nic->bus_attr->attr.pci;
      pci_type pcidev = std::make_tuple(attr.domain_id, attr.bus_id, attr.device_id, attr.function_id);
      pci_info_map.emplace(pcidev, p);
    }
    return pci_info_map;
  }

  /**
   * @brief Get fabric info for a PCI device
   * @param pci PCI device object
   * @param pci_info_map Map of PCI devices to fabric info
   * @return Fabric info structure for the device
   */
  static struct fi_info *GetFiInfo(hwloc_obj_t pci, const pci_info_map_type &pci_info_map) {
    auto attr = pci->attr;
    pci_type pcidev = std::make_tuple(attr->pcidev.domain, attr->pcidev.bus, attr->pcidev.dev, attr->pcidev.func);
    return pci_info_map.at(pcidev);
  }

  /**
   * @brief Stream output operator for GPUloc
   * @param os Output stream
   * @param loc GPUloc object to output
   * @return Reference to output stream
   */
  friend std::ostream &operator<<(std::ostream &os, const GPUloc &loc) {
    for (size_t i = 0; i < loc.affinity_.size(); ++i) {
      auto &affinity = loc.affinity_[i];
      auto numanode = affinity.numanode;
      auto gpu = affinity.gpu;
      auto &cores = affinity.cores;
      auto &efas = affinity.efas;
      os << fmt::format("GPU({}) ({:02x}:{:02x}.{:01x})", i, gpu->attr->pcidev.bus, gpu->attr->pcidev.dev, gpu->attr->pcidev.func);
      os << fmt::format(" NUMA{}", numanode->logical_index);
      os << fmt::format(" Core{:>2}-Core{:>2}", cores.front()->logical_index, cores.back()->logical_index);
      os << "\n";
      for (auto e : efas) {
        auto efa = e.first;
        auto info = e.second;
        os << fmt::format("  EFA ({:02x}:{:02x}.{:01x})", efa->attr->pcidev.bus, efa->attr->pcidev.dev, efa->attr->pcidev.func);
        os << fmt::format(" fabric:{} domain:{}\n", info->fabric_attr->name, info->domain_attr->name);
      }
    }
    return os;
  }

 private:
  Hwloc hwloc_;
  pci_info_map_type pci_info_map_;
  affinity_type affinity_;
};
