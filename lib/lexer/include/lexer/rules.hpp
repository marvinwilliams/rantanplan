#ifndef RULES_HPP
#define RULES_HPP

#include "lexer/rule_matcher.hpp"
#include "lexer/token.hpp"

#include <string>

namespace lexer {

namespace rule {

struct basic_rule : basic_token {};

struct Empty : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &) noexcept {
    return true;
  }

  static constexpr auto printable_name = "<empty>";
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

  static constexpr auto printable_name = "<literal>";
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

  static constexpr auto printable_name = "<literalif>";
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

  static constexpr auto printable_name = "<any>";
};

template <typename Rule> struct Optional : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    Rule::match(provider);
    return true;
  }
  static constexpr auto printable_name = "<optional>";
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

  static constexpr auto printable_name = "<sequence>";
};

template <typename... Rules> struct Choice : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    return (Rules::match(provider) || ...);
  }

  static constexpr auto printable_name = "<choice>";
};

template <typename Rule> struct Star : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    while (Rule::match(provider)) {
    }
    return true;
  }

  static constexpr auto printable_name = "<star>";
};

template <typename Rule> struct Plus : Sequence<Rule, Star<Rule>> {
  static constexpr auto printable_name = "<plus>";
};

template <typename Rule> struct And : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return result;
  }

  static constexpr auto printable_name = "<And>";
};

template <typename Rule> struct Not : basic_rule {
  template <typename CharProvider>
  static constexpr bool match(CharProvider &provider) noexcept {
    auto current = provider.get_pos();
    bool result = Rule::match(provider);
    provider.set_pos(current);
    return !result;
  }

  static constexpr auto printable_name = "<Not>";
};

template <typename... Rules> struct RuleSet {
  using Token = std::variant<ErrorToken, Rules..., EndToken>;
  using Matcher = RuleMatcher<Token, Rules...>;
};

} // namespace rule

} // namespace lexer

#endif /* end of include guard: RULES_HPP */
