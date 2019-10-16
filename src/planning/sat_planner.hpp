#ifndef SAT_PLANNER_HPP
#define SAT_PLANNER_HPP

#include "config.hpp"
#include "model/model.hpp"
#include "model/preprocess.hpp"
#include "planning/planner.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"
#include "sat/solver.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace planning {

template <typename Encoding> class SatPlanner : Planner {
public:
  using EncodingFormula = sat::Formula<typename Encoding::Variable>;
  explicit SatPlanner(model::Problem problem)
      : problem_{std::move(problem)}, support_{problem_} {}

  virtual ~SatPlanner() = default;

  static std::unique_ptr<sat::Solver>
  get_solver(const Config &config) noexcept {
    if (config.solver == "ipasir") {
      return std::make_unique<sat::Solver>(sat::IpasirSolver{});
    }
    return std::unique_ptr<sat::Solver>{};
  }

  Plan plan(const Config &config) noexcept final {
    if (config.preprocess) {
      model::preprocess(problem_, support_);
    }
    if (std::any_of(support_.get_problem().goal.begin(),
                    support_.get_problem().goal.end(), [this](const auto &p) {
                      return support_.is_rigid(p.definition) &&
                             support_.is_init(model::GroundPredicate{p}) ==
                                 p.negated;
                    })) {
      PRINT_INFO("Problem is trivially unsolvable");
      return {};
    }
    Encoding encoding{support_};
    return solve(config, encoding);
  }

protected:
  struct ActionVariable {
    model::ActionPtr action_ptr;
  };

  struct PredicateVariable {
    model::GroundPredicatePtr predicate_ptr;
    bool this_step;
  };

  struct ParameterVariable {
    model::ActionPtr action_ptr;
    size_t parameter_index;
    size_t constant_index;
  };

  struct GlobalParameterVariable {
    size_t parameter_index;
    size_t constant_index;
  };

  SatPlanner(const SatPlanner &planner) = default;
  SatPlanner(SatPlanner &&planner) = default;
  SatPlanner &operator=(const SatPlanner &planner) = default;
  SatPlanner &operator=(SatPlanner &&planner) = default;

private:
  static Plan solve(const Config &config, const Encoding &encoding) noexcept {
    auto solver = get_solver(config);
    if (!solver) {
      PRINT_ERROR("Unknown solver type \'%s\'", config.planner.c_str());
    }

    LOG_DEBUG(logger, "Initial state");
    add_formula(solver, encoding, encoding.get_initial_clauses(), 0);
    LOG_DEBUG(logger, "Universal clauses");
    add_formula(solver, encoding, encoding.get_universal_clauses(), 0);
    LOG_DEBUG(logger, "Transition clauses");
    add_formula(solver, encoding, encoding.get_transition_clauses(), 0);
    unsigned int step = 1;
    while (config.max_steps == 0 || step < config.max_steps) {
      LOG_DEBUG(logger, "Universal clauses");
      add_formula(solver, encoding, encoding.get_universal_clauses(), step);
      if (config.max_steps > 0) {
        PRINT_INFO("Solving step %u/%u", step, config.max_steps);
      } else {
        PRINT_INFO("Solving step %u", step);
      }
      LOG_DEBUG(logger, "Goal clauses");
      assume_goal(solver, encoding, step);
      auto model = solver->solve();
      if (model) {
        PRINT_INFO("Plan found");
        return encoding.extract_plan(*model, step);
      }
      LOG_DEBUG(logger, "Transition clauses");
      add_formula(solver, encoding, encoding.get_transition_clauses(), step);
      ++step;
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

  model::Problem problem_;
  model::Support support_;
};

} // namespace planning

#endif /* end of include guard: SAT_PLANNER_HPP */
