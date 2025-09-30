file(GLOB src *.cc)
set(target batch)

set(ENV{PKG_CONFIG_PATH} "/opt/amazon/efa/lib/pkgconfig")
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(MPI REQUIRED)
find_package(CUDA REQUIRED)
pkg_check_modules(Efa IMPORTED_TARGET libefa)
pkg_check_modules(Fabric IMPORTED_TARGET libfabric)
pkg_check_modules(HWLOC REQUIRED hwloc)

add_executable(${target} ${src})
target_compile_options(${target} PRIVATE -Wall -Werror -O3 -g)
target_include_directories(${target} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${MPI_INCLUDE_PATH}"
  "${HWLOC_INCLUDE_DIRS}"
)
target_link_libraries(${target} PRIVATE
  PkgConfig::Efa
  PkgConfig::Fabric
  spdlog::spdlog
  Threads::Threads
  "${MPI_LIBRARIES}"
  "${NVML_LIBRARIES}"
  "${HWLOC_LIBRARIES}"
  "${CUDA_LIBRARIES}"
  cuda
)

set_target_properties(${target} PROPERTIES CUDA_RUNTIME_LIBRARY Shared)

