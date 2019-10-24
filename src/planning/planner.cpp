#include "planning/planner.hpp"

#include "logging/logging.hpp"
#include "model/model.hpp"
#include <string>

namespace planning {

logging::Logger Planner::logger{"Planning"};

std::string Planner::to_string(const Plan &plan) const noexcept {
  std::stringstream ss;
  unsigned int step = 0;
  for (const auto &action : plan) {
    ss << step << ": " << '(' << action.name << ' ';
    for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
         ++it) {
      assert(it->constant);
      if (it != action.parameters.cbegin()) {
        ss << ' ';
      }
      ss << problem_.constants[*(it->constant)].name;
    }
    ss << ')' << '\n';
    ++step;
  }
  return ss.str();
}

} // namespace planning
