#ifndef COMBINATION_ITERATOR_HPP
#define COMBINATION_ITERATOR_HPP

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

  CombinationIterator operator++(int) noexcept {
    CombinationIterator old = *this;
    ++(*this);
    return old;
  }

  void reset() noexcept {
    if (!is_end_) {
      std::fill(current_combination_.begin(), current_combination_.end(), 0);
    }
    is_end_ = (number_combinations_ == 0);
  }

  inline size_t number_combinations() const noexcept {
    return number_combinations_;
  }

  inline bool operator==(const CombinationIterator &other) const noexcept {
    return current_combination_ == other.current_combination_;
  }

  inline bool operator!=(const CombinationIterator &other) const noexcept {
    return !(*this == other);
  }

  inline reference operator*() const noexcept { return current_combination_; }
  inline bool end() const noexcept { return is_end_; }

private:
  bool is_end_;
  size_t number_combinations_;
  const std::vector<size_t> list_sizes_;
  std::vector<size_t> current_combination_;
};

} // namespace util

#endif /* end of include guard: COMBINATION_ITERATOR_HPP */
