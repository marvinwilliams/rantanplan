#include "pddl/pddl_visitor.hpp"
/* #include "model/model.hpp" */
/* #include "model/model_utils.hpp" */
#include "lexer/location.hpp"
#include "model/problem.hpp"
#include "pddl/ast.hpp"
#include "pddl/parser_exception.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace pddl {

void PddlAstParser::reset() {
  state_ = State::Header;
  positive_ = true;
  root_type_ = Handle<Type>{};
  current_type_ = Handle<Type>{};
  current_predicate_ = Handle<Predicate>{};
  current_action_ = Handle<Action>{};
  condition_stack_.clear();
  num_requirements_ = 0;
  num_types_ = 0;
  num_constants_ = 0;
  num_predicates_ = 0;
  num_actions_ = 0;
  problem_.reset();
}

std::unique_ptr<Problem> PddlAstParser::parse(const AST &ast) {
  LOG_INFO(logger, "Traverse AST");

  reset();

  problem_ = std::make_unique<Problem>();
  root_type_ = problem_->add_type("_root");
  auto equal_predicate = problem_->add_predicate("=");
  problem_->add_parameter_type(equal_predicate, root_type_);
  problem_->add_parameter_type(equal_predicate, root_type_);

  try {
    traverse(ast);
  } catch (const ModelException &e) {
    throw ParserException{*current_location_, "Error constructing the model: " +
                                                  std::string{e.what()}};
  }

  LOG_INFO(logger,
           "Problem has\n%lu requirements\n%lu types\n%lu constants\n%lu "
           "predicates\n%lu actions",
           num_requirements_, num_types_, num_constants_, num_predicates_,
           num_actions_);

  return std::move(problem_);
}

bool PddlAstParser::visit_begin(const ast::Domain &domain) {
  LOG_DEBUG(logger, "Visiting domain '%s'", domain.name->name.c_str());
  problem_->set_domain_name(domain.name->name);
  return true;
}

bool PddlAstParser::visit_begin(const ast::Problem &problem) {
  LOG_DEBUG(logger, "Visiting problem '%s' with domain reference '%s'",
            problem.name->name.c_str(), problem.domain_ref->name.c_str());
  problem_->set_problem_name(problem.name->name, problem.domain_ref->name);
  return true;
}

bool PddlAstParser::visit_begin(const ast::SingleTypeIdentifierList &list) {
  LOG_DEBUG(logger, "Visiting identifier list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    current_type_ = problem_->get_type(list.type->name);
  } else {
    current_type_ = root_type_;
  }
  return true;
}

bool PddlAstParser::visit_end(const ast::SingleTypeIdentifierList &) {
  current_type_ = root_type_;
  return true;
}

bool PddlAstParser::visit_begin(const ast::SingleTypeVariableList &list) {
  LOG_DEBUG(logger, "Visiting variable list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    current_type_ = problem_->get_type(list.type->name);
  } else {
    current_type_ = root_type_;
  }
  return true;
}

bool PddlAstParser::visit_end(const ast::SingleTypeVariableList &) {
  current_type_ = root_type_;
  return true;
}

