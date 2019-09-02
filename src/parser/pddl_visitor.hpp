#ifndef PDDL_VISITOR_HPP
#define PDDL_VISITOR_HPP

#include "model/model.hpp"
#include "parser/parser_exception.hpp"
#include "parser/visitor.hpp"
#include <sstream>
#include <string>

namespace parser {

class PddlAstParser : public visitor::Visitor<PddlAstParser> {
public:
  enum class Context {
    Requirements,
    Types,
    Constants,
    Predicates,
    Actions,
    Precondition,
    Effect,
    InitialState,
    Goal
  };

  model::Problem parse(const ast::AST &ast) {
    problem.types.emplace_back("_root", 0);
    auto equal_predicate = model::PredicateDefinition{"="};
    equal_predicate.parameters.emplace_back("first", 0);
    equal_predicate.parameters.emplace_back("second", 0);
    problem.predicates.push_back(std::move(equal_predicate));
    traverse(ast);
    return problem;
  }
  friend visitor::Visitor<PddlAstParser>;

private:
  using Visitor<PddlAstParser>::traverse;
  using Visitor<PddlAstParser>::visit_begin;
  using Visitor<PddlAstParser>::visit_end;

  Context context;
  model::Problem problem;
  bool negated;
  std::vector<model::Condition> condition_stack;
  model::TypePtr type_ptr = 0;

  template <typename List, typename Element>
  typename List::const_iterator find(const List &list, const Element &name) {
    return std::find_if(list.cbegin(), list.cend(),
                        [&name](auto &e) { return e.name == name.name; });
  }

  bool visit_begin(const ast::Domain &ast_domain) {
    problem.domain_name = ast_domain.name->name;
    return true;
  }

  bool visit_begin(const ast::Problem &ast_problem) {
    if (ast_problem.domain_ref->name != problem.domain_name) {
      std::string msg = "Domain reference \"" + ast_problem.domain_ref->name +
                        "\" does not match domain name \"" +
                        problem.domain_name + "\"";
      throw ParserException(ast_problem.domain_ref->location, msg.c_str());
    }
    problem.problem_name = ast_problem.name->name;
    return true;
  }

