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
        for_each_instantiation(
            condition, action,
            [&](auto new_condition, auto assignment) {
              auto id = get_id(new_condition);
              select_support(id, new_condition.positive, is_effect)
                  .emplace_back(ActionIndex{i}, std::move(assignment));
            },
            problem_);
      }
    }
  }
}
