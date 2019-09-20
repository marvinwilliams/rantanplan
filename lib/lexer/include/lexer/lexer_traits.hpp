#ifndef LEXER_TRAITS_HPP
#define LEXER_TRAITS_HPP

#include <cctype>

namespace lexer {

struct LexerTraits {
  using char_type = char;

  // Controls whether tokens can consume newlines
  static constexpr bool end_at_newline = true;

  // Controls whether tokens can consume blanks
  static constexpr bool end_at_blank = false;

  // Newlines are skippend and used for location information
  static constexpr bool is_newline(char_type c) { return c == '\n'; }

  // Leading blanks are skipped
  static bool is_blank(char_type c) { return std::isblank(c); }
};

} // namespace lexer

#endif /* end of include guard: LEXER_TRAITS_HPP */
