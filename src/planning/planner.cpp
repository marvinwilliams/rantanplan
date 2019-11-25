#include "planning/planner.hpp"

#include "logging/logging.hpp"
#include "model/model.hpp"
#include <string>

namespace planning {

logging::Logger Planner::logger{"Planning"};

std::string Planner::to_string(const Plan &plan,
                               const normalized::Problem &problem) const
    noexcept {
  std::stringstream ss;
  unsigned int step = 0;
  for (const auto &[handle, constants] : plan) {
    ss << step << ": " << '(' << problem.action_names[handle] << ' ';
    for (auto it = constants.cbegin(); it != constants.cend(); ++it) {
      if (it != constants.cbegin()) {
        ss << ' ';
      }
      ss << problem.constant_names[*it];
    }
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}

} // namespace planning
