#ifndef REPRESENTATION_HPP
#define REPRESENTATION_HPP

#include "model/model.hpp"
#include "sat/formula.hpp"
#include <utility>
#include <vector>

namespace sat {

class Representation {
public:
  Representation(const model::Problem &problem) : problem_{problem} {}

  virtual Variable get_variable(const model::Action &action) const = 0;

  virtual Variable
  get_variable(const model::GroundPredicate &predicate) const = 0;

  virtual Formula get_action_assignment(
      const model::Action &action,
      std::vector<std::pair<size_t, model::Constant>>) const = 0;

  virtual size_t get_num_vars() const = 0;

  const model::Problem &get_problem() const { return problem_; }

  virtual ~Representation() {}

protected:
  const Variable::value_type SAT = 1;
  const model::Problem &problem_;
};

} // namespace sat

#endif /* end of include guard: REPRESENTATION_HPP */
