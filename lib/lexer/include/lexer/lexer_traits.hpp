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
};

} // namespace lexer

#endif /* end of include guard: LEXER_TRAITS_HPP */
