#include "encoding/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "util/combination_iterator.hpp"

#include <algorithm>
#include <cassert>
#include <variant>

using namespace normalized;

logging::Logger Support::logger{"Support"};

Support::Support(const Problem &problem) noexcept : problem_{problem} {
  num_instantations_ =
      std::accumulate(problem.predicates.begin(), problem.predicates.end(), 0ul,
                      [&problem](size_t sum, const auto &p) {
                        return sum + get_num_instantiated(p, problem);
                      });
  instantiations_.reserve(num_instantations_);
  LOG_INFO(logger, "The problem has %u predicates", num_instantations_);
  init_.reserve(problem_.init.size());
  for (const auto &predicate : problem_.init) {
    auto [it, success] =
        instantiations_.try_emplace(predicate, instantiations_.size());
    init_.insert(it->second);
  }
  LOG_INFO(logger, "Computing predicate support...");
  set_predicate_support();
}

void Support::set_predicate_support() noexcept {
  condition_supports_.resize(num_instantations_);
  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    for (auto is_effect : {true, false}) {
      for (const auto &[predicate, positive] :
           (is_effect ? action.eff_instantiated : action.pre_instantiated)) {
        auto [it, success] =
            instantiations_.try_emplace(predicate, instantiations_.size());
        select_support(it->second, positive, is_effect)
            .emplace_back(ActionIndex{i}, ParameterAssignment{});
      }

      for (const auto &predicate :
           is_effect ? action.effects : action.preconditions) {
        for_each_instantiation(
            get_referenced_parameters(action, predicate), action,
            [&](ParameterAssignment assignment) {
              auto parameters = action.parameters;
              for (auto [p, c] : assignment) {
                parameters[p].set(c);
              }
              auto instantiation = instantiate(predicate);
              auto [it, success] = instantiations_.try_emplace(
                  instantiation, instantiations_.size());
              select_support(it->second, predicate.positive, is_effect)
                  .emplace_back(ActionIndex{i}, std::move(assignment));
            },
            problem_);
      }
    }
  }
}
