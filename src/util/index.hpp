#ifndef INDEX_HPP
#define INDEX_HPP

#include <cstdint>
#include <functional>
#include <type_traits>

namespace util {

template <typename T> struct Index {
  using value_type = uint_fast64_t;
  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  Index(Integral i = 0) : i{static_cast<value_type>(i)} {}
  Index(const Index &other) : Index{other.i} {}
  Index() : Index(0) {}
  Index &operator=(const Index<T> &other) {
    i = other.i;
    return *this;
  }

  inline operator value_type() const { return i; }

  inline Index &operator++() {
    ++i;
    return *this;
  }

  inline Index operator++(int) {
    Index old{*this};
    ++(*this);
    return old;
  }

  value_type i;
};

template <typename T>
inline bool operator==(const Index<T> &first, const Index<T> &second) {
  return first.i == second.i;
}

template <typename T>
inline bool operator!=(const Index<T> &first, const Index<T> &second) {
  return !(first == second);
}

template <typename T>
inline bool operator<(const Index<T> &first, const Index<T> &second) {
  return first.i < second.i;
}

} // namespace util

namespace std {

template <typename T> struct hash<util::Index<T>> {
  size_t operator()(util::Index<T> index) const noexcept {
    return std::hash<size_t>{}(index.i);
  }
};

} // namespace std

#endif /* end of include guard: INDEX_HPP */
