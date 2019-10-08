#ifndef PDDL_VISITOR_HPP
#define PDDL_VISITOR_HPP

#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "pddl/parser_exception.hpp"
#include "pddl/visitor.hpp"
#include <sstream>
#include <string>

namespace pddl {

class PddlAstParser : public ast::Visitor<PddlAstParser> {
public:
  friend ast::Visitor<PddlAstParser>;

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

    bool negated = false;
    model::TypePtr type_ptr = 0;
    State state;
  };

  using State = Context::State;

  model::AbstractProblem parse(const AST &ast) {
    header_ = model::ProblemHeader{};
    problem_ = model::AbstractProblem{};
    context_ = Context{};
    condition_stack_.clear();

    problem_.types.emplace_back("_root", 0);
    auto equal_predicate = model::PredicateDefinition{"="};
    equal_predicate.parameters.emplace_back("first", 0);
    equal_predicate.parameters.emplace_back("second", 0);
    problem_.predicates.push_back(std::move(equal_predicate));

    traverse(ast);

    problem_.header = std::move(header_);

    return problem_;
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
    header_.domain_name = domain.name->name;
    return true;
  }

  bool visit_begin(const ast::Problem &problem) {
    if (problem.domain_ref->name != header_.domain_name) {
      std::string msg = "Domain reference \"" + problem.domain_ref->name +
                        "\" does not match domain name \"" +
                        header_.domain_name + "\"";
      throw ParserException(problem.domain_ref->location, msg.c_str());
    }
    header_.problem_name = problem.name->name;
    return true;
  }

