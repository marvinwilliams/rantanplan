#ifndef RULES_HPP
#define RULES_HPP

#include "location.hpp"
#include <tuple>
#include <variant>
#include <vector>

namespace lexer {

template <typename RuleT> struct Matcher {
  template <typename CharT>
  bool matches(const std::vector<CharT> &current_token, CharT c) {
    valid_before = valid_after;
    if (valid_before) {
      valid_after = rule.matches(current_token, c);
    }
    return valid_after;
  }

  void reset() {
    valid_before = true;
    valid_after = true;
  }

  void end() {
    valid_before = valid_after;
    valid_after = false;
  }

  bool valid_before = true;
  bool valid_after = true;
  RuleT rule;
};

template <typename... TokenizerFuncs> class Rules {
public:
  using RuleSet = std::tuple<Matcher<TokenizerFuncs>...>;
  using return_type = std::variant<typename TokenizerFuncs::token_type...>;

  template <typename CharT>
  bool valid(const std::vector<CharT> &current_token, CharT current_char) {
    return std::apply(
        [&current_token, current_char](Matcher<TokenizerFuncs> &... matchers) {
          return (matchers.matches(current_token, current_char) | ...);
        },
        rule_set);
  }

  void reset() {
    std::apply(
        [](Matcher<TokenizerFuncs> &... matchers) { (matchers.reset(), ...); },
        rule_set);
  }

  void end() {
    std::apply(
        [](Matcher<TokenizerFuncs> &... matchers) { (matchers.end(), ...); },
        rule_set);
  }

  template <typename CharT>
  return_type get_token(const std::vector<CharT> &current_token,
                        Location location) {
    return std::apply(
        [this, &current_token,
         &location](Matcher<TokenizerFuncs> &... matchers) {
          return get_token(current_token, location, matchers...);
        },
        rule_set);
  }

private:
  template <typename CharT>
  return_type get_token(const std::vector<CharT> &current_token,
                        Location location,
                        Matcher<TokenizerFuncs> &... matchers) {
    return_type token;
    ([&token, &current_token, &location, &matchers]() {
      if (matchers.valid_before) {
        token = matchers.rule.get_token(current_token, location);
      }
      return matchers.valid_before;
    }() ||
     ...);
    return token;
  }

  RuleSet rule_set;
};

} // namespace lexer

#endif /* end of include guard: RULES_HPP */
