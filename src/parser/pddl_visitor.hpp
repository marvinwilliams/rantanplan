#ifndef PDDL_VISITOR_HPP
#define PDDL_VISITOR_HPP

#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "parser/parser_exception.hpp"
#include "parser/visitor.hpp"
#include <sstream>
#include <string>

namespace parser {

class PddlAstParser : public visitor::Visitor<PddlAstParser> {
public:
  friend visitor::Visitor<PddlAstParser>;

  struct Context {
    enum class State {
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

    State state;
    bool negated = false;
    model::TypePtr type_ptr = 0;
  };

  using State = Context::State;

  model::AbstractProblem parse(const ast::AST &ast) {
    base = model::ProblemBase{};
    problem = model::AbstractProblem{};
    context = Context{};
    condition_stack.clear();

    base.types.emplace_back("_root", 0);
    auto equal_predicate = model::PredicateDefinition{"="};
    equal_predicate.parameters.emplace_back("first", 0);
    equal_predicate.parameters.emplace_back("second", 0);
    base.predicates.push_back(std::move(equal_predicate));

    traverse(ast);

    problem.base = std::move(base);

    return problem;
  }

private:
  using Visitor<PddlAstParser>::traverse;
  using Visitor<PddlAstParser>::visit_begin;
  using Visitor<PddlAstParser>::visit_end;

  template <typename List, typename Element>
  typename List::const_iterator find(const List &list,
                                     const Element &name) const {
    return std::find_if(list.cbegin(), list.cend(),
                        [&name](const auto &e) { return e.name == name.name; });
  }

  bool visit_begin(const ast::Domain &domain) {
    base.domain_name = domain.name->name;
    return true;
  }

  bool visit_begin(const ast::Problem &problem) {
    if (problem.domain_ref->name != base.domain_name) {
      std::string msg = "Domain reference \"" + problem.domain_ref->name +
                        "\" does not match domain name \"" + base.domain_name +
                        "\"";
      throw ParserException(problem.domain_ref->location, msg.c_str());
    }
    base.problem_name = problem.name->name;
    return true;
  }

