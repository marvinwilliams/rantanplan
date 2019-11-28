#include "model/support.hpp"
#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/to_string.hpp"
#include "model/utils.hpp"
#include "util/combinatorics.hpp"

#include <algorithm>
#include <cassert>
#include <variant>

using namespace normalized;

logging::Logger Support::logger{"Support"};

Support::Support(const Problem &problem_) noexcept
    : constants_by_type_(problem_.types.size()), problem_{problem_} {
  LOG_INFO(logger, "Sorting constants by type...");
  for (size_t i = 0; i < problem_.constants.size(); ++i) {
    const auto &c = problem_.constants[i];
    auto t = c.type;
    constants_by_type_[t].push_back(ConstantHandle{i});
    while (problem_.types[t].parent != t) {
      t = problem_.types[t].parent;
      constants_by_type_[t].push_back(ConstantHandle{i});
    }
  }

  LOG_INFO(logger, "Grounding all predicates...");
  instantiate_predicates();
  instantiated_ = true;
  LOG_INFO(logger, "The problem has %u grounded predicates",
           instantiations_.size());
  init_.reserve(problem_.init.size());
  for (const auto &predicate : problem_.init) {
    assert(instantiations_.find(predicate) != instantiations_.end());
    init_.insert(instantiations_[predicate]);
  }
  LOG_INFO(logger, "Computing predicate support...");
  set_predicate_support();
  predicate_support_constructed_ = true;
  /* for (size_t i = 0; i < problem_.predicates.size(); ++i) { */
  /*   LOG_DEBUG(logger, "%s %s\n", */
  /*             model::to_string(problem_.predicates[i], problem_).c_str(), */
  /*             is_rigid(i) ? "rigid" : "not rigid"); */
  /* } */
  /* LOG_DEBUG(logger, ""); */
  /* if constexpr (logging::has_debug_log()) { */
  /*   for (const auto &predicate : instantiations_) { */
  /*     LOG_DEBUG(logger, "%s %s\n", */
  /*               to_string(predicate.first, problem_).c_str(), */
  /*               is_rigid(predicate.first, false) ? "rigid" : "not rigid"); */
  /*     LOG_DEBUG(logger, "!%s %s\n", */
  /*               to_string(predicate.first, problem_).c_str(), */
  /*               is_rigid(predicate.first, true) ? "rigid" : "not rigid"); */
  /*   } */
  /* } */
}

void Support::instantiate_predicates() noexcept {
  /* std::vector<PredicateInstantiation> tmp; */
  instantiations_.reserve(1000000);
  for (size_t i = 0; i < problem_.predicates.size(); ++i) {
    LOG_DEBUG(logger, "Grounding %s",
              to_string(normalized::PredicateHandle{i}, problem_).c_str());
    for_each_instantiation(
        problem_.predicates[i],
        [this, i](std::vector<ConstantHandle> arguments) {
      instantiations_.insert(
          {PredicateInstantiation{normalized::PredicateHandle{i},
                                  std::move(arguments)},
           InstantiationHandle{instantiations_.size()}});
      /* tmp.push_back(PredicateInstantiation{normalized::PredicateHandle{i}, */
      /*                                      std::move(arguments)}); */
        },
        constants_by_type_);
  }
}

void Support::set_predicate_support() noexcept {
  pos_precondition_support_.resize(instantiations_.size());
  neg_precondition_support_.resize(instantiations_.size());
  pos_effect_support_.resize(instantiations_.size());
  neg_effect_support_.resize(instantiations_.size());

  for (size_t i = 0; i < problem_.actions.size(); ++i) {
    const auto &action = problem_.actions[i];
    for (const auto &[predicate, positive] : action.pre_instantiated) {
      auto &predicate_support =
          positive ? pos_precondition_support_ : neg_precondition_support_;
      predicate_support[instantiations_.at(predicate)][ActionHandle{i}]
          .emplace_back();
    }

    for (const auto &predicate : action.preconditions) {
      for_each_assignment(
          predicate, action.parameters,
          [&](ParameterAssignment assignment) {
            auto &predicate_support = select_support(predicate.positive, false);
            auto parameters = action.parameters;
            for (const auto &[p, c] : assignment) {
              parameters[p].set(c);
            }
            auto instantiation = instantiate(predicate, parameters);
            predicate_support[instantiations_.at(instantiation)]
                             [ActionHandle{i}]
                                 .push_back(std::move(assignment));
          },
          constants_by_type_);
    }
    for (const auto &[predicate, positive] : action.eff_instantiated) {
      auto &predicate_support =
          positive ? pos_effect_support_ : neg_effect_support_;
      predicate_support[instantiations_.at(predicate)][ActionHandle{i}]
          .emplace_back();
    }
    for (const auto &predicate : action.effects) {
      for_each_assignment(
          predicate, action.parameters,
          [&](ParameterAssignment assignment) {
            auto &predicate_support = select_support(predicate.positive, true);
            auto parameters = action.parameters;
            for (const auto &[p, c] : assignment) {
              parameters[p].set(c);
            }
            auto instantiation = instantiate(predicate, parameters);
            predicate_support[instantiations_[instantiation]][ActionHandle{i}]
                .push_back(std::move(assignment));
          },
          constants_by_type_);
    }
  }

  for (const auto &[instantiation, handle] : instantiations_) {
    if (neg_effect_support_[handle].empty() && is_init(handle)) {
      pos_rigid_predicates_.insert(handle);
    }
    if (pos_effect_support_[handle].empty() && !is_init(handle)) {
      neg_rigid_predicates_.insert(handle);
    }
  }
}
