#include "planning/planner.hpp"
#include "model/utils.hpp"

#include "logging/logging.hpp"
#include "model/model.hpp"
#include <string>

logging::Logger Planner::logger{"Planner"};

std::string Planner::to_string(const Plan &plan,
                               const normalized::Problem &problem) const
    noexcept {
  std::stringstream ss;
  unsigned int step = 0;
  for (auto [action, constants] : plan) {
    ss << step << ": " << '('
       << problem.action_names[get_index(action, problem)] << ' ';
    for (auto it = constants.cbegin(); it != constants.cend(); ++it) {
      if (it != constants.cbegin()) {
        ss << ' ';
      }
      ss << problem.constant_names[get_index(*it, problem)];
    }
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}
