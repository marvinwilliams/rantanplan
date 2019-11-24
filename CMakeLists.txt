cmake_minimum_required(VERSION 3.9)
project(rantanplan)
set(rantanplan_VERSION_MAJOR 0)
set(rantanplan_VERSION_MINOR 1)

function(PreventInTreeBuilds)
  get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)
  if("${srcdir}" STREQUAL "${bindir}")
    message("")
    message("In-tree builds are not supported.")
    message("Use a build/ directory (preferably not as a subdirectory)")
    message("    mkdir build")
    message("    cd build")
    message("    cmake <OPTIONS> ..")
    message("")
    message(FATAL_ERROR "Stopping build.")
  endif()
endfunction()

PreventInTreeBuilds()

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wnon-virtual-dtor
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wconversion
  -Wsign-conversion
  -Wnull-dereference
  )

configure_file(
  "src/build_config.in"
  "${CMAKE_CURRENT_BINARY_DIR}/build_config.hpp"
  )

include_directories(PRIVATE "src")
include_directories(PRIVATE "include")
include_directories(PRIVATE "lib/lexer/include")
include_directories(PRIVATE "lib/sat/include")
include_directories(PRIVATE "lib/logging/include")
include_directories(PRIVATE "lib/options/include")
include_directories(PRIVATE "third_party/sat_solvers")
include_directories(PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(SAT_SOLVER_DIR "${THIRD_PARTY_DIR}/sat_solvers")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libipasirglucose4.a"
  COMMAND BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} make all
  WORKING_DIRECTORY "${SAT_SOLVER_DIR}/glucose4"
  )
# set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "release" "ipasirglucoseglue.o")

link_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_executable(rantanplan_glucose
  "src/rantanplan.cpp"
  "lib/logging/src/logging/logging.cpp"
  "src/model/problem.cpp"
  "src/model/normalize.cpp"
  # "src/model/support.cpp"
  # "src/model/to_string.cpp"
  "src/pddl/parser.cpp"
  # "src/encoding/foreach.cpp"
  "src/pddl/pddl_visitor.cpp"
  "src/util/combinatorics.cpp"
  # "src/model/simple_preprocess.cpp"
  # "src/planning/planner.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/libipasirglucose4.a"
  )

target_link_libraries(rantanplan_glucose PRIVATE "ipasirglucose4")

option(STATIC_GLIBC "Link statically against glibc" OFF)
option(DEBUG_BUILD "Compile in debug mode" OFF)
option(DEBUG_LOG "Compile with debug logging capability" OFF)

if (STATIC_GLIBC)
  target_link_libraries(rantanplan_glucose PRIVATE -static-libgcc -static-libstdc++)
endif()

if(DEBUG_BUILD)
  set(CMAKE_BUILD_TYPE "Debug")
  add_definitions(-DDEBUG_BUILD -D_GLIBCXX_DEBUG)
  add_compile_options(
    -g -O0
    )
else()
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(
    -O3
    )
  if (NOT STATIC_GLIBC)
    add_compile_options(
      -march=native
      )
  endif()
endif()

if(DEBUG_LOG)
  add_definitions(-DDEBUG_LOG)
endif()

install(TARGETS rantanplan_glucose DESTINATION "${PROJECT_SOURCE_DIR}/bin")
