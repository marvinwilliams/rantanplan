#ifndef RULE_MATCHER_HPP
#define RULE_MATCHER_HPP

#include "lexer/char_provider.hpp"
#include "lexer/token.hpp"
#include <variant>

namespace lexer {

template <typename Token> struct MatchResult {
  Token token = ErrorToken{};
  size_t begin = 0;
  size_t end = 0;
};

template <typename Token, typename... Rules>
static constexpr MatchResult<Token> match(CharProvider &provider) noexcept {
  MatchResult<Token> result{};
  result.begin = provider.get_pos();
  result.end = provider.get_pos();
  size_t longest_match = 0;

  (
      [&result, &provider, &longest_match]() {
        provider.set_pos(result.begin);
        if (Rules::match(provider) &&
            provider.get_pos() - result.begin > longest_match) {
          result.token = Rules{};
          result.end = provider.get_pos();
          longest_match = result.end - result.begin;
        }
      }(),
      ...);
  return result;
}
} // namespace lexer

#endif /* end of include guard: RULE_MATCHER_HPP */