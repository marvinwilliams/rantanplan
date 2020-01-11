#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "model/normalized/model.hpp"

#include <string>

std::string to_string(const normalized::Type &type,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Constant &constant,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Predicate &predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Condition &predicate,
                      const normalized::Action &action,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Condition &predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::PredicateInstantiation &predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Action &,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Problem &problem);
std::string to_string(const Plan &plan,
                      const normalized::Problem &problem) noexcept;

#endif /* end of include guard: TO_STRING_HPP */
