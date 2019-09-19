#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "model/model.hpp"
#include "model/support.hpp"
#include "sat/formula.hpp"
#include "sat/ipasir_solver.hpp"

#include <variant>
#include <vector>

namespace encoding {

using namespace std::chrono_literals;

static logging::Logger logger{"Encoding"};

struct ActionVariable {
  model::ActionPtr action_ptr;
};

struct PredicateVariable {
  model::GroundPredicatePtr predicate_ptr;
  bool this_step;
};

struct ParameterVariable {
  model::ActionPtr action_ptr;
  size_t parameter_index;
  size_t constant_index;
};

using Variable =
    std::variant<ActionVariable, PredicateVariable, ParameterVariable>;

class Encoder {

public:
  using Formula = sat::Formula<Variable>;
  using Literal = Formula::Literal;
  using Plan =
      std::vector<std::pair<model::ActionPtr, std::vector<model::ConstantPtr>>>;

  explicit Encoder(const model::Problem &problem)
      : solver_{std::make_unique<sat::IpasirSolver>()}, problem_{problem},
        support_{problem} {}

  virtual ~Encoder() = default;

  void encode() {
    generate_formula_();
    init_vars_();
  }

  virtual void plan(const Config &) = 0;

protected:
  Encoder(const Encoder &) = delete;
  Encoder(Encoder &&) = default;
  Encoder &operator=(const Encoder &) = delete;
  Encoder &operator=(Encoder &&) = default;

  struct Representation {
    const unsigned int SAT = 1;
    std::vector<unsigned int> predicates;
    std::vector<unsigned int> actions;
    std::vector<std::vector<std::vector<unsigned int>>> parameters;
    size_t num_vars;
  };

  std::unique_ptr<sat::Solver> solver_;

  const model::Problem &problem_;
  model::Support support_;
  Representation representation_;

private:
  virtual void generate_formula_() = 0;
  virtual void init_vars_() = 0;
};

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
