#ifndef SAT_MODEL_HPP
#define SAT_MODEL_HPP

#include <vector>
#include <cstdint>

namespace sat {

struct Model {
  bool operator[](std::size_t i) const noexcept { return assignment[i]; }
  std::vector<bool> assignment;
};

} // namespace sat

#endif /* end of include guard: SAT_MODEL_HPP */
