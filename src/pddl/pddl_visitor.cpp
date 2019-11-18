#include "pddl/pddl_visitor.hpp"
#include "model/model.hpp"
#include "model/model_utils.hpp"
#include "model/problem.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser_exception.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace pddl {

logging::Logger PddlAstParser::logger{"Ast"};

Problem PddlAstParser::parse(const AST &ast) {
  LOG_INFO(logger, "Traverse AST");
  problem_ = Problem{};
  context_ = Context{};

  problem_.add_predicate("=")
      .add_parameter_type(problem_.get_root_type().name)
      .add_parameter_type(problem_.get_root_type().name);

  try {
    traverse(ast);
  } catch (const ModelException &e) {
    throw ParserException{*current_location_, "Error constructing the model: " +
                                                  std::string{e.what()}};
  }

  LOG_INFO(logger,
           "Problem has\n%lu requirements\n%lu types\n%lu constants\n%lu "
           "predicates\n%lu actions",
           context_.num_requirements, context_.num_types,
           context_.num_constants, context_.num_predicates,
           context_.num_actions);

  return problem_;
}

bool PddlAstParser::visit_begin(const ast::Domain &domain) {
  LOG_DEBUG(logger, "Visiting domain '%s'", domain.name->name.c_str());
  problem_.set_domain_name(domain.name->name);
  return true;
}

bool PddlAstParser::visit_begin(const ast::Problem &problem) {
  LOG_DEBUG(logger, "Visiting problem '%s' with domain reference '%s'",
            problem.name->name.c_str(), problem.domain_ref->name.c_str());
  problem_.set_problem_name(problem.name->name, problem.domain_ref->name);
  return true;
}

bool PddlAstParser::visit_begin(const ast::SingleTypeIdentifierList &list) {
  LOG_DEBUG(logger, "Visiting identifier list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    context_.current_type = problem_.get_type(list.type->name).name;
  }
  return true;
}

bool PddlAstParser::visit_end(const ast::SingleTypeIdentifierList &) {
  context_.current_type = problem_.get_root_type().name;
  return true;
}

bool PddlAstParser::visit_begin(const ast::SingleTypeVariableList &list) {
  LOG_DEBUG(logger, "Visiting variable list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    context_.current_type = problem_.get_type(list.type->name).name;
  }
  return true;
}

bool PddlAstParser::visit_end(const ast::SingleTypeVariableList &) {
  context_.current_type = problem_.get_root_type().name;
  return true;
}

