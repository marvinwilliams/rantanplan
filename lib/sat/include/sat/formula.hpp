#ifndef FORMULA_HPP
#define FORMULA_HPP

#include "util/combinatorics.hpp"
#include <type_traits>
#include <vector>
#include <cstdio>

namespace sat {

namespace detail {

struct end_clause_t {};

} // namespace detail

static const detail::end_clause_t EndClause;

template <typename Variable> struct Formula {
  struct Literal {
    constexpr explicit Literal(Variable variable, bool negated = false)
        : variable{variable}, negated{negated} {}

    Literal operator!() { return Literal{variable, !negated}; }

    Variable variable;
    bool negated;
  };

  struct Clause {
    std::vector<Literal> literals;
  };

  Formula &operator<<(Literal literal) {
    current_clause.literals.push_back(std::move(literal));
    return *this;
  }

  Formula &operator<<(detail::end_clause_t) {
    clauses.push_back(std::move(current_clause));
    current_clause.literals.clear();
    return *this;
  }

  void add_formula(const Formula &formula) {
    clauses.insert(clauses.cend(), formula.clauses.begin(),
                   formula.clauses.end());
  }

  void add_dnf(const Formula &formula) {
    std::vector<size_t> list_sizes;
    list_sizes.reserve(formula.clauses.size());
    for (const auto &clause : formula.clauses) {
      list_sizes.push_back(clause.literals.size());
    }

    auto combinations = all_combinations(list_sizes);
    /* fprintf(stderr, "Dimension: %lu\n", combinations.size()); */

    for (const auto &combination : combinations) {
      for (size_t i = 0; i < combination.size(); ++i) {
        *this << formula.clauses[i].literals[combination[i]];
      }
      *this << EndClause;
    }
  }

  void at_most_one(std::vector<Variable> group) {
    for (size_t i = 0; i + 1 < group.size(); ++i) {
      for (size_t j = i + 1; j < group.size(); ++j) {
        *this << Literal{group[i], true} << Literal{group[j], true}
              << EndClause;
      }
    }
  }

  Clause current_clause;
  std::vector<Clause> clauses;
};

} // namespace sat

#endif /* end of include guard: FORMULA_HPP */
