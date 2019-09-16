#ifndef COMBINATORICS_HPP
#define COMBINATORICS_HPP

#include <algorithm>
#include <functional>
#include <vector>
#include <numeric>

std::vector<std::vector<size_t>>
all_combinations(const std::vector<size_t> &list_sizes) {
  std::vector<std::vector<size_t>> combinations;
  size_t number_combinations = std::accumulate(
      list_sizes.cbegin(), list_sizes.cend(), 1ul, std::multiplies<>());
  combinations.reserve(number_combinations);
  std::vector<size_t> combination(list_sizes.size());
  for (size_t i = 0; i < number_combinations; ++i) {
    combinations.push_back(combination);
    for (size_t j = 0; j < list_sizes.size(); ++j) {
      combination[j]++;
      if (combination[j] < list_sizes[j]) {
        break;
      } else {
        combination[j] = 0;
      }
    }
  }
  return combinations;
}

#endif /* end of include guard: COMBINATORICS_HPP */
