#ifndef LEXER_TRAITS_HPP
#define LEXER_TRAITS_HPP

#include <cctype>

namespace lexer {

template <typename CharT> struct LexerTraits;

template <> struct LexerTraits<char> {
  // Controls whether tokens can consume newlines
  static const bool end_at_newline = true;
  // Controls whether tokens can consume blanks
  static const bool end_at_blank = false;
  // Newlines are skippend and used for location information
  static bool is_newline(char c) { return c == '\n'; }
  // Leading blanks are skipped
  static bool is_blank(char c) { return std::isblank(c); }
};

} // namespace lexer

#endif /* end of include guard: LEXER_TRAITS_HPP */