bool PddlAstParser::visit_begin(const ast::IdentifierList &list) {
  switch (context_.state) {
  case State::Types:
    LOG_DEBUG(logger, "Visiting identifier list as types");
    for (const auto &name : *list.elements) {
      problem_.add_type(name->name, context_.current_type);
    }
    break;
  case State::Constants:
    LOG_DEBUG(logger, "Visiting identifier list as constants");
    for (const auto &name : *list.elements) {
      problem_.add_constant(name->name, context_.current_type);
    }
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::VariableList &list) {
  if (context_.state != State::Predicates && context_.state != State::Actions) {
    throw ParserException("Internal error occurred while parsing");
  }
  LOG_DEBUG(logger, "Visiting variable list as %s parameters",
            context_.state == State::Predicates ? "predicate" : "action");
  for (const auto &variable : *list.elements) {
    if (auto [it, success] = context_.parameter_lookup.insert(
            {variable->name,
             model::ParameterHandle{context_.parameter_lookup.size()}});
        success) {
      if (context_.state == State::Predicates) {
        problem_->predicates.back().parameter_types.push_back(
            context_.type_handle);
        problem_->predicate_names.push_back(it->first);
      } else {
        problem_->actions.back().parameters.emplace_back(false,
                                                         context_.type_handle);
        problem_->action_names.push_back(it->first);
      }
    } else {
      std::string msg = "Parameter \'" + variable->name + "\' already defined";
      throw ParserException(variable->location, msg);
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::ArgumentList &list) {
  LOG_DEBUG(logger, "Visiting argument list");

  auto predicate =
      std::get_if<model::ConditionPredicate>(&context_.condition_stack_.back());

  if (!predicate) {
    throw ParserException("Internal error occurred while parsing");
  }

  if (problem_->predicates[predicate->definition].parameter_types.size() !=
      list.elements->size()) {
    std::stringstream msg;
    msg << "Wrong number of arguments: Expected "
        << problem_->predicates[predicate->definition].parameter_types.size()
        << " but got " << list.elements->size();
    throw ParserException(list.location, msg.str());
  }

  for (size_t i = 0; i < list.elements->size(); ++i) {
    const auto &argument = (*list.elements)[i];
    const auto predicate_type =
        problem_->predicates[predicate->definition].parameter_types[i];
    if (auto name = std::get_if<std::unique_ptr<ast::Identifier>>(&argument)) {
      if (auto it = context_.constant_lookup.find((*name)->name);
          it != context_.constant_lookup.end()) {
        auto constant_type = problem_->constants[it->second].type;
        if (!model::is_subtype(constant_type, predicate_type,
                               problem_->types)) {
          std::string msg = "Type mismatch of argument \'" + (*name)->name +
                            "\': Expected a subtype of \'" +
                            problem_->type_names[predicate_type] +
                            "\' but got type \'" +
                            problem_->type_names[constant_type] + "\'";
          throw ParserException((*name)->location, msg);
        }

        predicate->arguments.emplace_back(true, it->second);
      } else {
        std::string msg = "Constant \'" + (*name)->name + "\' undefined";
        throw ParserException((*name)->location, msg);
      }
    } else if (auto variable =
                   std::get_if<std::unique_ptr<ast::Variable>>(&argument)) {
      if (context_.state == State::InitialState ||
          context_.state == State::Goal) {
        throw ParserException(
            (*variable)->location,
            "Variables are only allowed in preconditions and effects");
      }

      if (auto it = context_.parameter_lookup.find((*variable)->name);
          it != context_.parameter_lookup.end()) {
        auto parameter_type = model::TypeHandle{
            problem_->actions.back().parameters[it->second].index};
        if (!model::is_subtype(parameter_type, predicate_type,
                               problem_->types)) {
          std::string msg = "Type mismatch of argument \'" + (*variable)->name +
                            "\': Expected a subtype of \'" +
                            problem_->type_names[predicate_type] +
                            "\' but got type \'" +
                            problem_->type_names[parameter_type] + "\'";
          throw ParserException((*variable)->location, msg);
        }
        predicate->arguments.emplace_back(false, it->second);
      } else {
        std::string msg = "Parameter \'" + (*variable)->name +
                          "\' undefined in action \'" +
                          problem_->action_names.back() + "\'";
        throw ParserException((*variable)->location, msg);
      }
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::RequirementsDef &) {
  LOG_DEBUG(logger, "Visiting requirements definition");
  context_.state = State::Requirements;
  return true;
}

bool PddlAstParser::visit_begin(const ast::TypesDef &) {
  LOG_DEBUG(logger, "Visiting types definition");
  context_.state = State::Types;
  return true;
}

bool PddlAstParser::visit_begin(const ast::ConstantsDef &) {
  LOG_DEBUG(logger, "Visiting constants definition");
  context_.state = State::Constants;
  return true;
}

bool PddlAstParser::visit_begin(const ast::PredicatesDef &) {
  LOG_DEBUG(logger, "Visiting predicates definition");
  context_.state = State::Predicates;
  context_.parameter_lookup.clear();
  return true;
}

bool PddlAstParser::visit_begin(const ast::ActionDef &action_def) {
  LOG_DEBUG(logger, "Visiting action definition");
  context_.state = State::Actions;
  context_.parameter_lookup.clear();
  model::AbstractAction action{};
  problem_->actions.push_back(std::move(action));
  problem_->action_names.push_back(action_def.name->name);
  return true;
}

bool PddlAstParser::visit_begin(const ast::ObjectsDef &) {
  LOG_DEBUG(logger, "Visiting objects definition");
  context_.state = State::Constants;
  return true;
}

bool PddlAstParser::visit_begin(const ast::InitDef &) {
  LOG_DEBUG(logger, "Visiting init definition");
  context_.state = State::InitialState;
  return true;
}

bool PddlAstParser::visit_begin(const ast::GoalDef &) {
  LOG_DEBUG(logger, "Visiting goal definition");
  context_.state = State::Goal;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Effect &) {
  context_.state = State::Effect;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Precondition &) {
  context_.state = State::Precondition;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Negation &negation) {
  if (context_.negated && context_.state != State::Precondition) {
    throw ParserException(negation.location,
                          "Nested negation is only allowed in preconditions");
  }
  context_.negated = !context_.negated;
  return true;
}

bool PddlAstParser::visit_end(const ast::Negation &) {
  context_.negated = !context_.negated;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Predicate &ast_predicate) {
  if (auto [it, success] = context_.predicate_lookup.insert(
          {ast_predicate.name->name,
           model::PredicateHandle{context_.predicate_lookup.size()}});
      success) {
    problem_->predicates.emplace_back(ast_predicate.name->name);
    problem_->predicate_names.push_back(it->first);
  } else {
    std::string msg =
        "Predicate \'" + ast_predicate.name->name + "\' already defined";
    throw ParserException(ast_predicate.name->location, msg);
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::PredicateEvaluation &ast_predicate) {
  if (auto it = context_.predicate_lookup.find(ast_predicate.name->name);
      it != context_.predicate_lookup.end()) {
    auto predicate = model::ConditionPredicate{};
    predicate.definition = it->second;
    predicate.negated = context_.negated;
    context_.condition_stack_.push_back(std::move(predicate));
  } else {
    std::string msg =
        "Predicate \'" + ast_predicate.name->name + "\' not defined";
    throw ParserException(ast_predicate.name->location, msg);
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::Conjunction &conjunction) {
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
  context_.condition_stack_.push_back(std::move(junction));
  return true;
}

bool PddlAstParser::visit_begin(const ast::Disjunction &disjunction) {
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
  context_.condition_stack_.push_back(std::move(junction));
  return true;
}

bool PddlAstParser::visit_end(const ast::Condition &condition) {
  if (std::holds_alternative<std::unique_ptr<ast::Empty>>(condition) ||
      std::holds_alternative<std::unique_ptr<ast::Negation>>(condition)) {
    return true;
  }
  auto last_condition = context_.condition_stack_.back();
  context_.condition_stack_.pop_back();
  if (context_.condition_stack_.empty()) {
    switch (context_.state) {
    case State::Precondition:
      problem_->actions.back().precondition = std::move(last_condition);
      break;
    case State::Effect:
      problem_->actions.back().effect = std::move(last_condition);
      break;
    case State::Goal:
      problem_->goal = std::move(last_condition);
      break;
    case State::InitialState:
      problem_->init = std::move(last_condition);
      break;
    default:
      throw ParserException("Internal error occurred while parsing");
    }
  } else {
    if (auto junction =
            std::get_if<model::Junction>(&context_.condition_stack_.back())) {
      junction->arguments.push_back(std::move(last_condition));
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::Requirement &ast_requirement) {
  LOG_DEBUG(logger, "Visiting requirement '%s'", ast_requirement.name);
  header_.requirements.emplace_back(ast_requirement.name);
  return true;
}

} // namespace pddl
