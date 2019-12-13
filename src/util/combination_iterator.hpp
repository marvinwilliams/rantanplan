#ifndef COMBINATION_ITERATOR_HPP
#define COMBINATION_ITERATOR_HPP

#include <iterator>
#include <vector>

class CombinationIterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::vector<size_t>;
  using difference_type = std::ptrdiff_t;
  using pointer = const std::vector<size_t> *;
  using reference = const std::vector<size_t> &;

  explicit CombinationIterator(std::vector<size_t> list_sizes) noexcept;

  CombinationIterator &operator++() noexcept;

  CombinationIterator operator++(int) noexcept {
    CombinationIterator old = *this;
    ++(*this);
    return old;
  }

  void reset() noexcept;

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

#endif /* end of include guard: COMBINATION_ITERATOR_HPP */
