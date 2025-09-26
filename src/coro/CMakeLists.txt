file(GLOB src *.cc)
set(target coro)

set(ENV{PKG_CONFIG_PATH} "/opt/amazon/efa/lib/pkgconfig")
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

add_executable(${target} ${src})
target_compile_options(${target} PRIVATE -Wall -Werror -O3 -g)
target_include_directories(${target} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(${target} PRIVATE
  spdlog::spdlog
  Threads::Threads
)
