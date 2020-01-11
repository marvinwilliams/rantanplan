#include "planner/planner.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include <string>

logging::Logger Planner::logger{"Planner"};

void Planner::find_plan(const normalized::Problem &problem,
                        std::chrono::seconds timeout) {
  assert(status_ == Status::Ready);
  status_ = find_plan_impl(problem, timeout);
}

Planner::Status Planner::get_status() const noexcept { return status_; }

const Plan &Planner::get_plan() const noexcept {
  assert(status_ == Status::Success);
  return plan_;
}
