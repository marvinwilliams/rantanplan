#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "model/model.hpp"

#include <string>

namespace model {

std::string to_string(const Type &type, const ProblemBase &problem);
std::string to_string(const Constant &constant, const ProblemBase &problem);
std::string to_string(const PredicateDefinition &predicate,
                      const ProblemBase &problem);
std::string to_string(const PredicateEvaluation &predicate,
                      const AbstractAction &action, const ProblemBase &problem);
std::string to_string(const PredicateEvaluation &predicate,
                      const Action &action, const ProblemBase &problem);
std::string to_string(const PredicateEvaluation &predicate,
                      const ProblemBase &problem);
std::string to_string(const GroundPredicate &predicate,
                      const ProblemBase &problem);
std::string to_string(const Condition &condition, const AbstractAction &action,
                      const ProblemBase &problem);
std::string to_string(const Condition &condition, const ProblemBase &problem);
std::string to_string(const AbstractAction &action, const ProblemBase &problem);
std::string to_string(const Action &action, const ProblemBase &problem);
std::string to_string(const AbstractProblem &problem);
std::string to_string(const Problem &problem);

} // namespace model

#endif /* end of include guard: TO_STRING_HPP */
