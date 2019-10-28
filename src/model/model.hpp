#ifndef MODEL_HPP
#define MODEL_HPP

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace model {

template <typename T> struct Handle {
  using value_type = size_t;
  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  explicit Handle(Integral i = 0) : i{static_cast<value_type>(i)} {}
  Handle(const Handle &handle) : Handle{handle.i} {}
  Handle() : Handle(0) {}
  Handle &operator=(const Handle<T> &handle) {
    i = handle.i;
    return *this;
  }

  inline operator value_type() const { return i; }

  inline Handle &operator++() {
    ++i;
    return *this;
  }

  inline Handle operator++(int) {
    Handle old{*this};
    ++(*this);
    return old;
  }

  value_type i;
};

template <typename T>
inline bool operator==(const Handle<T> &first, const Handle<T> &second) {
  return first.i == second.i;
}

template <typename T>
inline bool operator!=(const Handle<T> &first, const Handle<T> &second) {
  return !(first == second);
}

namespace hash {

template <typename T> struct Handle {
  size_t operator()(const ::model::Handle<T> &handle) const {
    return std::hash<size_t>{}(handle.i);
  }
};

} // namespace hash

struct Type;
struct Constant;
struct Parameter;
struct PredicateDefinition;
struct GroundPredicate;
struct Action;

using TypeHandle = Handle<Type>;
using ConstantHandle = Handle<Constant>;
using ParameterHandle = Handle<Parameter>;
using PredicateHandle = Handle<PredicateDefinition>;
using GroundPredicateHandle = Handle<GroundPredicate>;
using ActionHandle = Handle<Action>;

struct Type {
  Type(const std::string &name, TypeHandle parent)
      : name{name}, parent{parent} {}
  std::string name;
  TypeHandle parent;
};

struct Parameter {
  explicit Parameter(const std::string &name, TypeHandle type)
      : name{name}, type{type} {}
  std::string name;
  TypeHandle type;
};

struct PredicateDefinition {
  explicit PredicateDefinition(const std::string &name) : name{name} {}
  std::string name;
  std::vector<Parameter> parameters;
};

struct Constant {
  Constant(const std::string &name, TypeHandle type) : name{name}, type{type} {}
  std::string name;
  TypeHandle type;
};

using Argument = std::variant<ConstantHandle, ParameterHandle>;

struct Junction;
struct PredicateEvaluation;
template <bool> struct Trivial {};
using TrivialTrue = Trivial<true>;
using TrivialFalse = Trivial<false>;

using Condition =
    std::variant<Junction, PredicateEvaluation, TrivialTrue, TrivialFalse>;

struct PredicateEvaluation {
  PredicateEvaluation(PredicateHandle predicate) : definition{predicate} {}

  PredicateEvaluation(PredicateHandle predicate,
                      std::vector<Argument> arguments)
      : definition{predicate}, arguments{std::move(arguments)} {}
  PredicateHandle definition;
  std::vector<Argument> arguments;
  bool negated = false;
};

struct Junction {
  enum class Connective { And, Or };
  Connective connective;
  std::vector<Condition> arguments;
};

struct Action {
  explicit Action(const std::string &name) : name{name} {}
  std::string name;
  std::vector<Parameter> parameters;
  std::vector<PredicateEvaluation> preconditions;
  std::vector<PredicateEvaluation> effects;
};

inline bool operator==(const Action &first, const Action &second) {
  return first.name == second.name;
}

struct ArgumentMapping {
  explicit ArgumentMapping(
      const std::vector<model::Parameter> &parameters,
      const std::vector<model::Argument> &arguments) noexcept {
    std::vector<std::vector<ParameterHandle>> parameter_matches{
        parameters.size()};
    for (size_t i = 0; i < arguments.size(); ++i) {
      auto parameter_handle = std::get_if<ParameterHandle>(&arguments[i]);
      if (parameter_handle && !parameters[*parameter_handle].constant) {
        parameter_matches[*parameter_handle].push_back(ParameterHandle{i});
      }
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (!parameter_matches[i].empty()) {
        matches.emplace_back(ParameterHandle{i},
                             std::move(parameter_matches[i]));
      }
    }
  }

  std::vector<std::pair<ParameterHandle, std::vector<ParameterHandle>>> matches;
};

struct ArgumentAssignment {
  explicit ArgumentAssignment() noexcept = default;
  explicit ArgumentAssignment(const ArgumentMapping &mapping,
                              const std::vector<size_t> &arguments) noexcept {
    assert(arguments.size() == mapping.size());
    this->arguments.reserve(mapping.matches.size());
    for (size_t i = 0; i < mapping.matches.size(); ++i) {
      this->arguments.insert({mapping.matches[i].first, arguments[i]});
    }
  }

  std::unordered_map<ParameterHandle, size_t, hash::Handle<Parameter>>
      arguments;
};

struct ProblemHeader {
  std::string domain_name;
  std::string problem_name;
  std::vector<std::string> requirements;
};

struct ProblemBase {
  virtual ~ProblemBase() = default;

  ProblemHeader header;
  std::vector<Type> types;
  std::vector<Constant> constants;
  std::vector<PredicateDefinition> predicates;

protected:
  ProblemBase() = default;
  ProblemBase(const ProblemBase &) = default;
  ProblemBase(ProblemBase &&) = default;
  ProblemBase &operator=(const ProblemBase &) = default;
  ProblemBase &operator=(ProblemBase &&) = default;
};

struct Problem : ProblemBase {
  explicit Problem(const ProblemBase &other) : ProblemBase{other} {
    constants_by_type.resize(types.size());
    for (size_t i = 0; i < constants.size(); ++i) {
      TypeHandle type = constants[i].type;
      constants_by_type[type].push_back(ConstantHandle{i});
      while (types[type].parent != type) {
        type = types[type].parent;
        constants_by_type[type].push_back(ConstantHandle{i});
      }
    }
  }

  std::vector<Action> actions;
  std::vector<PredicateEvaluation> initial_state;
  std::vector<PredicateEvaluation> goal;
  std::vector<std::vector<ConstantHandle>> constants_by_type;
};

struct AbstractAction {
  std::string name;
  std::vector<Parameter> parameters;
  Condition precondition = TrivialTrue{};
  Condition effect = TrivialTrue{};
};

struct AbstractProblem : ProblemBase {
  std::vector<AbstractAction> actions;
  model::Condition initial_state = TrivialTrue{};
  model::Condition goal = TrivialTrue{};
};

struct GroundPredicate {
  GroundPredicate(PredicateHandle definition,
                  std::vector<ConstantHandle> arguments)
      : definition{definition}, arguments(std::move(arguments)) {}

  GroundPredicate(const PredicateEvaluation &predicate) {
    /* assert(is_grounded(predicate)); */
    definition = predicate.definition;
    arguments.reserve(predicate.arguments.size());
    for (const auto &argument : predicate.arguments) {
      arguments.push_back(std::get<ConstantHandle>(argument));
    }
  }
  GroundPredicate(const Problem &problem, const PredicateEvaluation &predicate,
                  const ArgumentAssignment &assignment) {
    /* assert(is_grounded(predicate, action)); */
    definition = predicate.definition;
    arguments.reserve(predicate.arguments.size());
    for (const auto &argument : predicate.arguments) {
      if (const ConstantHandle *p = std::get_if<ConstantHandle>(&argument)) {
        arguments.push_back(*p);
      } else {
        /* arguments.push_back(problem.constants_by_type[assiassignment.arguments.at(std::get<ParameterHandle>(argument))
         */
      }
    }
  }

  PredicateHandle definition;
  std::vector<ConstantHandle> arguments;
};

inline bool operator==(const GroundPredicate &first,
                       const GroundPredicate &second) {
  return first.definition == second.definition &&
         first.arguments == second.arguments;
}

namespace hash {

struct GroundPredicate {
  size_t operator()(const ::model::GroundPredicate &predicate) const noexcept {
    size_t hash = Handle<PredicateDefinition>{}(predicate.definition);
    for (ConstantHandle p : predicate.arguments) {
      hash ^= Handle<Constant>{}(p);
    }
    return hash;
  }
};

} // namespace hash

} // namespace model

#endif /* end of include guard: MODEL_HPP */
