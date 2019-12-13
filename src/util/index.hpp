#ifndef INDEX_HPP
#define INDEX_HPP

#include <cstdint>
#include <type_traits>
#include <functional>

template <typename T> class Index {
public:
  using value_type = uint_fast64_t;
  template <typename Integral,
            std::enable_if_t<std::is_integral_v<Integral>, int> = 0>
  explicit Index(Integral i = 0) : i_{static_cast<value_type>(i)} {}
  Index(const Index &other) : Index{other.i} {}
  Index() : Index(0) {}
  Index &operator=(const Index<T> &other) {
    i_ = other.i_;
    return *this;
  }

  inline operator value_type() const { return i_; }

  inline Index &operator++() {
    ++i_;
    return *this;
  }

  inline Index operator++(int) {
    Index old{*this};
    ++(*this);
    return old;
  }

private:
  value_type i_;
};

template <typename T>
inline bool operator==(const Index<T> &first, const Index<T> &second) {
  return first.i_ == second.i_;
}

template <typename T>
inline bool operator!=(const Index<T> &first, const Index<T> &second) {
  return !(first == second);
}

namespace std {

template <typename T> struct hash<Index<T>> {
  size_t operator()(Index<T> handle) const noexcept {
    return std::hash<size_t>{}(handle.i);
  }
};

} // namespace std

#endif /* end of include guard: INDEX_HPP */
