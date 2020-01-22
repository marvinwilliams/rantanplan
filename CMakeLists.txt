cmake_minimum_required(VERSION 3.9)
project(rantanplan)
set(rantanplan_VERSION_MAJOR 0)
set(rantanplan_VERSION_MINOR 1)

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
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/liblgl.a"
  COMMAND ./configure.sh && make liblgl.a
  COMMAND cp liblgl.a ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY "${SAT_SOLVER_DIR}/lingeling"
  )
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libipasirminisat220.a"
  COMMAND  make all
  COMMAND cp libipasirminisat220.a ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY "${SAT_SOLVER_DIR}/minisat220"
  )
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libipasirpicosat961.a"
  COMMAND make all
  COMMAND cp libipasirpicosat961.a ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY "${SAT_SOLVER_DIR}/picosat961"
  )
# set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "release" "ipasirglucoseglue.o")

link_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(COMMON_SOURCES
"lib/sat/include/sat/ipasir_solver.cpp"
"lib/sat/include/sat/solver.cpp"
"lib/logging/src/logging/logging.cpp"
"src/engine/engine.cpp"
"src/engine/oneshot_engine.cpp"
"src/engine/interrupt_engine.cpp"
"src/preprocess/preprocess.cpp"
"src/model/normalize.cpp"
"src/model/parsed/model.cpp"
"src/model/to_string.cpp"
"src/rantanplan.cpp"
"src/pddl/parser.cpp"
"src/pddl/model_builder.cpp"
"src/encoder/foreach_encoder.cpp"
"src/encoder/support.cpp"
"src/planner/planner.cpp"
"src/planner/sat_planner.cpp")

set(PARALLEL_SOURCES
"src/engine/parallel_engine.cpp"
"src/preprocess/parallel_preprocess.cpp")

add_executable(rantanplan_glucose ${COMMON_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirglucose4.a")
add_executable(rantanplan_glucose_parallel ${COMMON_SOURCES} ${PARALLEL_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirglucose4.a")
target_link_libraries(rantanplan_glucose PRIVATE "ipasirglucose4")
target_link_libraries(rantanplan_glucose_parallel PRIVATE "ipasirglucose4")
target_compile_definitions(rantanplan_glucose_parallel PRIVATE "-DPARALLEL")
target_link_libraries(rantanplan_glucose_parallel PRIVATE -lpthread)

add_executable(rantanplan_lingeling ${COMMON_SOURCES} ${SEQUENTIAL_SOURCES} ${SAT_SOLVER_DIR}/lingeling/Lingeling.cpp "${CMAKE_CURRENT_BINARY_DIR}/liblgl.a")
add_executable(rantanplan_lingeling_parallel ${COMMON_SOURCES} ${PARALLEL_SOURCES} ${SAT_SOLVER_DIR}/lingeling/Lingeling.cpp "${CMAKE_CURRENT_BINARY_DIR}/liblgl.a")
target_link_libraries(rantanplan_lingeling PRIVATE "lgl")
target_link_libraries(rantanplan_lingeling_parallel PRIVATE "lgl")
target_compile_definitions(rantanplan_lingeling_parallel PRIVATE "-DPARALLEL")
target_link_libraries(rantanplan_lingeling_parallel PRIVATE -lpthread)

add_executable(rantanplan_minisat ${COMMON_SOURCES} ${SEQUENTIAL_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirminisat220.a")
add_executable(rantanplan_minisat_parallel ${COMMON_SOURCES} ${PARALLEL_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirminisat220.a")
target_link_libraries(rantanplan_minisat PRIVATE "ipasirminisat220")
target_link_libraries(rantanplan_minisat_parallel PRIVATE "ipasirminisat220")
target_compile_definitions(rantanplan_minisat_parallel PRIVATE "-DPARALLEL")
target_link_libraries(rantanplan_minisat_parallel PRIVATE -lpthread)

add_executable(rantanplan_picosat ${COMMON_SOURCES} ${SEQUENTIAL_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirpicosat961.a")
add_executable(rantanplan_picosat_parallel ${COMMON_SOURCES} ${PARALLEL_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/libipasirpicosat961.a")
target_link_libraries(rantanplan_picosat PRIVATE "ipasirpicosat961")
target_link_libraries(rantanplan_picosat_parallel PRIVATE "ipasirpicosat961")
target_compile_definitions(rantanplan_picosat_parallel PRIVATE "-DPARALLEL")
target_link_libraries(rantanplan_picosat_parallel PRIVATE -lpthread)

option(DEBUG_BUILD "Compile in debug mode" OFF)

if(DEBUG_BUILD)
  set(CMAKE_BUILD_TYPE "Debug")
  add_definitions(-DDEBUG_BUILD -D_GLIBCXX_DEBUG)
  add_compile_options(-g -O0 -fsanitize=address -fsanitize=undefined)
else()
  set(CMAKE_BUILD_TYPE "Release")
  add_compile_options(-march=native -O3 -g)
endif()

install(TARGETS rantanplan_glucose DESTINATION "${PROJECT_SOURCE_DIR}/bin")
install(TARGETS rantanplan_glucose_parallel DESTINATION "${PROJECT_SOURCE_DIR}/bin")
