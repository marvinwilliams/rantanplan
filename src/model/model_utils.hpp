#ifndef MODEL_UTILS_HPP
#define MODEL_UTILS_HPP

#include "model/model.hpp"

#include <algorithm>
#include <variant>
#include <vector>

namespace model {

bool is_subtype(const std::vector<Type> &types, TypePtr type,
                TypePtr supertype) {
  if (type == supertype) {
    return true;
  }
  while (get(types, type).parent != type) {
    type = get(types, type).parent;
    if (type == supertype) {
      return true;
    }
  }
  return false;
}

bool is_unifiable(const GroundPredicate &grounded_predicate,
                  const PredicateEvaluation &predicate) {
  if (grounded_predicate.definition != predicate.definition) {
    return false;
  }
  return std::mismatch(
             grounded_predicate.arguments.cbegin(),
             grounded_predicate.arguments.cend(), predicate.arguments.cbegin(),
             [](const auto &constant, const auto &argument) {
               const auto const_a = std::get_if<ConstantPtr>(&argument);
               return const_a == nullptr || constant == *const_a;
             })
             .first == grounded_predicate.arguments.cend();
}

bool holds(const State &state, const GroundPredicate &predicate,
           bool negated = false) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return state_predicate == predicate;
                  });
  return negated != in_state;
}

bool holds(const State &state, const PredicateEvaluation &predicate) {
  bool in_state =
      std::any_of(state.predicates.cbegin(), state.predicates.cend(),
                  [&predicate](const auto &state_predicate) {
                    return is_unifiable(state_predicate, predicate);
                  });
  return predicate.negated != in_state;
}

bool holds(const RelaxedState &state, const GroundPredicate &predicate,
           bool negated) {
  if (state.predicates.find(predicate) == state.predicates.end()) {
    return holds(state.initial_state, predicate, negated);
  }
  return true;
}

std::string print_condition(const AbstractProblem &problem,
                            const Condition &condition,
                            const AbstractAction &action) {
  std::string result;
  if (std::holds_alternative<Junction>(condition)) {
    const Junction &junction = std::get<Junction>(condition);
    result += "(";
    for (auto it = junction.arguments.cbegin(); it != junction.arguments.cend();
         ++it) {
      if (it != junction.arguments.cbegin()) {
        result +=
            junction.connective == Junction::Connective::And ? " ∧ " : " ∨ ";
      }
      result += print_condition(problem, *it, action);
    }
    result += ")";
  } else {
    const PredicateEvaluation &predicate =
        std::get<PredicateEvaluation>(condition);
    if (predicate.negated) {
      result += "¬";
    }
    result += get(problem.predicates, predicate.definition).name;
    result += "(";
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        result += ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        result += get(problem.constants, std::get<ConstantPtr>(*it)).name;
      } else {
        result +=
            "?" + get(action.parameters, std::get<ParameterPtr>(*it)).name;
      }
    }
    result += ")";
  }
  return result;
}

std::ostream &operator<<(std::ostream &out, const AbstractProblem &problem) {
  out << "Domain: " << problem.domain_name << '\n';
  out << "Problem: " << problem.problem_name << '\n';
  out << "Requirements:";
  for (auto &requirement : problem.requirements) {
    out << ' ' << requirement;
  }
  out << '\n';
  out << "Types:" << '\n';
  for (auto &type : problem.types) {
    out << '\t' << type.name;
    if (type.parent.i != 0) {
      out << " - " << type.name;
    }
    out << '\n';
  }
  out << "Constants:" << '\n';
  for (auto &constant : problem.constants) {
    out << '\t' << constant.name;
    if (constant.type.i != 0) {
      out << " - " << constant.name;
    }
    out << '\n';
  }
  out << "Predicates:" << '\n';
  for (auto &predicate : problem.predicates) {
    out << '\t' << predicate.name << '(';
    for (auto it = predicate.parameters.cbegin();
         it != predicate.parameters.cend(); ++it) {
      if (it != predicate.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << (*it).name;
      if (it->type.i != 0) {
        out << " - " << get(problem.types, it->type).name;
      }
    }
    out << ')' << '\n';
  }
  out << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    out << '\t' << action.name << '(';
    for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
         ++it) {
      if (it != action.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << it->name;
      if (it->type.i != 0) {
        out << " - " << get(problem.types, it->type).name;
      }
    }
    out << ')' << '\n';
    if (!std::holds_alternative<TrivialTrue>(action.precondition)) {
      out << '\t' << "Preconditions:" << '\n';
      out << '\t' << '\t'
          << print_condition(problem, action.precondition, action);
      out << '\n';
    }
    if (!std::holds_alternative<TrivialTrue>(action.effect)) {
      out << '\t' << "Effects:" << '\n';
      out << '\t' << '\t' << print_condition(problem, action.effect, action);
      out << '\n';
    }
    out << '\n';
  }
  out << "Initial state: "
      << print_condition(problem, problem.initial_state, AbstractAction{});
  out << '\n';
  out << "Goal: " << print_condition(problem, problem.goal, AbstractAction{});
  out << '\n';
  return out;
}

