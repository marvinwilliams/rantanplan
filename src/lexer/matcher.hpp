#ifndef MATCHER_HPP
#define MATCHER_HPP

#include "lexer.hpp"

#include <tuple>
#include <variant>
#include <vector>

namespace lexer {

namespace detail {

// This wrapper manages the status of a given rule
template <typename Rule> struct Wrapper {
  // Given a partial token and the current char, checks whether the rule still
  // matches
  template <typename CharT>
  void matches(const std::vector<CharT> &current_string, CharT c) {
    valid_before = valid_after;
    if (valid_before) {
      valid_after = Rule::matches(current_string, c);
    }
  }

  template <typename CharT>
  typename Rule::TokenType get_token(const std::vector<CharT>& current_string) {
    return Rule::get_token(current_string);
  }

  void end() {
    valid_before = valid_after;
    valid_after = false;
  }

  bool valid_before = true;
  bool valid_after = true;
};

} // namespace detail

using detail::Wrapper;

// This class manages the given rules
template <typename... Rules> class Matcher {
public:
  using RuleSet = std::tuple<Wrapper<Rules>...>;
  using Token = std::variant<ErrorToken, typename Rules::TokenType...>;

  template <typename CharT>
  void match(const std::vector<CharT> &current_string, CharT c) {
    return std::apply(
        [&current_string, c](Wrapper<Rules> &... rules) {
          (rules.matches(current_string, c), ...);
        },
        rule_set_);
  }

  // Checks whether at least one rule matches the current partial token
  bool valid() {
    return std::apply(
        [](Wrapper<Rules> &... rules) {
          return (rules.valid_after || ...);
        },
        rule_set_);
  }

  void end() {
    std::apply([](Wrapper<Rules> &... rules) { (rules.end(), ...); },
               rule_set_);
  }

  // Extracts the current token from the first matching rule
  template <typename CharT>
  Token get_token(const std::vector<CharT> &current_string) {
    return std::apply(
        [this, &current_string](Wrapper<Rules> &... rules) {
          return get_token(current_string, rules...);
        },
        rule_set_);
  }

private:
  template <typename CharT>
  Token get_token(const std::vector<CharT> &current_string,
                        Wrapper<Rules> &... rules) {
    Token token{ErrorToken{}};
    ([&token, &current_string, &rules]() {
      if (rules.valid_before) {
        token = rules.get_token(current_string);
        return true;
      }
      return false;
    }() ||
     ...);
    return token;
  }

  RuleSet rule_set_;
};

} // namespace lexer

#endif /* end of include guard: MATCHER_HPP */
