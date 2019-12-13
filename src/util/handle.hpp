#ifndef HANDLE_HPP
#define HANDLE_HPP

/* This class is intended to only be instantiatable by the base and represents a
 * pointer */

template <typename T, typename Base> class Handle {
public:
  friend Base;

  Handle() = default;

  const T *get() { return p_; }
  const Base *get_base() { return base_; }
  const T &operator*() { return *p_; }
  const T *operator->() { return p_; }
  bool operator==(const Handle<T, Base> &other) const {
    return p_ == other.p_ && base_ == other.base_;
  }

private:
  Handle(T *p, const Base *base) : p_{p}, base_{base} {}

  T *p_ = nullptr;
  const Base *base_ = nullptr;
};

#endif /* end of include guard: HANDLE_HPP */