std::ostream &operator<<(std::ostream &out, const Problem &problem) {
  out << "Domain: " << problem.domain_name << '\n';
  out << "Problem: " << problem.problem_name << '\n';
  out << "Requirements:";
  for (auto &requirement : problem.requirements) {
    out << ' ' << requirement;
  }
  out << '\n';
  out << "Types:" << '\n';
  for (auto &type : problem.types) {
    out << '\t' << type.name;
    if (type.parent.i != 0) {
      out << " - " << get(problem.types, type.parent).name;
    }
    out << '\n';
  }
  out << "Constants:" << '\n';
  for (auto &constant : problem.constants) {
    out << '\t' << constant.name;
    if (constant.type.i != 0) {
      out << " - " << get(problem.types, constant.type).name;
    }
    out << '\n';
  }
  out << "Predicates:" << '\n';
  for (auto &predicate : problem.predicates) {
    out << '\t' << predicate.name << '(';
    for (auto it = predicate.parameters.cbegin();
         it != predicate.parameters.cend(); ++it) {
      out << '?' << it->name;
      if (it->type.i != 0) {
        out << " - " << get(problem.types, it->type).name;
      }
      if (it != predicate.parameters.cend()) {
        out << ", ";
      }
    }
    out << ')' << '\n';
  }
  out << "Actions:" << '\n';
  for (auto &action : problem.actions) {
    out << '\t' << action.name << '(';
    for (auto it = action.parameters.cbegin(); it != action.parameters.cend();
         ++it) {
      if (it != action.parameters.cbegin()) {
        out << ", ";
      }
      out << '?' << (*it).name;
      if (it->type.i != 0) {
        out << " - " << get(problem.types, it->type).name;
      }
    }
    out << ')' << '\n';
    if (action.preconditions.size() > 0) {
      out << '\t' << "Preconditions:";
      for (const auto &precondition : action.preconditions) {
        out << '\n';
        out << '\t' << '\t';
        if (precondition.negated) {
          out << "¬";
        }
        out << get(problem.predicates, precondition.definition).name;
        out << "(";
        for (auto it = precondition.arguments.cbegin();
             it != precondition.arguments.cend(); ++it) {
          if (it != precondition.arguments.cbegin()) {
            out << ", ";
          }
          if (std::holds_alternative<ConstantPtr>(*it)) {
            out << get(problem.constants, std::get<ConstantPtr>(*it)).name;
          } else {
            out << "?"
                << get(action.parameters, std::get<ParameterPtr>(*it)).name;
          }
        }
        out << ")";
      }
      out << '\n';
    }
    if (action.effects.size() > 0) {
      out << '\t' << "Effects:";
      for (const auto &effect : action.effects) {
        out << '\n';
        out << '\t' << '\t';
        if (effect.negated) {
          out << "¬";
        }
        out << get(problem.predicates, effect.definition).name;
        out << "(";
        for (auto it = effect.arguments.cbegin(); it != effect.arguments.cend();
             ++it) {
          if (it != effect.arguments.cbegin()) {
            out << ", ";
          }
          if (std::holds_alternative<ConstantPtr>(*it)) {
            out << get(problem.constants, std::get<ConstantPtr>(*it)).name;
          } else {
            out << "?" +
                       get(action.parameters, std::get<ParameterPtr>(*it)).name;
          }
        }
        out << ")";
      }
      out << '\n';
    }
    out << '\n';
  }
  out << "Initial state:";
  for (const auto &predicate : problem.initial_state) {
    out << '\n';
    out << '\t';
    if (predicate.negated) {
      out << "¬";
    }
    out << get(problem.predicates, predicate.definition).name;
    out << "(";
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        out << ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        out << get(problem.constants, std::get<ConstantPtr>(*it)).name;
      }
    }
    out << ")";
  }
  out << '\n';
  out << "Goal:";
  for (const auto &predicate : problem.goal) {
    out << '\n';
    out << '\t';
    if (predicate.negated) {
      out << "¬";
    }
    out << get(problem.predicates, predicate.definition).name;
    out << "(";
    for (auto it = predicate.arguments.cbegin();
         it != predicate.arguments.cend(); ++it) {
      if (it != predicate.arguments.cbegin()) {
        out << ", ";
      }
      if (std::holds_alternative<ConstantPtr>(*it)) {
        out << get(problem.constants, std::get<ConstantPtr>(*it)).name;
      }
    }
    out << ")";
  }
  out << '\n';
  return out;
}

} // namespace model

#endif /* end of include guard: MODEL_UTILS_HPP */
