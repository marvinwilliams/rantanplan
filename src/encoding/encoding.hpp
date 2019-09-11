#ifndef ENCODING_HPP
#define ENCODING_HPP

#include "model/model.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

namespace encoding {

std::vector<model::GroundPredicate>
ground_predicate(const model::Problem &problem,
                 model::PredicatePtr predicate_ptr) {
  const model::PredicateDefinition &predicate =
      model::get(problem.predicates, predicate_ptr);
  std::vector<model::GroundPredicate> predicates;
  std::vector<std::vector<model::ConstantPtr>> stack(1);
  size_t number_predicates = std::accumulate(
      predicate.parameters.begin(), predicate.parameters.end(), 1ul,
      [&problem](size_t i, const auto &parameter) {
        return i * problem.constants_of_type[parameter.type.i].size();
      });
  predicates.reserve(number_predicates);
  while (!stack.empty()) {
    std::vector<model::ConstantPtr> last = stack.back();
    stack.pop_back();
    size_t index = last.size();
    for (const auto &constant :
         problem.constants_of_type[predicate.parameters[index].type.i]) {
      std::vector<model::ConstantPtr> new_args = last;
      new_args.push_back(constant);
      if (index == predicate.parameters.size() - 1) {
        model::GroundPredicate new_predicate;
        new_predicate.definition = predicate_ptr;
        new_predicate.arguments = std::move(new_args);
        predicates.push_back(std::move(new_predicate));
      } else {
        stack.push_back(std::move(new_args));
      }
    }
  }
  return predicates;
}

class Encoder {

public:
  Encoder(const model::Problem &problem) : problem_{problem} {}

  void encode() {
    for (size_t i = 0; i < problem_.predicates.size(); ++i) {
      for (const auto &ground_predicate :
           ground_predicate(problem_, model::PredicatePtr{i})) {
        std::cout << '\n';
        std::cout << '\t';
        std::cout << get(problem_.predicates, ground_predicate.definition).name;
        std::cout << "(";
        for (auto it = ground_predicate.arguments.cbegin();
             it != ground_predicate.arguments.cend(); ++it) {
          if (it != ground_predicate.arguments.cbegin()) {
            std::cout << ", ";
          }
          std::cout << get(problem_.constants, *it).name;
        }
        std::cout << ")";
      }
    }
  }

protected:
  std::vector<model::GroundPredicate> predicates_;
  const model::Problem &problem_;
};

} // namespace encoding

#endif /* end of include guard: ENCODING_HPP */
