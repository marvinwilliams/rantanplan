#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"

#include <vector>

namespace model {

extern logging::Logger normalize_logger;

Condition normalize_condition(const Condition &condition) noexcept;

std::vector<PredicateEvaluation> to_list(const Condition &condition) noexcept;

std::vector<Action> normalize_action(const AbstractAction &action) noexcept;

Problem normalize(const AbstractProblem &abstract_problem) noexcept;

} // namespace model

#endif /* end of include guard: NORMALIZE_HPP */
