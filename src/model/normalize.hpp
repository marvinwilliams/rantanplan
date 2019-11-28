#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "logging/logging.hpp"
#include "model/normalized_problem.hpp"
#include "model/problem.hpp"

#include <vector>

extern logging::Logger normalize_logger;

normalized::Condition
normalize_atomic_condition(const BaseAtomicCondition &condition) noexcept;

Condition normalize_condition(const Condition &condition) noexcept;

std::vector<std::shared_ptr<BaseAtomicCondition>>
to_list(const Condition &condition) noexcept;

std::vector<normalized::Action> normalize_action(const Action &action) noexcept;

normalized::Problem normalize(const Problem &problem) noexcept;

#endif /* end of include guard: NORMALIZE_HPP */
