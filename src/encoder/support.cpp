#include "encoder/support.hpp"
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
  init_.reserve(problem_.init.size());
  for (const auto &predicate : problem_.init) {
    auto id = get_id(predicate);
    init_.insert(id);
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
        auto id = get_id(predicate);
        select_support(id, positive, is_effect)
            .emplace_back(ActionIndex{i}, ParameterAssignment{});
      }

      for (const auto &condition :
           is_effect ? action.effects : action.preconditions) {
        auto mapping = get_mapping(action, condition);
        for_each_instantiation(
            mapping.parameters, action,
            [&](auto assignment) {
              auto new_condition = condition;
              for (size_t i = 0; i < mapping.parameters.size(); ++i) {
                assert(assignment[i].first == mapping.parameters[i]);
                for (auto a : mapping.arguments[i]) {
                  assert(new_condition.arguments[a].get_parameter() ==
                         assignment[i].first);
                  new_condition.arguments[a].set(assignment[i].second);
                }
              }
              auto id = get_id(instantiate(new_condition));
              select_support(id, new_condition.positive, is_effect)
                  .emplace_back(ActionIndex{i}, std::move(assignment));
            },
            problem_);
      }
    }
  }
}
