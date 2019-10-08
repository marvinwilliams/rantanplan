#ifndef LITERAL_CLASS_HPP
#define LITERAL_CLASS_HPP

struct LiteralClass {
  // Newlines are skippend and used for location information
  static constexpr bool newline(char c) noexcept {
    return c == '\n' || c == '\r';
  }

  // Leading blanks are skipped
  static constexpr bool blank(char c) noexcept {
    return c == ' ' || c == '\t';
  }

  static constexpr bool digit(char c) noexcept {
    return '0' <= c && c <= '9';
  }

  static constexpr bool upper(char c) noexcept {
    return 'A' <= c && c <= 'Z';
  }

  static constexpr bool lower(char c) noexcept {
    return 'a' <= c && c <= 'z';
  }

  static constexpr bool alpha(char c) noexcept {
    return upper(c) || lower(c);
  }

  static constexpr bool alnum(char c) noexcept {
    return digit(c) || alpha(c);
  }
};

#endif /* end of include guard: LITERAL_CLASS_HPP */
