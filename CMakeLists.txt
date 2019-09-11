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
  "src/build_config.in"
  "${CMAKE_CURRENT_BINARY_DIR}/build_config.hpp"
  )

include_directories(PRIVATE "src")
include_directories(PRIVATE "lib/lexer/include")
include_directories(PRIVATE "lib/sat/include")
include_directories(PRIVATE "third_party/sat_solvers")
include_directories(PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(SAT_SOLVER_DIR "${THIRD_PARTY_DIR}/sat_solvers")

add_custom_command(
  OUTPUT "${SAT_SOLVER_DIR}/glucose4/libipasirglucose4.a"
  COMMAND make all
  WORKING_DIRECTORY "${SAT_SOLVER_DIR}/glucose4"
  )

add_executable(rantanplan_glucose
  "src/rantanplan.cpp"
  "${SAT_SOLVER_DIR}/glucose4/libipasirglucose4.a"
  )

target_link_directories(rantanplan_glucose PRIVATE "${SAT_SOLVER_DIR}/glucose4")
target_link_libraries(rantanplan_glucose PRIVATE "ipasirglucose4")

option(DEBUG_MODE "Compile with additional debug information enabled" OFF)

if(DEBUG_MODE)
  set(CMAKE_BUILD_TYPE "Debug")
  add_definitions(-DDEBUG_MODE -DDEBUG_PRINT)
  add_compile_options(
    -g -O0
    )
else()
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(
    -O3 -march=native
    )
endif()

install(TARGETS rantanplan_glucose DESTINATION "${PROJECT_SOURCE_DIR}/bin")
