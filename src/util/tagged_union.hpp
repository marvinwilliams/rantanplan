#ifndef TAGGED_UNION_HPP
#define TAGGED_UNION_HPP

#include <cassert>

template <typename T1, typename T2> class TaggedUnion {
protected:
  bool holds_t1_;
  union {
    T1 t1_;
    T2 t2_;
  };

public:
  explicit TaggedUnion(T1 t) noexcept : holds_t1_{true}, t1_{t} {}
  explicit TaggedUnion(T2 t) noexcept : holds_t1_{false}, t2_{t} {}
  explicit TaggedUnion(const TaggedUnion<T1, T2> &other) noexcept
      : holds_t1_{other.holds_t1_} {
    if (other.holds_t1_) {
      t1_ = other.t1_;
    } else {
      t2_ = other.t2_;
    }
  }

  TaggedUnion &operator=(const TaggedUnion<T1, T2> &other) noexcept {
    holds_t1_ = other.holds_t1_;
    if (other.holds_t1_) {
      t1_ = other.t1_;
    } else {
      t2_ = other.t2_;
    }
    return *this;
  }

  inline void set(T1 t) noexcept {
    holds_t1_ = true;
    t1_ = t;
  }

  inline void set(T2 t) noexcept {
    holds_t1_ = false;
    t2_ = t;
  }

  inline T1 get_first() const noexcept {
    assert(holds_t1_);
    return t1_;
  }

  inline T2 get_second() const noexcept {
    assert(!holds_t1_);
    return t2_;
  }
};

#endif /* end of include guard: TAGGED_UNION_HPP */
