#ifndef FORMULA_HPP
#define FORMULA_HPP

#include "util/combination_iterator.hpp"

#include <cstdio>
#include <type_traits>
#include <vector>

namespace sat {

inline constexpr struct end_clause_t {
} EndClause;

template <typename Variable> struct Formula {
  struct Literal {
    constexpr explicit Literal(Variable variable, bool positive = true)
        : variable{variable}, positive{positive} {}

    Literal operator!() { return Literal{variable, !positive}; }

    Variable variable;
    bool positive;
  };

  struct Clause {
    std::vector<Literal> literals;
  };

  Formula &operator<<(Literal literal) {
    current_clause.literals.push_back(std::move(literal));
    return *this;
  }

  Formula &operator<<(end_clause_t) {
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

    auto combination_iterator = util::CombinationIterator{list_sizes};

    while (!combination_iterator.end()) {
      const auto &combination = *combination_iterator;
      for (size_t i = 0; i < combination.size(); ++i) {
        *this << formula.clauses[i].literals[combination[i]];
      }
      *this << EndClause;
      ++combination_iterator;
    }
  }

  void at_most_one(std::vector<Variable> group) {
    for (size_t i = 0; i + 1 < group.size(); ++i) {
      for (size_t j = i + 1; j < group.size(); ++j) {
        *this << Literal{group[i], false} << Literal{group[j], false}
              << EndClause;
      }
    }
  }

  Clause current_clause;
  std::vector<Clause> clauses;
};

} // namespace sat

#endif /* end of include guard: FORMULA_HPP */
