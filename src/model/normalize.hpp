#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "logging/logging.hpp"
#include "model/normalized/model.hpp"
#include "model/parsed/model.hpp"

#include <memory>
#include <vector>

extern logging::Logger normalize_logger;

normalized::Condition
normalize_atomic_condition(const parsed::BaseAtomicCondition &condition,
                           const parsed::Problem &problem) noexcept;

normalized::Condition
normalize_atomic_condition(const parsed::BaseAtomicCondition &condition,
                           const parsed::Action &action,
                           const parsed::Problem &problem) noexcept;

std::vector<std::shared_ptr<parsed::BaseAtomicCondition>>
to_list(const parsed::Condition &condition) noexcept;

std::vector<normalized::Action>
normalize_action(const parsed::Action &action,
                 const parsed::Problem &problem) noexcept;

std::shared_ptr<normalized::Problem> normalize(const parsed::Problem &problem);

#endif /* end of include guard: NORMALIZE_HPP */
