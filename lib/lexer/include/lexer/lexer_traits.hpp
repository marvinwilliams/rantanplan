#ifndef LEXER_TRAITS_HPP
#define LEXER_TRAITS_HPP

#include <cctype>

namespace lexer {

struct LexerTraits {
  LexerTraits() = delete;

  // Controls whether tokens can consume newlines
  static constexpr bool end_at_newline = true;

  // Controls whether tokens can consume blanks
  static constexpr bool end_at_blank = false;

  // Newlines are skippend and used for location information
  static constexpr bool is_newline(char c) noexcept {
    return c == '\n' || c == '\r';
  }

  // Leading blanks are skipped
  static constexpr bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t';
  }

  static constexpr bool is_digit(char c) noexcept {
    return '0' <= c && c <= '9';
  }

  static constexpr bool is_upper(char c) noexcept {
    return 'A' <= c && c <= 'Z';
  }

  static constexpr bool is_lower(char c) noexcept {
    return 'a' <= c && c <= 'z';
  }

  static constexpr bool is_alpha(char c) noexcept {
    return is_upper(c) || is_lower(c);
  }

  static constexpr bool is_alnum(char c) noexcept {
    return is_digit(c) || is_alpha(c);
  }
};

} // namespace lexer

#endif /* end of include guard: LEXER_TRAITS_HPP */
