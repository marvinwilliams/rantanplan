#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"

#include <cassert>
#include <chrono>
#include <utility>
#include <vector>

extern logging::Logger planner_logger;

class Planner {
public:
  enum class Status { Ready, Success, Timeout, MaxStepsExceeded, Error };

  void find_plan(const std::shared_ptr<normalized::Problem> &problem,
                 unsigned int max_steps, std::chrono::seconds timeout);

  Status get_status() const noexcept;
  const Plan &get_plan() const noexcept;

  virtual ~Planner() = default;

protected:
  Status status_ = Status::Ready;
  Plan plan_;

private:
  virtual Status
  find_plan_impl(const std::shared_ptr<normalized::Problem> &problem,
                 unsigned int max_steps, std::chrono::seconds timeout) = 0;
};

#endif /* end of include guard: PLANNER_HPP */