  bool visit_begin(const ast::SingleTypeIdentifierList &list) {
    if (list.type) {
      const auto p = find(problem_.types, *list.type);
      if (p == problem_.types.cend()) {
        if (list.type->name == "object") {
          context_.type_ptr = 0;
          return true;
        }
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      context_.type_ptr = std::distance(problem_.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeIdentifierList &) {
    context_.type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::SingleTypeVariableList &list) {
    if (list.type) {
      const auto p = find(problem_.types, *list.type);
      if (list.type->name == "object") {
        context_.type_ptr = 0;
        return true;
      }
      if (p == problem_.types.cend()) {
        std::string msg = "Type \"" + list.type->name + "\" undefined";
        throw ParserException(list.type->location, msg.c_str());
      }
      context_.type_ptr = std::distance(problem_.types.cbegin(), p);
    }
    return true;
  }

  bool visit_end(const ast::SingleTypeVariableList &) {
    context_.type_ptr = 0;
    return true;
  }

  bool visit_begin(const ast::IdentifierList &list) {
    if (context_.state == State::Types) {
      for (const auto &name : *list.elements) {
        if (find(problem_.types, *name) != problem_.types.cend()) {
          std::string msg = "Type \"" + name->name + "\" already defined";
          throw ParserException(name->location, msg.c_str());
        }
        problem_.types.emplace_back(name->name, context_.type_ptr);
      }
    } else if (context_.state == State::Constants) {
      for (const auto &name : *list.elements) {
        if (find(problem_.constants, *name) != problem_.constants.cend()) {
          std::string msg = "Constant \"" + name->name + "\" already defined";
          throw ParserException(name->location, msg.c_str());
        }
        problem_.constants.emplace_back(name->name, context_.type_ptr);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::VariableList &list) {
    if (context_.state == State::Predicates) {
      for (const auto &variable : *list.elements) {
        if (find(problem_.predicates.back().parameters, *variable) !=
            problem_.predicates.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        problem_.predicates.back().parameters.emplace_back(variable->name,
                                                           context_.type_ptr);
      }
    } else if (context_.state == State::Actions) {
      for (const auto &variable : *list.elements) {
        if (find(problem_.actions.back().parameters, *variable) !=
            problem_.actions.back().parameters.cend()) {
          std::string msg =
              "Parameter \"" + variable->name + "\" already defined";
          throw ParserException(variable->location, msg.c_str());
        }
        problem_.actions.back().parameters.emplace_back(variable->name,
                                                        context_.type_ptr);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
    return true;
  }

  bool visit_begin(const ast::ArgumentList &list) {
    if (!std::holds_alternative<model::PredicateEvaluation>(
            condition_stack_.back())) {
      throw ParserException("Internal error occurred while parsing");
    }

    auto &predicate =
        std::get<model::PredicateEvaluation>(condition_stack_.back());

    if (problem_.predicates[predicate.definition].parameters.size() !=
        list.elements->size()) {
      std::stringstream msg;
      msg << "Wrong number of arguments for predicate \""
          << problem_.predicates[predicate.definition].name << "\": Expected "
          << problem_.predicates[predicate.definition].parameters.size()
          << " but got " << list.elements->size();
      throw ParserException(list.location, msg.str().c_str());
    }

    for (size_t i = 0; i < list.elements->size(); ++i) {
      const auto &argument = (*list.elements)[i];
      const auto supertype =
          problem_.predicates[predicate.definition].parameters[i].type;
      if (std::holds_alternative<std::unique_ptr<ast::Identifier>>(argument)) {
        const auto &name =
            *std::get<std::unique_ptr<ast::Identifier>>(argument);
        const auto p = find(problem_.constants, name);

        if (p == problem_.constants.cend()) {
          std::string msg = "Constant \"" + name.name + "\" undefined";
          throw ParserException(name.location, msg.c_str());
        }

        if (!model::is_subtype(problem_.types, p->type, supertype)) {
          std::string msg =
              "Type mismatch of argument \"" + name.name +
              "\": Expected a subtype of \"" + problem_.types[supertype].name +
              "\" but got type \"" + problem_.types[p->type].name + "\"";
          throw ParserException(name.location, msg.c_str());
        }

        model::ConstantPtr constant_ptr =
            std::distance(problem_.constants.cbegin(), p);
        predicate.arguments.push_back(constant_ptr);
      } else {
        const auto &variable =
            *std::get<std::unique_ptr<ast::Variable>>(argument);

        if (context_.state == State::InitialState ||
            context_.state == State::Goal) {
          throw ParserException(variable.location,
                                "Variables are only allowed in actions");
        }

        const auto p = find(problem_.actions.back().parameters, variable);

        if (p == problem_.actions.back().parameters.cend()) {
          std::string msg = "Parameter \"" + variable.name +
                            "\" undefined in action \"" +
                            problem_.actions.back().name + "\"";
          throw ParserException(variable.location, msg.c_str());
        }

        if (!model::is_subtype(problem_.types, p->type, supertype)) {
          std::string msg =
              "Type mismatch of argument \"" + variable.name +
              "\": Expected a subtype of \"" + problem_.types[supertype].name +
              "\" but got type \"" + problem_.types[p->type].name + "\"";
          throw ParserException(variable.location, msg.c_str());
        }
        model::ParameterPtr parameter_ptr =
            std::distance(problem_.actions.back().parameters.cbegin(), p);
        predicate.arguments.push_back(std::move(parameter_ptr));
      }
    }
    return true;
  }

  bool visit_begin(const ast::RequirementsDef &) {
    context_.state = State::Requirements;
    return true;
  }

  bool visit_begin(const ast::TypesDef &) {
    context_.state = State::Types;
    return true;
  }

  bool visit_begin(const ast::ConstantsDef &) {
    context_.state = State::Constants;
    return true;
  }

  bool visit_begin(const ast::PredicatesDef &) {
    context_.state = State::Predicates;
    return true;
  }

  bool visit_begin(const ast::ActionDef &action_def) {
    context_.state = State::Actions;
    model::AbstractAction action{};
    action.name = action_def.name->name;
    problem_.actions.push_back(std::move(action));
    return true;
  }

  bool visit_begin(const ast::ObjectsDef &) {
    context_.state = State::Constants;
    return true;
  }

  bool visit_begin(const ast::InitDef &) {
    context_.state = State::InitialState;
    return true;
  }

  bool visit_begin(const ast::GoalDef &) {
    context_.state = State::Goal;
    return true;
  }

  bool visit_begin(const ast::Effect &) {
    context_.state = State::Effect;
    return true;
  }

  bool visit_begin(const ast::Precondition &) {
    context_.state = State::Precondition;
    return true;
  }

  bool visit_begin(const ast::Negation &negation) {
    if (context_.negated && context_.state != State::Precondition) {
      throw ParserException(negation.location,
                            "Nested negation is only allowed in preconditions");
    }
    context_.negated = !context_.negated;
    return true;
  }

  bool visit_end(const ast::Negation &) {
    context_.negated = !context_.negated;
    return true;
  }

  bool visit_begin(const ast::Predicate &ast_predicate) {
    if (find(problem_.predicates, *ast_predicate.name) !=
        problem_.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" already defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    problem_.predicates.emplace_back(ast_predicate.name->name);
    return true;
  }

  bool visit_begin(const ast::PredicateEvaluation &ast_predicate) {
    const auto p = find(problem_.predicates, *ast_predicate.name);
    if (p == problem_.predicates.cend()) {
      std::string msg =
          "Predicate \"" + ast_predicate.name->name + "\" not defined";
      throw ParserException(ast_predicate.name->location, msg.c_str());
    }
    model::PredicatePtr predicate_ptr =
        std::distance(problem_.predicates.cbegin(), p);
    auto predicate = model::PredicateEvaluation{std::move(predicate_ptr)};
    predicate.negated = context_.negated;
    condition_stack_.push_back(std::move(predicate));
    return true;
  }

  bool visit_begin(const ast::Conjunction &conjunction) {
    auto junction = model::Junction{};
    if (context_.negated) {
      if (context_.state != State::Precondition) {
        throw ParserException(
            conjunction.location,
            "Negated conjunction only allowed in preconditions");
      }
      junction.connective = model::Junction::Connective::Or;
    } else {
      junction.connective = model::Junction::Connective::And;
    }
    condition_stack_.push_back(std::move(junction));
    return true;
  }

  bool visit_begin(const ast::Disjunction &disjunction) {
    if (context_.state != State::Precondition) {
      throw ParserException(disjunction.location,
                            "Disjunction only allowed in preconditions");
    }
    auto junction = model::Junction{};
    if (context_.negated) {
      junction.connective = model::Junction::Connective::And;
    } else {
      junction.connective = model::Junction::Connective::Or;
    }
    condition_stack_.push_back(std::move(junction));
    return true;
  }

  bool visit_end(const ast::Condition &condition) {
    if (std::holds_alternative<std::unique_ptr<ast::Empty>>(condition) ||
        std::holds_alternative<std::unique_ptr<ast::Negation>>(condition)) {
      return true;
    }
    auto last_condition = condition_stack_.back();
    condition_stack_.pop_back();
    if (condition_stack_.empty()) {
      if (context_.state == State::Precondition) {
        problem_.actions.back().precondition = std::move(last_condition);
      } else if (context_.state == State::Effect) {
        problem_.actions.back().effect = std::move(last_condition);
      } else if (context_.state == State::Goal) {
        problem_.goal = std::move(last_condition);
      } else if (context_.state == State::InitialState) {
        problem_.initial_state = std::move(last_condition);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
    } else {
      auto junction_ptr =
          std::get_if<model::Junction>(&condition_stack_.back());
      if (junction_ptr == nullptr) {
        throw ParserException("Internal error occurred while parsing");
      }
      junction_ptr->arguments.push_back(std::move(last_condition));
    }
    return true;
  }

  bool visit_begin(const ast::Requirement &ast_requirement) {
    header_.requirements.emplace_back(ast_requirement.name);
    return true;
  }

  Context context_;
  model::AbstractProblem problem_;
  model::ProblemHeader header_;
  std::vector<model::Condition> condition_stack_;
};

} // namespace pddl

#endif /* end of include guard: PDDL_VISITOR_HPP */
