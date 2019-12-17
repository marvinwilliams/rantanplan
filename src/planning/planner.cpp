#include "planning/planner.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include <string>

logging::Logger Planner::logger{"Planner"};

std::string Planner::to_string(const Plan &plan,
                               const normalized::Problem &problem) const
    noexcept {
  std::stringstream ss;
  unsigned int step = 0;
  for (auto [action, arguments] : plan) {
    ss << step << ": " << '(' << problem.action_names[action] << ' ';
    for (auto it = arguments.cbegin(); it != arguments.cend(); ++it) {
      if (it != arguments.cbegin()) {
        ss << ' ';
      }
      ss << problem.constant_names[*it];
    }
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}
