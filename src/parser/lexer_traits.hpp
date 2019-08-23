#ifndef LEXER_TRAITS_HPP
#define LEXER_TRAITS_HPP

#include <cctype>

namespace lexer {

template <typename CharT> struct LexerTraits;

template <> struct LexerTraits<char> {
  static bool is_newline(char c) { return c == '\n'; }
  static bool is_blank(char c) { return std::isblank(c); }
};

} // namespace lexer

#endif /* end of include guard: LEXER_TRAITS_HPP */
