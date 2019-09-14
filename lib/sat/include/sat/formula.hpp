#ifndef FORMULA_HPP
#define FORMULA_HPP

#include "algorithm"
#include <type_traits>
#include <vector>

namespace sat {

class Variable {
public:
  using value_type = unsigned int;

  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  constexpr Literal(Integral index) : index{static_cast<value_type>(index)} {
    assert(index > 1);
  }

  operator value_type() const { return index; }

private:
  value_type index;
};

class Literal {
public:
  constexpr explicit Literal(Variable variable, bool negated = false)
      : variable{variable}, negated{negated} {}

  Literal operator!() { return Literal{variable, !negated}; }
  operator int() const { return (negated ? -1 : 1) * variable; }

private:
  Variable variable;
  bool negated = true;
};

struct Clause {
  std::vector<Literal> literals;
};

namespace detail {

struct end_clause_t {};

} // namespace detail

static const detail::end_clause_t EndClause;

struct Formula {
  Formula &operator<<(Literal literal) {
    current_clause.literals.push_back(literal);
  }

  Formula &operator<<(detail::end_clause_t) {
    clauses.push_back(std::move(current_clause));
    current_clause.literals.clear();
  }

  void add_formula(Formula &&formula) {
    clauses.insert(clauses.cend(), formula.clauses.begin(),
                   formula.clauses.end());
  }

  void add_dnf(const Formula &formula) {
    std::vector<size_t> list_sizes;
    list_sizes.reserve(formula.clauses.size());
    for (const auto &clause : formula.clauses) {
      list_sizes.push_back(clause.literals.size());
    }

    auto combinations = algorithm::all_combinations(list_sizes);

    for (const auto &combination : combinations) {
      for (size_t i = 0; i < combination.size(); ++i) {
        *this << formula.clauses[i].literals[combination[i]];
      }
      *this << EndClause;
    }
  }

  void at_most_one(std::vector<Literal> group) {
    for (size_t i = 0; i < group.size() - 1; ++i) {
      for (size_t j = i + 1; j < group.size(); ++j) {
        *this << !Literal{group[i]} << !Literal{group[j]} << EndClause;
      }
    }
  }

  Clause current_clause;
  std::vector<Clause> clauses;
};

} // namespace sat

#endif /* end of include guard: FORMULA_HPP */