  bool visit_begin(const ast::SingleTypeIdentifierList &list) {
    if (list.type) {
      const auto p = find(base.types, *list.type);
      if (list.type->name == "object") {
        context.type_ptr = 0;
        return true;
      }
      if (p == base.types.cend()) {
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      context.type_ptr = std::distance(base.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeIdentifierList &) {
    context.type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::SingleTypeVariableList &list) {
    if (list.type) {
      const auto p = find(base.types, *list.type);
      if (list.type->name == "object") {
        context.type_ptr = 0;
        return true;
      }
      if (p == base.types.cend()) {
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      context.type_ptr = std::distance(base.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeVariableList &) {
    context.type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::IdentifierList &list) {
    if (context.state == State::Types) {
      for (const auto &name : *list.elements) {
        if (name->name == "object" ||
            find(base.types, *name) != base.types.cend()) {
          std::string msg = "Type \"" + name->name +
                            "\" already defined (\"object\" not allowed)";
          throw ParserException(name->location, msg.c_str());
        }
        base.types.emplace_back(name->name, context.type_ptr);
      }
    } else if (context.state == State::Constants) {
      for (const auto &name : *list.elements) {
        if (find(base.constants, *name) != base.constants.cend()) {
          std::string msg = "Constant \"" + name->name + "\" already defined";
          throw ParserException(name->location, msg.c_str());
        }
        base.constants.emplace_back(name->name, context.type_ptr);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::VariableList &list) {
    if (context.state == State::Predicates) {
      for (const auto &variable : *list.elements) {
        if (find(base.predicates.back().parameters, *variable) !=
            base.predicates.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        base.predicates.back().parameters.emplace_back(variable->name,
                                                       context.type_ptr);
      }
    } else if (context.state == State::Actions) {
      for (const auto &variable : *list.elements) {
        if (find(problem.actions.back().parameters, *variable) !=
            problem.actions.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        problem.actions.back().parameters.emplace_back(variable->name,
                                                       context.type_ptr);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::ArgumentList &list) {
    if (!std::holds_alternative<model::PredicateEvaluation>(
            condition_stack.back())) {
      throw ParserException("Internal error occurred while parsing");
    }

    auto &predicate =
        std::get<model::PredicateEvaluation>(condition_stack.back());

    if (base.predicates[predicate.definition].parameters.size() !=
        list.elements->size()) {
      std::stringstream msg;
      msg << "Wrong number of arguments for predicate \""
          << base.predicates[predicate.definition].name << "\": Expected "
          << base.predicates[predicate.definition].parameters.size()
          << " but got " << list.elements->size();
      throw ParserException(list.location, msg.str().c_str());
    }

    for (size_t i = 0; i < list.elements->size(); ++i) {
      const auto &argument = (*list.elements)[i];
      const auto supertype =
          base.predicates[predicate.definition].parameters[i].type;
      if (std::holds_alternative<std::unique_ptr<ast::Identifier>>(argument)) {
        const auto &name =
            *std::get<std::unique_ptr<ast::Identifier>>(argument);
        const auto p = find(base.constants, name);

        if (p == base.constants.cend()) {
          std::string msg = "Constant \"" + name.name + "\" undefined";
          throw ParserException(name.location, msg.c_str());
        }

        if (!model::is_subtype(base.types, p->type, supertype)) {
          std::string msg = "Type mismatch of argument \"" + name.name +
                            "\": Expected a subtype of \"" +
                            base.types[supertype].name + "\" but got type \"" +
                            base.types[p->type].name + "\"";
          throw ParserException(name.location, msg.c_str());
        }

        model::ConstantPtr constant_ptr =
            std::distance(base.constants.cbegin(), p);
        predicate.arguments.push_back(constant_ptr);
      } else {
        const auto &variable =
            *std::get<std::unique_ptr<ast::Variable>>(argument);

        if (context.state == State::InitialState ||
            context.state == State::Goal) {
          throw ParserException(variable.location,
                                "Variables are only allowed in actions");
        }

        const auto p = find(problem.actions.back().parameters, variable);

        if (p == problem.actions.back().parameters.cend()) {
          std::string msg = "Parameter \"" + variable.name +
                            "\" undefined in action \"" +
                            problem.actions.back().name + "\"";
          throw ParserException(variable.location, msg.c_str());
        }

        if (!model::is_subtype(base.types, p->type, supertype)) {
          std::string msg = "Type mismatch of argument \"" + variable.name +
                            "\": Expected a subtype of \"" +
                            base.types[supertype].name + "\" but got type \"" +
                            base.types[p->type].name + "\"";
          throw ParserException(variable.location, msg.c_str());
        }
        model::ParameterPtr parameter_ptr =
            std::distance(problem.actions.back().parameters.cbegin(), p);
        predicate.arguments.push_back(std::move(parameter_ptr));
      }
    }
    return true;
  }

  bool visit_begin(const ast::RequirementsDef &) {
    context.state = State::Requirements;
    return true;
  }

  bool visit_begin(const ast::TypesDef &) {
    context.state = State::Types;
    return true;
  }

  bool visit_begin(const ast::ConstantsDef &) {
    context.state = State::Constants;
    return true;
  }

  bool visit_begin(const ast::PredicatesDef &) {
    context.state = State::Predicates;
    return true;
  }

  bool visit_begin(const ast::ActionDef &action_def) {
    context.state = State::Actions;
    model::AbstractAction action{};
    action.name = action_def.name->name;
    problem.actions.push_back(std::move(action));
    return true;
  }

  bool visit_begin(const ast::ObjectsDef &) {
    context.state = State::Constants;
    return true;
  }

  bool visit_begin(const ast::InitDef &) {
    context.state = State::InitialState;
    return true;
  }

  bool visit_begin(const ast::GoalDef &) {
    context.state = State::Goal;
    return true;
  }

  bool visit_begin(const ast::Effect &) {
    context.state = State::Effect;
    return true;
  }

  bool visit_begin(const ast::Precondition &) {
    context.state = State::Precondition;
    return true;
  }

  bool visit_begin(const ast::Negation &negation) {
    if (context.negated && context.state != State::Precondition) {
      throw ParserException(negation.location,
                            "Nested negation is only allowed in preconditions");
    }
    context.negated = !context.negated;
    return true;
  }

  bool visit_end(const ast::Negation &) {
    context.negated = !context.negated;
    return true;
  }

  bool visit_begin(const ast::Predicate &ast_predicate) {
    if (find(base.predicates, *ast_predicate.name) != base.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" already defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    base.predicates.emplace_back(ast_predicate.name->name);
    return true;
  }

  bool visit_begin(const ast::PredicateEvaluation &ast_predicate) {
    const auto p = find(base.predicates, *ast_predicate.name);
    if (p == base.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" not defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    model::PredicatePtr predicate_ptr =
        std::distance(base.predicates.cbegin(), p);
    auto predicate = model::PredicateEvaluation{std::move(predicate_ptr)};
    predicate.negated = context.negated;
    condition_stack.push_back(std::move(predicate));
    return true;
  }

  bool visit_begin(const ast::Conjunction &conjunction) {
    auto junction = model::Junction{};
    if (context.negated) {
      if (context.state != State::Precondition) {
        throw ParserException(
            conjunction.location,
            "Negated conjunction only allowed in preconditions");
      }
      junction.connective = model::Junction::Connective::Or;
    } else {
      junction.connective = model::Junction::Connective::And;
    }
    condition_stack.push_back(std::move(junction));
    return true;
  }

  bool visit_begin(const ast::Disjunction &disjunction) {
    if (context.state != State::Precondition) {
      throw ParserException(disjunction.location,
                            "Disjunction only allowed in preconditions");
    }
    auto junction = model::Junction{};
    if (context.negated) {
      junction.connective = model::Junction::Connective::And;
    } else {
      junction.connective = model::Junction::Connective::Or;
    }
    condition_stack.push_back(std::move(junction));
    return true;
  }

  bool visit_end(const ast::Condition &condition) {
    if (std::holds_alternative<std::unique_ptr<ast::Empty>>(condition) ||
        std::holds_alternative<std::unique_ptr<ast::Negation>>(condition)) {
      return true;
    }
    auto last_condition = condition_stack.back();
    condition_stack.pop_back();
    if (condition_stack.empty()) {
      if (context.state == State::Precondition) {
        problem.actions.back().precondition = std::move(last_condition);
      } else if (context.state == State::Effect) {
        problem.actions.back().effect = std::move(last_condition);
      } else if (context.state == State::Goal) {
        problem.goal = std::move(last_condition);
      } else if (context.state == State::InitialState) {
        problem.initial_state = std::move(last_condition);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
    } else {
      auto junction_ptr = std::get_if<model::Junction>(&condition_stack.back());
      if (junction_ptr == nullptr) {
        throw ParserException("Internal error occurred while parsing");
      }
      junction_ptr->arguments.push_back(std::move(last_condition));
    }
    return true;
  }

  bool visit_begin(const ast::Requirement &ast_requirement) {
    base.requirements.emplace_back(ast_requirement.name);
    return true;
  }

  Context context;
  model::AbstractProblem problem;
  std::vector<model::Condition> condition_stack;
}; // namespace parser

} // namespace parser

#endif /* end of include guard: PDDL_VISITOR_HPP */
