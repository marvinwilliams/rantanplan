#include "planner/planner.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include <string>

void Planner::find_plan(const std::shared_ptr<normalized::Problem> &problem,
                        unsigned int max_steps, std::chrono::seconds timeout) {
  assert(status_ == Status::Ready);
  status_ = find_plan_impl(problem, max_steps, timeout);
}

Planner::Status Planner::get_status() const noexcept { return status_; }

const Plan &Planner::get_plan() const noexcept {
  assert(status_ == Status::Success);
  return plan_;
}
