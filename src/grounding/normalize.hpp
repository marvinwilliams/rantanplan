#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "model/model.hpp"

#include <algorithm>
#include <vector>

namespace grounding {

model::Condition to_dnf(const model::Condition &condition) {
  if (std::holds_alternative<model::PredicateEvaluation>(condition)) {
    return condition;
  }
  auto &junction = std::get<model::Junction>(condition);
  auto new_junction = model::Junction{};
  new_junction.connective = junction.connective;
  std::vector<model::Condition> dnf_children;
  for (const auto &child : junction.arguments) {
    auto child_dnf = to_dnf(child);
    auto child_junction = std::get_if<model::Junction>(&child_dnf);
    if (child_junction && child_junction->connective == junction.connective) {
      new_junction.arguments.insert(new_junction.arguments.begin(),
                                    child_junction->arguments.begin(),
                                    child_junction->arguments.end());
    } else {
      new_junction.arguments.push_back(std::move(child_dnf));
    }
  }
  if (junction.connective == model::Junction::Connective::And) {
    auto first_disjunction = std::find_if(
        new_junction.arguments.cbegin(), new_junction.arguments.cend(),
        [](const auto &argument) {
          if (std::holds_alternative<model::Junction>(argument)) {
            return std::get<model::Junction>(argument).connective ==
                   model::Junction::Connective::Or;
          }
          return false;
        });
    if (first_disjunction != new_junction.arguments.cend()) {
      model::Junction disjunction =
          std::get<model::Junction>(*first_disjunction);
      new_junction.arguments.erase(first_disjunction);
      auto new_disjunction = model::Junction{};
      new_disjunction.connective = model::Junction::Connective::Or;
      for (auto &argument : disjunction.arguments) {
        auto new_conjunction = model::Junction{};
        new_conjunction.connective = model::Junction::Connective::And;
        new_conjunction.arguments = new_junction.arguments;
        new_conjunction.arguments.push_back(std::move(argument));
        new_disjunction.arguments.push_back(std::move(new_conjunction));
      }
      return to_dnf(new_disjunction);
    }
  }
  if (new_junction.arguments.size() == 1) {
    return new_junction.arguments[0];
  }
  return new_junction;
}

void normalize(model::Problem &problem) {
  for (auto &action : problem.actions) {
    if (action.precondition) {
      action.precondition = to_dnf(*action.precondition);
    }
    if (action.effect) {
      action.effect = to_dnf(*action.effect);
    }
  }
  problem.goal = to_dnf(problem.goal);
}

} // namespace grounding

#endif /* end of include guard: NORMALIZE_HPP */
