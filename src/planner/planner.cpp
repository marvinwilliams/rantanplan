#include "planner/planner.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "util/timer.hpp"

#include <string>

Plan Planner::find_plan(const std::shared_ptr<normalized::Problem> &problem,
                        util::Seconds timeout) {
  return find_plan_impl(problem, timeout);
}
