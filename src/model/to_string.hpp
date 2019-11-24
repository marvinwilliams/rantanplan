#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "model/problem.hpp"
#include "model/normalized_problem.hpp"

#include <string>

std::string to_string(normalized::TypeHandle type,
                      const normalized::Problem &problem);
std::string to_string(normalized::ConstantHandle constant,
                      const normalized::Problem &problem);
std::string to_string(normalized::PredicateHandle predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Condition &predicate,
                      const normalized::Action &action,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Condition &predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::PredicateInstantiation &predicate,
                      const normalized::Problem &problem);
/* std::string to_string(const normalized::Condition &condition, */
/*                       const normalized::Problem &problem); */
std::string to_string(normalized::ActionHandle,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Problem &problem);

#endif /* end of include guard: TO_STRING_HPP */
