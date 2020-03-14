#include "encoder/exists_encoder.hpp"
#include "config.hpp"
#include "encoder/encoder.hpp"
#include "encoder/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "planner/planner.hpp"
#include "sat/formula.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

using namespace normalized;

ExistsEncoder::ExistsEncoder(const std::shared_ptr<Problem> &problem,
                             util::Seconds timeout = util::inf_time)
    : Encoder{problem, timeout}, support_{*problem, timeout} {
  LOG_INFO(encoding_logger, "Init sat variables...");
  init_sat_vars();

  LOG_INFO(encoding_logger, "Encode problem...");
  encode_init();
  encode_actions();
  parameter_implies_predicate();
  interference();
  frame_axioms();
  assume_goal();
  num_vars_ -= 3; // subtract SAT und UNSAT for correct step semantics

  LOG_INFO(encoding_logger, "Variables per step: %lu", num_vars_);
  LOG_INFO(encoding_logger, "Implication chain variables: %lu",
           std::accumulate(
               pos_helpers_.begin(), pos_helpers_.end(), 0ul,
               [](size_t sum, const auto &m) { return sum + m.size(); }) +
               std::accumulate(
                   neg_helpers_.begin(), neg_helpers_.end(), 0ul,
                   [](size_t sum, const auto &m) { return sum + m.size(); }));
  LOG_INFO(encoding_logger, "Helper variables to mitigate dnf explosion: %lu",
           std::accumulate(
               dnf_helpers_.begin(), dnf_helpers_.end(), 0ul,
               [](size_t sum, const auto &m) { return sum + m.size(); }));
  LOG_INFO(encoding_logger, "Init clauses: %lu", init_.clauses.size());
  LOG_INFO(encoding_logger, "Universal clauses: %lu",
           universal_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Transition clauses: %lu",
           transition_clauses_.clauses.size());
  LOG_INFO(encoding_logger, "Goal clauses: %lu", goal_.clauses.size());
}

int ExistsEncoder::to_sat_var(Literal l, unsigned int step) const noexcept {
  uint_fast64_t variable = 0;
  variable = l.variable.sat_var;
  if (variable == DONTCARE) {
    return static_cast<int>(SAT);
  }
  if (variable == SAT || variable == UNSAT) {
    return (l.positive ? 1 : -1) * static_cast<int>(variable);
  }
  step += l.variable.this_step ? 0 : 1;
  return (l.positive ? 1 : -1) * static_cast<int>(variable + step * num_vars_);
}

Plan ExistsEncoder::extract_plan(const sat::Model &model,
                                 unsigned int step) const noexcept {
  Plan plan;
  plan.problem = problem_;
  for (unsigned int s = 0; s < step; ++s) {
    for (size_t i = 0; i < problem_->actions.size(); ++i) {
      if (model[actions_[i] + s * num_vars_]) {
        const Action &action = problem_->actions[i];
        std::vector<ConstantIndex> constants;
        for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
             ++parameter_pos) {
          auto &parameter = action.parameters[parameter_pos];
          if (!parameter.is_free()) {
            constants.push_back(parameter.get_constant());
          } else {
            for (size_t j = 0;
                 j < problem_->constants_of_type[parameter.get_type()].size();
                 ++j) {
              if (model[parameters_[i][parameter_pos][j] + s * num_vars_]) {
                constants.push_back(
                    problem_->constants_of_type[parameter.get_type()][j]);
                break;
              }
            }
          }
          assert(constants.size() == parameter_pos + 1);
        }
        plan.sequence.emplace_back(ActionIndex{i}, std::move(constants));
      }
    }
  }
  return plan;
}

void ExistsEncoder::init_sat_vars() {
  actions_.reserve(problem_->actions.size());
  parameters_.resize(problem_->actions.size());
  action_rank_.reserve(problem_->actions.size());
  pos_helpers_.resize(support_.get_num_ground_atoms());
  neg_helpers_.resize(support_.get_num_ground_atoms());
  dnf_helpers_.resize(problem_->actions.size());

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    action_rank_.push_back(i);
  }

  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    actions_.push_back(num_vars_++);

    parameters_[i].resize(action.parameters.size());

    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (!parameter.is_free()) {
        continue;
      }
      parameters_[i][parameter_pos].reserve(
          problem_->constants_of_type[parameter.get_type()].size());
      for (size_t j = 0;
           j < problem_->constants_of_type[parameter.get_type()].size(); ++j) {
        parameters_[i][parameter_pos].push_back(num_vars_++);
      }
    }
  }

  predicates_.resize(support_.get_num_ground_atoms());
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (support_.is_rigid(Support::PredicateId{i}, true)) {
      predicates_[i] = SAT;
    } else if (support_.is_rigid(Support::PredicateId{i}, false)) {
      predicates_[i] = UNSAT;
    } else {
      predicates_[i] = num_vars_++;
    }
  }

  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    for (const auto &[action_index, assignment] :
         support_.get_support(Support::PredicateId{i}, true, false)) {
      if (auto [it, success] = pos_helpers_[i].emplace(action_index, num_vars_);
          success) {
        ++num_vars_;
      }
    }
    for (const auto &[action_index, assignment] :
         support_.get_support(Support::PredicateId{i}, false, false)) {
      if (auto [it, success] = neg_helpers_[i].emplace(action_index, num_vars_);
          success) {
        ++num_vars_;
      }
    }
  }
}

