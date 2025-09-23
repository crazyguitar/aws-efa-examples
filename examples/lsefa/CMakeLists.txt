file(GLOB src *.cc)
set(target lsefa)

set(ENV{PKG_CONFIG_PATH} "/opt/amazon/efa/lib/pkgconfig")
find_package(PkgConfig REQUIRED)
pkg_check_modules(Efa IMPORTED_TARGET libefa)
pkg_check_modules(Fabric IMPORTED_TARGET libfabric)

add_executable(${target} ${src})
target_compile_options(${target} PRIVATE -Wall -Werror -O3 -g)
target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${target} PRIVATE PkgConfig::Efa PkgConfig::Fabric spdlog::spdlog)

