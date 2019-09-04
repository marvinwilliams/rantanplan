#ifndef SOLVER_HPP
#define SOLVER_HPP

#include <chrono>
#include <optional>
#include <vector>

namespace sat {
using namespace std::chrono_literals;

struct Literal {
  constexpr Literal(int index) : index{index} {}
  int index;
  Literal operator!() { return Literal{-index}; }
};
static constexpr Literal EndClause{0};

struct Model {
  bool operator[](size_t i) const { return assignment[i]; }
  std::vector<bool> assignment;
};

class Solver {
public:
  virtual Solver &operator<<(Literal literal) = 0;

  virtual void assume(Literal literal) = 0;
  virtual std::optional<Model> solve(std::chrono::seconds timeout = 0s) = 0;

  virtual ~Solver() {}
};

} // namespace sat

#endif /* end of include guard: SOLVER_HPP */