size_t ExistsEncoder::get_constant_index(ConstantIndex constant,
                                                TypeIndex type) const noexcept {
  auto it = problem_->constant_type_map[type].find(constant);
  if (it != problem_->constant_type_map[type].end()) {
    return it->second;
  }
  assert(false);
  return problem_->constants.size();
}

void ExistsEncoder::encode_init() {
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    auto literal = Literal{Variable{predicates_[i]},
                           support_.is_init(Support::PredicateId{i})};
    init_ << literal << sat::EndClause;
  }
}

void ExistsEncoder::encode_actions() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    const auto &action = problem_->actions[i];
    auto action_var = Variable{actions_[i]};
    for (size_t parameter_pos = 0; parameter_pos < action.parameters.size();
         ++parameter_pos) {
      const auto &parameter = action.parameters[parameter_pos];
      if (!parameter.is_free()) {
        continue;
      }
      size_t number_arguments =
          problem_->constants_of_type[parameter.get_type()].size();
      std::vector<Variable> all_arguments;
      all_arguments.reserve(number_arguments);
      universal_clauses_ << Literal{action_var, false};
      for (size_t constant_index = 0; constant_index < number_arguments;
           ++constant_index) {
        auto argument = Variable{parameters_[i][parameter_pos][constant_index]};
        universal_clauses_ << Literal{argument, true};
        all_arguments.push_back(argument);
      }
      universal_clauses_ << sat::EndClause;
      ++clause_count;
      clause_count += universal_clauses_.at_most_one(all_arguments);
      if (config.parameter_implies_action) {
        for (auto argument : all_arguments) {
          universal_clauses_ << Literal{argument, false};
          universal_clauses_ << Literal{action_var, true};
          universal_clauses_ << sat::EndClause;
        }
        clause_count += all_arguments.size();
      }
    }
  }
  LOG_INFO(encoding_logger, "Action clauses: %lu", clause_count);
}

void ExistsEncoder::parameter_implies_predicate() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (check_timeout()) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      for (bool is_effect : {true, false}) {
        auto &formula = is_effect ? transition_clauses_ : universal_clauses_;
        for (const auto &[action_index, assignment] : support_.get_support(
                 Support::PredicateId{i}, positive, is_effect)) {
          if (!config.parameter_implies_action || assignment.empty()) {
            formula << Literal{Variable{actions_[action_index]}, false};
          }
          for (const auto &[parameter_index, constant] : assignment) {
            auto index =
                get_constant_index(constant, problem_->actions[action_index]
                                                 .parameters[parameter_index]
                                                 .get_type());
            formula << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
                false};
          }
          formula << Literal{Variable{predicates_[i], !is_effect}, positive}
                  << sat::EndClause;
          ++clause_count;
        }
      }
    }
  }
  LOG_INFO(encoding_logger, "Implication clauses: %lu", clause_count);
}

