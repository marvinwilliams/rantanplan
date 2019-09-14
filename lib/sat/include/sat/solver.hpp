#ifndef SOLVER_HPP
#define SOLVER_HPP

#include <chrono>
#include <optional>
#include <vector>

namespace sat {
using namespace std::chrono_literals;

struct Model {
  bool operator[](size_t i) const { return assignment[i]; }
  std::vector<bool> assignment;
};

class Solver {
public:
  virtual Solver &operator<<(Literal literal) = 0;

  virtual void assume(Literal literal) = 0;
  virtual std::optional<Model> solve(std::chrono::milliseconds timeout = 0s) = 0;

  virtual ~Solver() {}
};

} // namespace sat

#endif /* end of include guard: SOLVER_HPP */
