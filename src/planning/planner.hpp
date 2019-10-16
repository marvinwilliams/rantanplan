#ifndef PLANNER_HPP
#define PLANNER_HPP

#include "config.hpp"
#include "model/model.hpp"

#include <utility>
#include <vector>

namespace planning {

extern logging::Logger logger;

using Plan = std::vector<model::Action>;

class Planner {
public:
  virtual ~Planner() = default;
  virtual Plan plan(const Config &config) = 0;

protected:
  Planner(const Planner &planner) = default;
  Planner(Planner &&planner) = default;
  Planner &operator=(const Planner &planner) = default;
  Planner &operator=(Planner &&planner) = default;
};

} // namespace planning

#endif /* end of include guard: PLANNER_HPP */
