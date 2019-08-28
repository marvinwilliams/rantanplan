#ifndef MATCHER_HPP
#define MATCHER_HPP

#include "lexer/lexer.hpp"

#include <tuple>
#include <variant>
#include <string>

namespace lexer {

namespace detail {

// This wrapper manages the status of a given rule
template <typename Rule> struct Wrapper {

  // Forwards the next character and checks whether the rule still matches
  template <typename CharT> void next(CharT c) {
    if (accepting) {
      accepting = rule.accepts(c);
      matching = rule.matches();
    } else {
      matching = false;
    }
  }

  template <typename Token, typename CharT>
  bool get_token(Token& token, const std::basic_string<CharT> &current_string) const {
    if (matching) {
      token = rule.get_token(current_string);
    }
    return matching;
  }

  void reset() {
    rule.reset();
    accepting = true;
    matching = false;
  }

  bool accepting = true;
  bool matching = false;
  Rule rule;
};

} // namespace detail

using detail::Wrapper;
// Used for default initialization and represents an invalid token.
struct ErrorToken {
  static constexpr auto printable_name = "";
};


// This class manages the given rules
template <typename... Rules> class RuleSet {
public:
  using Token = std::variant<ErrorToken, typename Rules::TokenType...>;

  template <typename CharT> void next(CharT c) {
    std::apply([c](Wrapper<Rules> &... rules) { (rules.next(c), ...); },
               rules_);
  }

  bool accepts() const {
    return std::apply(
        [](const Wrapper<Rules> &... rules) { return (rules.accepting || ...); },
        rules_);
  }

  bool matching() const {
    return std::apply(
        [](const Wrapper<Rules> &... rules) { return (rules.matching || ...); },
        rules_);
  }

  void reset() {
    std::apply([](Wrapper<Rules> &... rules) { (rules.reset(), ...); }, rules_);
  }

  // Extracts the current token from the first matching rule
  template <typename CharT>
  Token get_token(const std::basic_string<CharT> &current_string) {
    Token token{ErrorToken{}};
    std::apply(
        [&token, &current_string](Wrapper<Rules> &... rules) {
          (rules.get_token(token, current_string) || ...);
         },
        rules_);
    return token;
  }

private:
  std::tuple<Wrapper<Rules>...> rules_;
};

} // namespace lexer

#endif /* end of include guard: MATCHER_HPP */
