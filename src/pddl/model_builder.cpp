#include "pddl/model_builder.hpp"
#include "lexer/location.hpp"
#include "model/parsed/model.hpp"
#include "model/parsed/model_exception.hpp"
#include "pddl/ast/ast.hpp"
#include "pddl/parser_exception.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace pddl {

void ModelBuilder::reset() {
  state_ = State::Header;
  positive_ = true;
  root_type_ = parsed::TypeHandle{};
  current_type_ = parsed::TypeHandle{};
  current_predicate_ = parsed::PredicateHandle{};
  current_action_ = parsed::ActionHandle{};
  condition_stack_.clear();
  problem_.reset();
}

std::unique_ptr<parsed::Problem> ModelBuilder::parse(const ast::AST &ast) {
  LOG_INFO(parser_logger, "Building model...");

  reset();

  problem_ = std::make_unique<parsed::Problem>();
  root_type_ = problem_->add_type("_root");
  auto equal_predicate = problem_->add_predicate("=");
  problem_->add_parameter_type(equal_predicate, root_type_);
  problem_->add_parameter_type(equal_predicate, root_type_);

  try {
    traverse(ast);
  } catch (const parsed::ModelException &e) {
    throw ParserException{*current_location_, "Error constructing the model: " +
                                                  std::string{e.what()}};
  }

  return std::move(problem_);
}

bool ModelBuilder::visit_begin(const ast::Domain &domain) {
  LOG_DEBUG(parser_logger, "Visiting domain '%s'", domain.name->name.c_str());
  problem_->set_domain_name(domain.name->name);
  return true;
}

bool ModelBuilder::visit_begin(const ast::Problem &problem) {
  LOG_DEBUG(parser_logger, "Visiting problem '%s' with domain reference '%s'",
            problem.name->name.c_str(), problem.domain_ref->name.c_str());
  problem_->set_problem_name(problem.name->name, problem.domain_ref->name);
  return true;
}

bool ModelBuilder::visit_begin(const ast::SingleTypeIdentifierList &list) {
  LOG_DEBUG(parser_logger, "Visiting identifier list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    current_type_ = problem_->get_type(list.type->name);
  } else {
    current_type_ = root_type_;
  }
  return true;
}

bool ModelBuilder::visit_end(const ast::SingleTypeIdentifierList &) {
  current_type_ = root_type_;
  return true;
}

bool ModelBuilder::visit_begin(const ast::SingleTypeVariableList &list) {
  LOG_DEBUG(parser_logger, "Visiting variable list of type '%s'",
            list.type ? list.type->name.c_str() : "_root");
  if (list.type) {
    current_type_ = problem_->get_type(list.type->name);
  } else {
    current_type_ = root_type_;
  }
  return true;
}

bool ModelBuilder::visit_end(const ast::SingleTypeVariableList &) {
  current_type_ = root_type_;
  return true;
}

