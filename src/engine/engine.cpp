#include "engine/engine.hpp"
#include "planner/sat_planner.hpp"

Engine::Engine(const std::shared_ptr<normalized::Problem> &problem)
    : problem_{problem} {}

Plan Engine::start_planning() {
  return start_planning_impl();
}