  bool visit_begin(const ast::SingleTypeNameList &list) {
    if (list.type) {
      auto p = find(problem.types, *list.type);
      if (list.type->name == "object") {
        type_ptr = 0;
        return true;
      }
      if (p == problem.types.cend()) {
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      type_ptr = std::distance(problem.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeNameList &) {
    type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::SingleTypeVariableList &list) {
    if (list.type) {
      auto p = find(problem.types, *list.type);
      if (list.type->name == "object") {
        type_ptr = 0;
        return true;
      }
      if (p == problem.types.cend()) {
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      type_ptr = std::distance(problem.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeVariableList &) {
    type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::NameList &list) {
    if (context == Context::Types) {
      for (auto &name : *list.elements) {
        if (find(problem.types, *name) != problem.types.cend()) {
          std::string msg = "Type \"" + name->name + "\" already defined";
          throw ParserException(name->location, msg.c_str());
        }
        problem.types.emplace_back(name->name, type_ptr);
      }
    } else if (context == Context::Constants) {
      for (auto &name : *list.elements) {
        if (find(problem.constants, *name) != problem.constants.cend()) {
          std::string msg = "Constant \"" + name->name + "\" already defined";
          throw ParserException(name->location, msg.c_str());
        }
        problem.constants.emplace_back(name->name, type_ptr);
      }
    } else if (context == Context::InitialState) {
      auto &predicate = problem.initial_state.back();
      if (problem.predicates[predicate.definition.p].parameters.size() !=
          list.elements->size()) {
        std::stringstream msg;
        msg << "Wrong number of arguments for predicate \""
            << problem.predicates[predicate.definition.p].name
            << "\": Expected "
            << problem.predicates[predicate.definition.p].parameters.size()
            << " but got " << list.elements->size();
        throw ParserException(list.location, msg.str().c_str());
      }
      for (size_t i = 0; i < list.elements->size(); ++i) {
        const auto &name = *(*list.elements)[i];
        auto p = find(problem.constants, name);
        if (p == problem.constants.cend()) {
          std::string msg = "Constant \"" + name.name + "\" undefined";
          throw ParserException(name.location, msg.c_str());
        }
        if (!model::is_subtype(problem, (*p).type,
                               problem.predicates[predicate.definition.p]
                                   .parameters[i]
                                   .type)) {
          std::string msg =
              "Type mismatch of argument \"" + name.name +
              "\": Expected a subtype of \"" +
              problem
                  .types[problem.predicates[predicate.definition.p]
                             .parameters[i]
                             .type.p]
                  .name +
              "\" but got type \"" + problem.types[(*p).type.p].name + "\"";
          throw ParserException(name.location, msg.c_str());
        }
        model::ConstantPtr constant_ptr =
            std::distance(problem.constants.cbegin(), p);
        problem.initial_state.back().arguments.push_back(
            std::move(constant_ptr));
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::VariableList &list) {
    if (context == Context::Predicates) {
      for (auto &variable : *list.elements) {
        if (find(problem.predicates.back().parameters, *variable) !=
            problem.predicates.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        problem.predicates.back().parameters.emplace_back(variable->name,
                                                          type_ptr);
      }
    } else if (context == Context::Actions) {
      for (auto &variable : *list.elements) {
        if (find(problem.actions.back().parameters, *variable) !=
            problem.actions.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        problem.actions.back().parameters.emplace_back(variable->name,
                                                       type_ptr);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::ArgumentList &list) {
    if (context == Context::Precondition || context == Context::Effect ||
        context == Context::Goal) {
      if (!std::holds_alternative<model::PredicateEvaluation>(
              condition_stack.back())) {
        throw ParserException("Internal error occurred while parsing");
      }
      auto &predicate =
          std::get<model::PredicateEvaluation>(condition_stack.back());
      if (problem.predicates[predicate.definition.p].parameters.size() !=
          list.elements->size()) {
        std::stringstream msg;
        msg << "Wrong number of arguments for predicate \""
            << problem.predicates[predicate.definition.p].name
            << "\": Expected "
            << problem.predicates[predicate.definition.p].parameters.size()
            << " but got " << list.elements->size();
        throw ParserException(list.location, msg.str().c_str());
      }
      for (size_t i = 0; i < list.elements->size(); ++i) {
        const auto &argument = *(*list.elements)[i];
        if (std::holds_alternative<ast::Name>(argument)) {
          const ast::Name &name = std::get<ast::Name>(argument);
          auto p = find(problem.constants, name);
          if (p == problem.constants.cend()) {
            std::string msg = "Constant \"" + name.name + "\" undefined";
            throw ParserException(name.location, msg.c_str());
          }
          if (!model::is_subtype(problem, (*p).type,
                                 problem.predicates[predicate.definition.p]
                                     .parameters[i]
                                     .type)) {
            std::string msg =
                "Type mismatch of argument \"" + name.name +
                "\": Expected a subtype of \"" +
                problem
                    .types[problem.predicates[predicate.definition.p]
                               .parameters[i]
                               .type.p]
                    .name +
                "\" but got type \"" + problem.types[(*p).type.p].name + "\"";
            throw ParserException(name.location, msg.c_str());
          }
          model::ConstantPtr constant_ptr =
              std::distance(problem.constants.cbegin(), p);
          predicate.arguments.push_back(std::move(constant_ptr));
        } else {
          const ast::Variable &variable = std::get<ast::Variable>(argument);
          auto p = find(problem.actions.back().parameters, variable);
          if (p == problem.actions.back().parameters.cend()) {
            std::string msg = "Parameter \"" + variable.name +
                              "\" undefined in action \"" +
                              problem.actions.back().name + "\"";
            throw ParserException(variable.location, msg.c_str());
          }
          if (!model::is_subtype(problem, (*p).type,
                                 problem.predicates[predicate.definition.p]
                                     .parameters[i]
                                     .type)) {
            std::string msg =
                "Type mismatch of argument \"" + variable.name +
                "\": Expected a subtype of \"" +
                problem
                    .types[problem.predicates[predicate.definition.p]
                               .parameters[i]
                               .type.p]
                    .name +
                "\" but got type \"" + problem.types[(*p).type.p].name + "\"";
            throw ParserException(variable.location, msg.c_str());
          }
          model::ParameterPtr parameter_ptr =
              std::distance(problem.actions.back().parameters.cbegin(), p);
          predicate.arguments.push_back(std::move(parameter_ptr));
        }
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::RequirementsDef &) {
    context = Context::Requirements;
    return true;
  }

  bool visit_begin(const ast::TypesDef &) {
    context = Context::Types;
    return true;
  }

  bool visit_begin(const ast::ConstantsDef &) {
    context = Context::Constants;
    return true;
  }

  bool visit_begin(const ast::PredicatesDef &) {
    context = Context::Predicates;
    return true;
  }

  bool visit_begin(const ast::ActionDef &ast_action) {
    context = Context::Actions;
    problem.actions.emplace_back(ast_action.name->name);
    return true;
  }

  bool visit_begin(const ast::ObjectsDef &) {
    context = Context::Constants;
    return true;
  }

  bool visit_begin(const ast::InitDef &) {
    context = Context::InitialState;
    return true;
  }

  bool visit_begin(const ast::GoalDef &) {
    context = Context::Goal;
    return true;
  }

  bool visit_begin(const ast::Effect &) {
    context = Context::Effect;
    negated = false;
    return true;
  }

  bool visit_begin(const ast::Precondition &) {
    context = Context::Precondition;
    negated = false;
    return true;
  }

  bool visit_begin(const ast::InitCondition &) {
    negated = false;
    return true;
  }

  bool visit_begin(const ast::InitNegation &) {
    negated = !negated;
    return true;
  }

  bool visit_begin(const ast::InitPredicate &ast_init_predicate) {
    auto p = find(problem.predicates, *ast_init_predicate.name);
    if (p == problem.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_init_predicate.name->name + "\" not defined";
      throw ParserException(ast_init_predicate.name->location, msg.c_str());
    }
    model::PredicatePtr predicate_ptr =
        std::distance(problem.predicates.cbegin(), p);
    auto predicate = model::PredicateEvaluation{std::move(predicate_ptr)};
    predicate.negated = negated;
    negated = false;
    problem.initial_state.push_back(std::move(predicate));
    return true;
  }

  bool visit_begin(const ast::Predicate &ast_predicate) {
    if (find(problem.predicates, *ast_predicate.name) !=
        problem.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" already defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    problem.predicates.emplace_back(ast_predicate.name->name);
    return true;
  }

  bool visit_begin(const ast::PredicateEvaluation &ast_predicate) {
    auto p = find(problem.predicates, *ast_predicate.name);
    if (p == problem.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" not defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    model::PredicatePtr predicate_ptr =
        std::distance(problem.predicates.cbegin(), p);
    auto predicate = model::PredicateEvaluation{std::move(predicate_ptr)};
    predicate.negated = negated;
    negated = false;
    condition_stack.push_back(std::move(predicate));
    return true;
  }

  bool visit_begin(const ast::Negation &) {
    negated = !negated;
    return true;
  }

  bool visit_end(const ast::Condition &) {
    auto condition = condition_stack.back();
    condition_stack.pop_back();
    if (condition_stack.empty()) {
      if (context == Context::Precondition) {
        problem.actions.back().precondition = std::move(condition);
      } else if (context == Context::Effect) {
        problem.actions.back().effect = std::move(condition);
      } else if (context == Context::Goal) {
        problem.goal = std::move(condition);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
    } else {
      if (std::holds_alternative<model::And>(condition_stack.back())) {
        std::get<model::And>(condition_stack.back())
            .arguments.push_back(std::move(condition));
      } else if (std::holds_alternative<model::Or>(condition_stack.back())) {
        std::get<model::Or>(condition_stack.back())
            .arguments.push_back(std::move(condition));
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
    }
    return true;
  }

  bool visit_begin(const ast::Conjunction &) {
    auto conjunction = model::And{};
    conjunction.negated = negated;
    negated = false;
    condition_stack.push_back(std::move(conjunction));
    return true;
  }

  bool visit_begin(const ast::Disjunction &) {
    auto disjunction = model::Or{};
    disjunction.negated = negated;
    condition_stack.push_back(std::move(disjunction));
    return true;
  }

  bool visit_begin(const ast::Requirement &ast_requirement) {
    problem.requirements.emplace_back(ast_requirement.name);
    return true;
  }
}; // namespace parser

} // namespace parser

#endif /* end of include guard: PDDL_VISITOR_HPP */
