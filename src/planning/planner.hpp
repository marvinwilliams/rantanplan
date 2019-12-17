#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"

#include <cassert>
#include <utility>
#include <vector>

class Planner {
public:
  using Plan = std::vector<std::pair<normalized::ActionIndex,
                                     std::vector<normalized::ConstantIndex>>>;

  static logging::Logger logger;

  virtual ~Planner() = default;
  virtual Plan plan(const normalized::Problem &problem,
                    const Config &config) = 0;

  std::string to_string(const Plan &plan,
                        const normalized::Problem &problem) const noexcept;

  normalized::Problem problem;
};

#endif /* end of include guard: PLANNER_HPP */
