cmake_minimum_required(VERSION 3.17)
include(CheckLanguage)
project(efa LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
include(cmake/packages.cmake)
include_directories(
  /opt/amazon/efa/include
  /usr/local/cuda/include
)
link_directories(
  /opt/amazon/efa/lib
  /usr/local/cuda/lib64
)

add_subdirectory(examples)
