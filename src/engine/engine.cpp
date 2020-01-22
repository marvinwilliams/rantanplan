#include "engine/engine.hpp"
#include "planner/sat_planner.hpp"

Engine::Engine(const std::shared_ptr<normalized::Problem> &problem)
    : problem_{problem} {}

void Engine::start() {
  assert(status_ == Status::Ready);
  status_ = start_impl();
}

Engine::Status Engine::get_status() const { return status_; }

Plan Engine::get_plan() const {
  assert(status_ == Engine::Status::Success);
  return plan_;
}
