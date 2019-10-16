#ifndef SAT_MODEL_HPP
#define SAT_MODEL_HPP

#include <vector>

namespace sat {

struct Model {
  bool operator[](size_t i) const noexcept { return assignment[i]; }
  std::vector<bool> assignment;
};

} // namespace sat

#endif /* end of include guard: SAT_MODEL_HPP */