bool ModelBuilder::visit_begin(const ast::IdentifierList &list) {
  switch (state_) {
  case State::Types:
    LOG_DEBUG(parser_logger, "Visiting identifier list as types");
    for (const auto &name : *list.elements) {
      problem_->add_type(name->name, current_type_);
    }
    break;
  case State::Constants:
    LOG_DEBUG(parser_logger, "Visiting identifier list as constants");
    for (const auto &name : *list.elements) {
      problem_->add_constant(name->name, current_type_);
    }
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool ModelBuilder::visit_begin(const ast::VariableList &list) {
  if (state_ != State::Predicates && state_ != State::Action) {
    throw ParserException("Internal error occurred while parsing");
  }
  LOG_DEBUG(parser_logger, "Visiting variable list as %s parameters",
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

bool ModelBuilder::visit_begin(const ast::ArgumentList &list) {
  LOG_DEBUG(parser_logger, "Visiting argument list");

  auto predicate = dynamic_cast<parsed::BaseAtomicCondition *>(
      condition_stack_.back().get());

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
        throw parsed::ModelException{
            "Bound arguments are only allowed within actions"};
      }
      predicate->add_bound_argument(
          current_action_->get_parameter((*variable)->name));
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool ModelBuilder::visit_begin(const ast::RequirementsDef &) {
  LOG_DEBUG(parser_logger, "Visiting requirements definition");
  state_ = State::Requirements;
  return true;
}

bool ModelBuilder::visit_begin(const ast::TypesDef &) {
  LOG_DEBUG(parser_logger, "Visiting types definition");
  state_ = State::Types;
  return true;
}

bool ModelBuilder::visit_begin(const ast::ConstantsDef &) {
  LOG_DEBUG(parser_logger, "Visiting constants definition");
  state_ = State::Constants;
  return true;
}

bool ModelBuilder::visit_begin(const ast::PredicatesDef &) {
  LOG_DEBUG(parser_logger, "Visiting predicates definition");
  state_ = State::Predicates;
  return true;
}

bool ModelBuilder::visit_begin(const ast::ActionDef &action_def) {
  LOG_DEBUG(parser_logger, "Visiting action definition");
  state_ = State::Action;
  current_action_ = problem_->add_action(action_def.name->name);
  return true;
}

bool ModelBuilder::visit_end(const ast::ActionDef &) {
  current_action_ = parsed::ActionHandle{};
  return true;
}

bool ModelBuilder::visit_begin(const ast::ObjectsDef &) {
  LOG_DEBUG(parser_logger, "Visiting objects definition");
  state_ = State::Constants;
  return true;
}

bool ModelBuilder::visit_begin(const ast::InitDef &) {
  LOG_DEBUG(parser_logger, "Visiting init definition");
  state_ = State::Init;
  return true;
}

bool ModelBuilder::visit_begin(const ast::GoalDef &) {
  LOG_DEBUG(parser_logger, "Visiting goal definition");
  state_ = State::Goal;
  return true;
}

bool ModelBuilder::visit_begin(const ast::Precondition &) {
  state_ = State::Precondition;
  return true;
}

bool ModelBuilder::visit_begin(const ast::Effect &) {
  state_ = State::Effect;
  return true;
}

bool ModelBuilder::visit_begin(const ast::Negation &negation) {
  if (!positive_ && state_ != State::Precondition) {
    throw ParserException(negation.location,
                          "Nested negation is only allowed in preconditions");
  }
  positive_ = !positive_;
  return true;
}

bool ModelBuilder::visit_end(const ast::Negation &) {
  positive_ = !positive_;
  return true;
}

bool ModelBuilder::visit_begin(const ast::Predicate &predicate) {
  current_predicate_ = problem_->add_predicate(predicate.name->name);
  return true;
}

bool ModelBuilder::visit_end(const ast::Predicate &) {
  current_predicate_ = parsed::PredicateHandle{};
  return true;
}

bool ModelBuilder::visit_begin(const ast::PredicateEvaluation &predicate) {
  auto definition = problem_->get_predicate(predicate.name->name);
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(std::make_shared<parsed::AtomicCondition<
                                   parsed::ConditionContextType::Precondition>>(
        positive_, definition, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<
            parsed::AtomicCondition<parsed::ConditionContextType::Effect>>(
            positive_, definition, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<parsed::FreePredicate>(positive_, definition));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool ModelBuilder::visit_begin(const ast::Conjunction &) {
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(
        std::make_shared<
            parsed::Junction<parsed::ConditionContextType::Precondition>>(
            parsed::JunctionOperator::And, positive_, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<
            parsed::Junction<parsed::ConditionContextType::Effect>>(
            parsed::JunctionOperator::And, positive_, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<parsed::Junction<parsed::ConditionContextType::Free>>(
            parsed::JunctionOperator::And, positive_, problem_.get()));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool ModelBuilder::visit_begin(const ast::Disjunction &) {
  switch (state_) {
  case State::Precondition:
    condition_stack_.push_back(
        std::make_shared<
            parsed::Junction<parsed::ConditionContextType::Precondition>>(
            parsed::JunctionOperator::Or, positive_, current_action_));
    break;
  case State::Effect:
    condition_stack_.push_back(
        std::make_shared<
            parsed::Junction<parsed::ConditionContextType::Effect>>(
            parsed::JunctionOperator::Or, positive_, current_action_));
    break;
  case State::Init: // Fallthrough
  case State::Goal:
    condition_stack_.push_back(
        std::make_shared<parsed::Junction<parsed::ConditionContextType::Free>>(
            parsed::JunctionOperator::Or, positive_, problem_.get()));
    break;
  default:
    throw ParserException("Internal error occurred while parsing");
  }
  return true;
}

bool ModelBuilder::visit_end(const ast::Condition &condition) {
  if (std::holds_alternative<std::unique_ptr<ast::Empty>>(condition) ||
      std::holds_alternative<std::unique_ptr<ast::Negation>>(condition)) {
    return true;
  }
  auto last_condition = condition_stack_.back();
  condition_stack_.pop_back();
  if (condition_stack_.empty()) {
    switch (state_) {
    case State::Precondition:
      if (auto p =
              std::dynamic_pointer_cast<parsed::Precondition>(last_condition)) {
        problem_->set_precondition(current_action_, p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Effect:
      if (auto p = std::dynamic_pointer_cast<parsed::Effect>(last_condition)) {
        problem_->set_effect(current_action_, p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Goal:
      if (auto p = std::dynamic_pointer_cast<parsed::GoalCondition>(
              last_condition)) {
        problem_->set_goal(p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    case State::Init:
      if (auto p = std::dynamic_pointer_cast<parsed::FreePredicate>(
              last_condition)) {
        problem_->add_init(p);
      } else {
        throw ParserException("Internal error occurred while parsing");
      }
      break;
    default:
      throw ParserException("Internal error occurred while parsing");
    }
  } else {
    if (auto junction = std::dynamic_pointer_cast<parsed::BaseJunction>(
            condition_stack_.back())) {
      junction->add_condition(last_condition);
    } else {
      throw ParserException("Internal error occurred while parsing");
    }
  }
  return true;
}

bool ModelBuilder::visit_begin(const ast::Requirement &requirement) {
  LOG_DEBUG(parser_logger, "Visiting requirement '%s'",
            requirement.name.c_str());
  problem_->add_requirement(requirement.name);
  return true;
}

} // namespace pddl
