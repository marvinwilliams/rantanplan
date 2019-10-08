#ifndef TOKEN_SET_HPP
#define TOKEN_SET_HPP

#include "lexer/char_provider.hpp"
#include "lexer/token.hpp"

#include <variant>

namespace lexer {

template <typename... Rules> struct TokenSet {
  using Token = std::variant<ErrorToken, Rules..., EndToken>;
  struct MatchResult {
    Token token = ErrorToken{};
    size_t begin = 0;
    size_t end = 0;
  };

  static MatchResult match(CharProvider &provider) noexcept {
    MatchResult result{};
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
};

} // namespace lexer

#endif /* end of include guard: TOKEN_SET_HPP */
