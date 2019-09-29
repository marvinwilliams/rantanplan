#ifndef RULES_HPP
#define RULES_HPP

#include "lexer/token.hpp"
#include "lexer/rule_matcher.hpp"

namespace lexer {

namespace rule {

struct basic_rule : basic_token {};

struct Empty : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &) noexcept {
    return true;
  }
};

template <char c> struct Literal : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && provider.get() == c) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }
};

template <typename Predicate> struct LiteralIf : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    if (provider.length() > 0 && Predicate{}(provider.get())) {
      provider.bump();
      return true;
    }
    provider.reset();
    return false;
  }
};

template <size_t N> struct Any : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    if (provider.length() < N) {
      return false;
    }
    provider.skip(N);
    provider.bump();
    return true;
  }
};

template <typename Rule> struct Optional : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    Rule::match(provider);
    return true;
  }
};

template <typename... Rules> struct Sequence : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = (Rules::match(provider) && ...);
    if (!result) {
      provider.set_pos(current);
    }
    return result;
  }
};

template <typename... Rules> struct Choice : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    return (Rules::match(provider) || ...);
  }
};

template <typename Rule> struct Star : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    while (Rule::match(provider)) {
    }
    return true;
  }
};

template <typename Rule> struct Plus : Sequence<Rule, Star<Rule>> {};

template <typename Rule> struct And : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return result;
  }
};

template <typename Rule> struct Not : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return !result;
  }
};

template <typename... Rules> struct RuleSet {
  using Token = std::variant<ErrorToken, Rules..., EndToken>;
  using Matcher = RuleMatcher<Token, Rules...>;
};

} // namespace rule

} // namespace lexer

#endif /* end of include guard: RULES_HPP */
