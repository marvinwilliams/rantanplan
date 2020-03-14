#ifndef TO_STRING_HPP
#define TO_STRING_HPP

#include "model/normalized/model.hpp"

#include <string>
#include <sstream>

template <typename Iterator, typename Predicate>
void print_list(std::stringstream &ss, Iterator begin, Iterator end,
                const std::string &delim, Predicate &&predicate) noexcept {
  for (auto it = begin; it != end; ++it) {
    if (it != begin) {
      ss << delim;
    }
    ss << predicate(*it);
  }
}

std::string to_string(const normalized::TypeIndex &type,
                      const normalized::Problem &problem);
std::string to_string(const normalized::ConstantIndex &constant,
                      const normalized::Problem &problem);
std::string to_string(const normalized::PredicateIndex &predicate,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Condition &condition,
                      const normalized::Action &action,
                      const normalized::Problem &problem);
std::string to_string(const normalized::GroundAtom &ground_atom,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Action &,
                      const normalized::Problem &problem);
std::string to_string(const normalized::Problem &problem);
std::string to_string(const Plan &plan) noexcept;

#endif /* end of include guard: TO_STRING_HPP */
