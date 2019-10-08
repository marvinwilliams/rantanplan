#ifndef CHAR_PROVIDER_HPP
#define CHAR_PROVIDER_HPP

namespace lexer {

struct CharProvider {
  explicit CharProvider(char *begin, char *end) noexcept
      : begin_{begin}, current_{begin}, end_{end} {}

  inline char get() noexcept { return *(current_ + delta_++); }

  inline void bump() noexcept {
    current_ += delta_;
    delta_ = 0;
  }

  inline size_t length() const noexcept {
    return static_cast<size_t>(end_ - current_);
  }

  inline size_t get_pos() const noexcept {
    return static_cast<size_t>(current_ - begin_);
  }

  inline void set_pos(size_t pos) noexcept {
    current_ = begin_ + pos;
    delta_ = 0;
  }

  inline void skip(size_t n) noexcept { delta_ += n; }

  inline void reset() noexcept { delta_ = 0; }

private:
  size_t delta_ = 0;
  char *begin_ = nullptr;
  char *current_ = nullptr;
  char *end_ = nullptr;
};

} // namespace lexer

#endif /* end of include guard: CHAR_PROVIDER_HPP */