bool PddlAstParser::visit_begin(const ast::IdentifierList &list) {
  switch (state_) {
  case State::Types:
    LOG_DEBUG(logger, "Visiting identifier list as types");
    for (const auto &name : *list.elements) {
      problem_->add_type(name->name, current_type_);
    }
    break;
  case State::Constants:
    LOG_DEBUG(logger, "Visiting identifier list as constants");
    for (const auto &name : *list.elements) {
      problem_->add_constant(name->name, current_type_);
    }
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::VariableList &list) {
  if (state_ != State::Predicates && state_ != State::Action) {
    throw ParserException("Internal error occurred while parsing");
  }
  LOG_DEBUG(logger, "Visiting variable list as %s parameters",
            state_ == State::Predicates ? "predicate" : "action");
  for (const auto &variable : *list.elements) {
    if (state_ == State::Predicates) {
      problem_->add_parameter_type(current_predicate_, current_type_);
    } else {
      problem_->add_parameter(current_action_, variable->name, current_type_);
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::ArgumentList &list) {
  LOG_DEBUG(logger, "Visiting argument list");

  auto predicate =
      dynamic_cast<BaseAtomicCondition *>(condition_stack_.back().get());

  if (!predicate) {
    throw ParserException("Internal error occurred while parsing");
  }

  for (size_t i = 0; i < list.elements->size(); ++i) {
    const auto &argument = (*list.elements)[i];
    if (auto identifier =
            std::get_if<std::unique_ptr<ast::Identifier>>(&argument)) {
      predicate->add_constant_argument(
          problem_->get_constant((*identifier)->name));
    } else if (auto variable =
                   std::get_if<std::unique_ptr<ast::Variable>>(&argument)) {
      if (state_ != State::Precondition && state_ != State::Effect) {
        throw ModelException{"Bound arguments are only allowed within actions"};
      }
      predicate->add_bound_argument(
          current_action_->get_parameter((*variable)->name));
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::RequirementsDef &) {
  LOG_DEBUG(logger, "Visiting requirements definition");
  state_ = State::Requirements;
  return true;
}

bool PddlAstParser::visit_begin(const ast::TypesDef &) {
  LOG_DEBUG(logger, "Visiting types definition");
  state_ = State::Types;
  return true;
}

bool PddlAstParser::visit_begin(const ast::ConstantsDef &) {
  LOG_DEBUG(logger, "Visiting constants definition");
  state_ = State::Constants;
  return true;
}

bool PddlAstParser::visit_begin(const ast::PredicatesDef &) {
  LOG_DEBUG(logger, "Visiting predicates definition");
  state_ = State::Predicates;
  return true;
}

bool PddlAstParser::visit_begin(const ast::ActionDef &action_def) {
  LOG_DEBUG(logger, "Visiting action definition");
  state_ = State::Action;
  current_action_ = problem_->add_action(action_def.name->name);
  return true;
}

bool PddlAstParser::visit_end(const ast::ActionDef &) {
  current_action_ = Handle<Action>{};
  return true;
}

bool PddlAstParser::visit_begin(const ast::ObjectsDef &) {
  LOG_DEBUG(logger, "Visiting objects definition");
  state_ = State::Constants;
  return true;
}

bool PddlAstParser::visit_begin(const ast::InitDef &) {
  LOG_DEBUG(logger, "Visiting init definition");
  state_ = State::Init;
  return true;
}

bool PddlAstParser::visit_begin(const ast::GoalDef &) {
  LOG_DEBUG(logger, "Visiting goal definition");
  state_ = State::Goal;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Precondition &) {
  state_ = State::Precondition;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Effect &) {
  state_ = State::Effect;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Negation &negation) {
  if (!positive_ && state_ != State::Precondition) {
    throw ParserException(negation.location,
                          "Nested negation is only allowed in preconditions");
  }
  positive_ = !positive_;
  return true;
}

bool PddlAstParser::visit_end(const ast::Negation &) {
  positive_ = !positive_;
  return true;
}

bool PddlAstParser::visit_begin(const ast::Predicate &ast_predicate) {
  current_predicate_ = problem_->add_predicate(ast_predicate.name->name);
  return true;
}

bool PddlAstParser::visit_end(const ast::Predicate &) {
  current_predicate_ = Handle<Predicate>{};
  return true;
}

bool PddlAstParser::visit_begin(const ast::PredicateEvaluation &ast_predicate) {
  auto predicate = problem_->get_predicate(ast_predicate.name->name);
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(
        std::make_shared<AtomicCondition<ConditionContextType::Precondition>>(
            positive_, predicate, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<AtomicCondition<ConditionContextType::Effect>>(
            positive_, predicate, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<FreePredicate>(positive_, predicate));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::Conjunction &) {
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Precondition>>(
            JunctionOperator::And, positive_, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Effect>>(
            JunctionOperator::And, positive_, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Free>>(
            JunctionOperator::And, positive_, problem_.get()));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::Disjunction &) {
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Precondition>>(
            JunctionOperator::Or, positive_, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Effect>>(
            JunctionOperator::Or, positive_, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<Junction<ConditionContextType::Free>>(
            JunctionOperator::Or, positive_, problem_.get()));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool PddlAstParser::visit_end(const ast::Condition &condition) {
  if (std::holds_alternative<std::unique_ptr<ast::Empty>>(condition) ||
      std::holds_alternative<std::unique_ptr<ast::Negation>>(condition)) {
    return true;
  }
  auto last_condition = condition_stack_.back();
  condition_stack_.pop_back();
  if (condition_stack_.empty()) {
    switch (state_) {
    case State::Precondition:
      if (auto p = std::dynamic_pointer_cast<Precondition>(last_condition)) {
        problem_->set_precondition(current_action_, p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Effect:
      if (auto p = std::dynamic_pointer_cast<Effect>(last_condition)) {
        problem_->set_effect(current_action_, p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Goal:
      if (auto p = std::dynamic_pointer_cast<GoalCondition>(last_condition)) {
        problem_->set_goal(p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Init:
      if (auto p = std::dynamic_pointer_cast<FreePredicate>(last_condition)) {
        problem_->add_init(p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    default:
      throw ParserException("Internal error occurred while parsing");
    }
  } else {
    if (auto junction =
            std::dynamic_pointer_cast<BaseJunction>(condition_stack_.back())) {
      junction->add_condition(last_condition);
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool PddlAstParser::visit_begin(const ast::Requirement &ast_requirement) {
  LOG_DEBUG(logger, "Visiting requirement '%s'", ast_requirement.name);
  problem_->add_requirement(ast_requirement.name);
  return true;
}

} // namespace pddl