#ifndef SAT_PLANNER_HPP
#define SAT_PLANNER_HPP

#include "config.hpp"
#include "model/normalized_problem.hpp"
#include "model/preprocess.hpp"
#include "model/support.hpp"
#include "model/to_string.hpp"
#include "planning/planner.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"
#include "sat/solver.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace planning {

template <typename Encoding> class SatPlanner : public Planner {
public:
  using EncodingFormula = sat::Formula<typename Encoding::Variable>;

  static std::unique_ptr<sat::Solver>
  get_solver(const Config &config) noexcept {
    switch (config.solver) {
    case Config::Solver::Ipasir:
      return std::make_unique<sat::IpasirSolver>();
    default:
      return std::unique_ptr<sat::Solver>{};
    }
  }

  Planner::Plan plan(const normalized::Problem &problem,
                     const Config &config) noexcept final {
    PRINT_INFO("Encoding...");
    Encoding encoding{problem, config};
    return solve(config, encoding);
  }

protected:
  struct GlobalParameterVariable {
    size_t parameter_index;
    size_t constant_index;
  };

  /* SatPlanner(const SatPlanner &planner) = default; */
  /* SatPlanner(SatPlanner &&planner) = default; */
  SatPlanner &operator=(const SatPlanner &planner) = default;
  SatPlanner &operator=(SatPlanner &&planner) = default;

private:
  static Plan solve(const Config &config, const Encoding &encoding) noexcept {
    auto solver = get_solver(config);
    assert(solver);
    *solver << static_cast<int>(Encoding::SAT) << 0;
    *solver << -static_cast<int>(Encoding::UNSAT) << 0;
    LOG_DEBUG(logger, "Initial state");
    add_formula(solver.get(), encoding, encoding.get_init(), 0);
    LOG_DEBUG(logger, "Universal clauses");
    add_formula(solver.get(), encoding, encoding.get_universal_clauses(), 0);
    unsigned int step = 0;
    double current_step = 1.0;
    while (config.max_steps == 0 || step < config.max_steps) {
      do {
        LOG_DEBUG(logger, "Transition clauses");
        add_formula(solver.get(), encoding, encoding.get_transition_clauses(),
                    step);
        ++step;
        LOG_DEBUG(logger, "Universal clauses");
        add_formula(solver.get(), encoding, encoding.get_universal_clauses(),
                    step);
      } while (step < static_cast<unsigned int>(current_step));
      if (config.max_steps > 0) {
        PRINT_INFO("Solving step %u/%u", step, config.max_steps);
      } else {
        PRINT_INFO("Solving step %u", step);
      }
      LOG_DEBUG(logger, "Goal clauses");
      assume_goal(solver.get(), encoding, step);
      auto model = solver->solve();
      if (model) {
        PRINT_INFO("Plan found");
        return encoding.extract_plan(*model, step);
      }
      current_step *= config.step_factor;
    }
    PRINT_INFO("No plan could be found within %u steps", config.max_steps);
    return {};
  }

  static void add_formula(sat::Solver *solver, const Encoding &encoding,
                          const EncodingFormula &formula,
                          unsigned int step) noexcept {
    for (const auto &clause : formula.clauses) {
      for (const auto &literal : clause.literals) {
        *solver << encoding.get_sat_var(literal, step);
      }
      *solver << 0;
    }
  }

  static void assume_goal(sat::Solver *solver, const Encoding &encoding,
                          unsigned int step) noexcept {
    for (const auto &clause : encoding.get_goal_clauses().clauses) {
      for (const auto &literal : clause.literals) {
        solver->assume(encoding.get_sat_var(literal, step));
      }
    }
  }
};

} // namespace planning

#endif /* end of include guard: SAT_PLANNER_HPP */
