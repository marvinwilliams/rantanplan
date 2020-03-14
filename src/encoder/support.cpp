#include "encoder/support.hpp"
#include "grounder/grounder.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/normalized/utils.hpp"
#include "model/to_string.hpp"
#include "util/combination_iterator.hpp"
#include "util/timer.hpp"

#include <algorithm>
#include <cassert>
#include <variant>

using namespace normalized;

Support::Support(const Problem &problem, util::Seconds timeout = util::inf_time)
    : timeout_{timeout}, problem_{problem}{
  num_ground_atoms_ =
      std::accumulate(problem.predicates.begin(), problem.predicates.end(), 0ul,
                      [&problem](size_t sum, const auto &p) {
                        return sum + get_num_instantiated(p, problem);
                      });
  ground_atoms_.reserve(num_ground_atoms_);
  init_.reserve(problem_.init.size());
  for (const auto &predicate : problem_.init) {
    auto id = get_id(predicate);
    init_.insert(id);
  }
  set_predicate_support();
}

void Support::set_predicate_support() {
  condition_supports_.resize(num_ground_atoms_);
  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    for (auto is_effect : {true, false}) {
      for (const auto &[predicate, positive] :
           (is_effect ? action.ground_effects : action.ground_preconditions)) {
        auto id = get_id(predicate);
        select_support(id, positive, is_effect)
            .emplace_back(ActionIndex{i}, ParameterAssignment{});
      }
      for (const auto &condition :
           is_effect ? action.effects : action.preconditions) {
        if (global_timer.get_elapsed_time() > config.timeout ||
            timer_.get_elapsed_time() > timeout_) {
          throw TimeoutException{};
        }
        for (GroundAtomIterator it{condition.atom, action, problem_};
             it != GroundAtomIterator{}; ++it) {
          auto id = get_id(*it);
          select_support(id, condition.positive, is_effect)
              .emplace_back(ActionIndex{i}, std::move(it.get_assignment()));
        }
      }
    }
  }
}
