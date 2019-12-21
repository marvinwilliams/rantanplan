#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"

#include <cassert>
#include <utility>
#include <vector>
#ifdef PARALLEL
#include <atomic>
#endif

class Planner {
public:
  using Plan = std::vector<std::pair<normalized::ActionIndex,
                                     std::vector<normalized::ConstantIndex>>>;

  static logging::Logger logger;

  virtual ~Planner() = default;
#ifdef PARALLEL
  virtual Plan plan(const normalized::Problem &problem, const Config &config,
                    std::atomic_bool &plan_found) const = 0;
#else
  virtual Plan plan(const normalized::Problem &problem,
                    const Config &config) const = 0;
#endif

  static std::string to_string(const Plan &plan,
                               const normalized::Problem &problem) noexcept;
};

#endif /* end of include guard: PLANNER_HPP */