void ExistsEncoder::interference() {
  uint_fast64_t clause_count = 0;
  std::vector<ActionIndex> action_order(problem_->actions.size());
  for (size_t i = 0; i < problem_->actions.size(); ++i) {
    action_order[action_rank_[i]] = ActionIndex{i};
  }
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (check_timeout()) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      const auto &helpers = positive ? pos_helpers_ : neg_helpers_;
      auto chain_first = helpers[i].end();
      for (size_t j = 0; j + 1 < action_order.size(); ++j) {
        chain_first = helpers[i].find(action_order[j]);
        if (chain_first != helpers[i].end()) {
          break;
        }
      }
      while (chain_first != helpers[i].end()) {
        auto chain_next = helpers[i].end();
        for (size_t j = action_rank_[chain_first->first] + 1;
             j < action_order.size(); ++j) {
          chain_next = helpers[i].find(action_order[j]);
          if (chain_next != helpers[i].end()) {
            universal_clauses_ << Literal{Variable{chain_first->second}, false};
            universal_clauses_ << Literal{Variable{chain_next->second}, true};
            universal_clauses_ << sat::EndClause;
            ++clause_count;
            break;
          }
        }
        chain_first = chain_next;
      }
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, false)) {
        assert(helpers[i].find(action_index) != helpers[i].end());
        universal_clauses_ << Literal{
            Variable{helpers[i].find(action_index)->second}, false};
        if (!config.parameter_implies_action || assignment.empty()) {
          universal_clauses_
              << Literal{Variable{actions_[action_index]}, false};
        }
        for (const auto &[parameter_index, constant] : assignment) {
          auto index =
              get_constant_index(constant, problem_->actions[action_index]
                                               .parameters[parameter_index]
                                               .get_type());
          universal_clauses_ << Literal{
              Variable{parameters_[action_index][parameter_index][index]},
              false};
        }
        universal_clauses_ << sat::EndClause;
        ++clause_count;
      }
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, !positive, true)) {
        for (auto j = action_rank_[action_index] + 1; j < action_order.size();
             ++j) {
          auto next_helper = helpers[i].find(action_order[j]);
          if (next_helper != helpers[i].end()) {
            if (!config.parameter_implies_action || assignment.empty()) {
              universal_clauses_
                  << Literal{Variable{actions_[action_index]}, false};
            }
            for (const auto &[parameter_index, constant] : assignment) {
              auto index =
                  get_constant_index(constant, problem_->actions[action_index]
                                                   .parameters[parameter_index]
                                                   .get_type());
              universal_clauses_ << Literal{
                  Variable{parameters_[action_index][parameter_index][index]},
                  false};
            }
            universal_clauses_ << Literal{Variable{next_helper->second}, true};
            universal_clauses_ << sat::EndClause;
            ++clause_count;
            break;
          }
        }
      }
    }
  }
  LOG_INFO(encoding_logger, "Interference clauses: %lu", clause_count);
}

void ExistsEncoder::frame_axioms() {
  uint_fast64_t clause_count = 0;
  for (size_t i = 0; i < support_.get_num_ground_atoms(); ++i) {
    if (check_timeout()) {
      throw TimeoutException{};
    }
    for (bool positive : {true, false}) {
      bool use_helper = false;
      if (config.dnf_threshold > 0) {
        size_t num_nontrivial_clauses = 0;
        for (const auto &[action_index, assignment] :
             support_.get_support(Support::PredicateId{i}, positive, true)) {
          // Assignments with multiple arguments lead to combinatorial explosion
          if (assignment.size() > (config.parameter_implies_action ? 1 : 0)) {
            ++num_nontrivial_clauses;
          }
        }
        use_helper = num_nontrivial_clauses >= config.dnf_threshold;
      }
      Formula dnf;
      dnf << Literal{Variable{predicates_[i], true}, positive}
          << sat::EndClause;
      dnf << Literal{Variable{predicates_[i], false}, !positive}
          << sat::EndClause;
      for (const auto &[action_index, assignment] :
           support_.get_support(Support::PredicateId{i}, positive, true)) {
        if (use_helper &&
            assignment.size() > (config.parameter_implies_action ? 1 : 0)) {
          auto [it, success] =
              dnf_helpers_[action_index].try_emplace(assignment, num_vars_);
          if (success) {
            if (!config.parameter_implies_action) {
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_
                  << Literal{Variable{actions_[action_index]}, true};
              universal_clauses_ << sat::EndClause;
              ++clause_count;
            }
            for (const auto &[parameter_index, constant] : assignment) {
              auto index =
                  get_constant_index(constant, problem_->actions[action_index]
                                                   .parameters[parameter_index]
                                                   .get_type());
              universal_clauses_ << Literal{Variable{it->second}, false};
              universal_clauses_ << Literal{
                  Variable{parameters_[action_index][parameter_index][index]},
                  true};
              universal_clauses_ << sat::EndClause;
            }
            ++num_vars_;
            clause_count += assignment.size();
          }
          dnf << Literal{Variable{it->second}, true};
        } else {
          if (!config.parameter_implies_action || assignment.empty()) {
            dnf << Literal{Variable{actions_[action_index]}, true};
          }
          for (const auto &[parameter_index, constant] : assignment) {
            auto index =
                get_constant_index(constant, problem_->actions[action_index]
                                                 .parameters[parameter_index]
                                                 .get_type());
            dnf << Literal{
                Variable{parameters_[action_index][parameter_index][index]},
                true};
          }
        }
        dnf << sat::EndClause;
      }
      clause_count += transition_clauses_.add_dnf(dnf);
    }
  }
  LOG_INFO(encoding_logger, "Frame axiom clauses: %lu", clause_count);
}

void ExistsEncoder::assume_goal() {
  for (const auto &[goal, positive] : problem_->goal) {
    goal_ << Literal{Variable{predicates_[support_.get_id(goal)]}, positive}
          << sat::EndClause;
  }
}
