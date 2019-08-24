cmake_minimum_required(VERSION 3.9)
project(rantanplan)
set(rantanplan_VERSION_MAJOR 0)
set(rantanplan_VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
  -Wall
  -Wextra
  -Wnon-virtual-dtor
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wpedantic
  -Wconversion
  -Wsign-conversion
  -Wnull-dereference
  )

configure_file(
  "src/config.in"
  "${CMAKE_CURRENT_BINARY_DIR}/config.hpp"
  )

add_executable(rantanplan
  src/rantanplan.cpp
  )

target_include_directories(rantanplan PRIVATE "src/")
target_include_directories(rantanplan PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

install(TARGETS rantanplan DESTINATION "${PROJECT_SOURCE_DIR}/bin")
