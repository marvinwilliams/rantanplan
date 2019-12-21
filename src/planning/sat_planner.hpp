#ifndef SAT_PLANNER_HPP
#define SAT_PLANNER_HPP

#include "config.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/to_string.hpp"
#include "planning/planner.hpp"
#include "sat/ipasir_solver.hpp"
#include "sat/solver.hpp"

#include <memory>
#ifdef PARALLEL
#include <atomic>
#endif

template <typename Encoding> class SatPlanner final : public Planner {
public:
  static std::unique_ptr<sat::Solver>
  get_solver(const Config &config) noexcept {
    switch (config.solver) {
    case Config::Solver::Ipasir:
      return std::make_unique<sat::IpasirSolver>();
    }
    return std::make_unique<sat::IpasirSolver>();
  }

#ifdef PARALLEL
  Planner::Plan plan(const normalized::Problem &problem, const Config &config,
                     std::atomic_bool &plan_found) const noexcept override {
    Encoding encoding{problem, config};
    return solve(encoding, config, plan_found);
  }
#else
  Planner::Plan plan(const normalized::Problem &problem,
                     const Config &config) const noexcept override {
    Encoding encoding{problem, config};
    return solve(encoding, config);
  }
#endif

protected:
  SatPlanner &operator=(const SatPlanner &planner) = default;
  SatPlanner &operator=(SatPlanner &&planner) = default;

private:
#ifdef PARALLEL
  static Plan solve(const Encoding &encoding, const Config &config,
                    std::atomic_bool &plan_found) noexcept {
#else
  static Plan solve(const Encoding &encoding, const Config &config) noexcept {
#endif
    auto solver = get_solver(config);
    assert(solver);
    *solver << static_cast<int>(Encoding::SAT) << 0;
    *solver << -static_cast<int>(Encoding::UNSAT) << 0;
    add_formula(solver.get(), encoding, encoding.get_init(), 0);
    add_formula(solver.get(), encoding, encoding.get_universal_clauses(), 0);

    unsigned int step = 0;
    float current_step = 1.0f;
#ifdef PARALLEL
    while (!plan_found && (config.max_steps == 0 || step < config.max_steps)) {
#else
    while (config.max_steps == 0 || step < config.max_steps) {
#endif
      do {
        add_formula(solver.get(), encoding, encoding.get_transition_clauses(),
                    step);
        ++step;
        add_formula(solver.get(), encoding, encoding.get_universal_clauses(),
                    step);
      } while (step < static_cast<unsigned int>(current_step));
      if (config.max_steps > 0) {
        LOG_INFO(logger, "Solving step %u/%u", step, config.max_steps);
      } else {
        LOG_INFO(logger, "Solving step %u", step);
      }
      assume_goal(solver.get(), encoding, step);
      auto model = solver->solve();
      if (model) {
        bool found = false;
        if (plan_found.compare_exchange_strong(found, true)) {
          PRINT_INFO("Plan found");
          return encoding.extract_plan(*model, step);
        } else {
          return {};
        }
      }
      current_step *= config.step_factor;
    }
    PRINT_INFO("No plan could be found within %u steps", config.max_steps);
    return {};
  }

  static void add_formula(sat::Solver *solver, const Encoding &encoding,
                          const typename Encoding::Formula &formula,
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

#endif /* end of include guard: SAT_PLANNER_HPP */
