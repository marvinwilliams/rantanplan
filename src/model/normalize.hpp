#ifndef NORMALIZE_HPP
#define NORMALIZE_HPP

#include "logging/logging.hpp"
#include "model/model.hpp"

#include <vector>

namespace model {

extern logging::Logger normalize_logger;

Condition normalize_condition(const Condition &condition) ;

std::vector<PredicateEvaluation> to_list(const Condition &condition) ;

std::vector<Action> normalize_action(const AbstractAction &action) ;

Problem normalize(const AbstractProblem &abstract_problem) ;

} // namespace model

#endif /* end of include guard: NORMALIZE_HPP */
