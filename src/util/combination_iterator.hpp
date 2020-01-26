#ifndef COMBINATION_ITERATOR_HPP
#define COMBINATION_ITERATOR_HPP

#include <cassert>
#include <iterator>
#include <numeric>
#include <vector>

namespace util {

class CombinationIterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::vector<size_t>;
  using difference_type = std::ptrdiff_t;
  using pointer = const std::vector<size_t> *;
  using reference = const std::vector<size_t> &;

  explicit CombinationIterator() noexcept = default;

  explicit CombinationIterator(std::vector<size_t> list_sizes) noexcept
      : list_sizes_{std::move(list_sizes)},
        current_combination_(list_sizes_.size()) {
    number_combinations_ = std::accumulate(
        list_sizes_.cbegin(), list_sizes_.cend(), 1ul, std::multiplies<>());
    is_end_ = (number_combinations_ == 0);
  }

  CombinationIterator &operator++() noexcept {
    if (is_end_) {
      return *this;
    }
    for (size_t i = list_sizes_.size(); i-- > 0;) {
      if (++current_combination_[i] < list_sizes_[i]) {
        return *this;
      } else {
        current_combination_[i] = 0;
      }
    }
    is_end_ = true;
    return *this;
  }

  CombinationIterator operator++(int) noexcept {
    CombinationIterator old = *this;
    ++(*this);
    return old;
  }

  inline size_t get_num_combinations() const noexcept {
    return number_combinations_;
  }

  inline reference operator*() const noexcept { return current_combination_; }

  bool operator!=(const CombinationIterator &) const noexcept {
    return !is_end_;
  }

  bool operator==(const CombinationIterator &) const noexcept {
    return is_end_;
  }

private:
  bool is_end_ = true;
  size_t number_combinations_;
  std::vector<size_t> list_sizes_;
  std::vector<size_t> current_combination_;
};

} // namespace util

#endif /* end of include guard: COMBINATION_ITERATOR_HPP */
