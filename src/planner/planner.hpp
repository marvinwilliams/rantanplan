#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "util/timer.hpp"

#include <cassert>
#include <utility>
#include <vector>

extern logging::Logger planner_logger;

class Planner {
public:
  Plan find_plan(const std::shared_ptr<normalized::Problem> &problem,
                 util::Seconds timeout);

  virtual ~Planner() = default;

private:
  virtual Plan
  find_plan_impl(const std::shared_ptr<normalized::Problem> &problem,
                 util::Seconds timeout) = 0;
};

#endif /* end of include guard: PLANNER_HPP */
