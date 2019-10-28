#include "util/combinatorics.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

CombinationIterator::CombinationIterator(
    std::vector<size_t> list_sizes) noexcept
    : list_sizes_{std::move(list_sizes)},
      current_combination_(list_sizes_.size()) {
  number_combinations_ = std::accumulate(
      list_sizes_.cbegin(), list_sizes_.cend(), 1ul, std::multiplies<>());
  if (number_combinations_ == 0) {
    is_end_ = true;
  }
}

CombinationIterator &CombinationIterator::operator++() noexcept {
  if (is_end_) {
    return *this;
  }
  for (size_t i = 0; i < list_sizes_.size(); ++i) {
    current_combination_[i]++;
    if (current_combination_[i] < list_sizes_[i]) {
      return *this;
    } else {
      current_combination_[i] = 0;
    }
  }
  is_end_ = true;
  return *this;
}

CombinationIterator CombinationIterator::operator++(int) noexcept {
  CombinationIterator old = *this;
  ++(*this);
  return old;
}

void CombinationIterator::reset() noexcept {
  if (!is_end_) {
    std::fill(current_combination_.begin(), current_combination_.end(), 0);
  }
  is_end_ = (number_combinations_ == 0);
}
