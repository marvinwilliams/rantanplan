#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/model.hpp"

#include <cassert>
#include <utility>
#include <vector>

namespace planning {

extern logging::Logger logger;

using Plan = std::vector<model::Action>;

class Planner {
public:
  explicit Planner(const model::Problem &problem) : problem_{problem} {}
  explicit Planner(model::Problem &&problem) : problem_{std::move(problem)} {}

  virtual ~Planner() = default;
  virtual Plan plan(const Config &config) = 0;

  std::string to_string(const Plan &plan) const noexcept;

protected:
  model::Problem problem_;
};

} // namespace planning

#endif /* end of include guard: PLANNER_HPP */
