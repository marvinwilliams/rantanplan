#ifndef LEXER_HPP
#define LEXER_HPP

#include "lexer/lexer_exception.hpp"
#include "lexer/lexer_traits.hpp"
#include "lexer/location.hpp"
#include "lexer/rules.hpp"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <variant>

namespace lexer {

template <typename T> void apply(char *, char *, T &) {}

template <typename RuleSet, typename Traits = LexerTraits> class Lexer {
public:
  using Token = typename RuleSet::Token;
  using Matcher = typename RuleSet::Matcher;

  struct Provider {
    explicit Provider(char *begin, char *end) noexcept
        : begin_{begin}, current_{begin}, end_{end} {}

    constexpr char get() noexcept { return *(current_ + delta_++); }

    constexpr void bump() noexcept {
      current_ += delta_;
      delta_ = 0;
    }

    constexpr size_t length() noexcept { return end_ - current_; }

    constexpr size_t get_pos() noexcept { return current_ - begin_; }

    constexpr void set_pos(size_t pos) noexcept {
      current_ = begin_ + pos;
      delta_ = 0;
    }

    constexpr void skip(size_t n) noexcept { delta_ += n; }

    constexpr void reset() noexcept { delta_ = 0; }

  private:
    size_t delta_ = 0;
    char *begin_{};
    char *current_{};
    char *end_{};
  };

  explicit Lexer(const std::string &name, char *begin, char *end)
      : name_{name}, location_{name_}, begin_{begin}, current_{begin},
        end_{end} {
    get_next_token();
  }

  explicit Lexer() noexcept : Lexer{"", nullptr, nullptr} {}

  void set_source(const std::string &name, char *begin, char *end) {
    name_ = name;
    location_ = Location{name_};
    begin_ = begin;
    current_ = begin;
    end_ = end;
    get_next_token();
  }

  const std::string to_string() noexcept {
    return std::visit(
        [](const auto &t) { return std::string(t.printable_name); }, token_);
  }

  template <typename TokenType> bool has_type() const noexcept {
    return std::holds_alternative<TokenType>(token_);
  }

  const Location &location() const noexcept { return location_; }

  bool at_end() const noexcept { return has_type<EndToken>(); }

  const Token &get() const noexcept { return token_; }

  void next() { get_next_token(); }

private:
  void get_next_token() {
    // Skip blanks
    while (current_ != end_) {
      char current_char = *current_;
      if (Traits::is_blank(current_char)) {
        location_.advance_column();
      } else if (Traits::is_newline(current_char)) {
        location_.advance_line();
      } else {
        break;
      }
      ++current_;
    }
    location_.step();

    token_ = ErrorToken{};

    if (current_ == end_) {
      token_ = EndToken{};
      return;
    }

    char *token_end = end_;
    if (Traits::end_at_newline || Traits::end_at_blank) {
      token_end = std::find_if(current_, end_, [](char c) {
        return (Traits::end_at_newline && Traits::is_newline(c)) ||
               (Traits::end_at_blank && Traits::is_blank(c));
      });
    }
    Provider provider{current_, token_end};
    auto result = Matcher::match(provider);
    auto next = current_ + (result.end - result.begin);
    std::visit([this, &next](auto &t) { apply(current_, next, t); },
               result.token);

    // If no rule matched and no rule accepts chars, emit an error
    if (result.begin == result.end) {
      std::stringstream ss;
      ss << "Could not match token: \'" << *current_ << '\'';

      current_ = end_;

      // Location is one off because of early exit
      throw LexerException(location_ + 1, std::move(ss).str());
    }
    token_ = std::move(result.token);
    while (current_ != next) {
      location_.advance_column();
      if (Traits::is_newline(*current_)) {
        location_.advance_line();
      }
      ++current_;
    }
  }

  Token token_{ErrorToken{}};
  std::string name_ = "";
  Location location_{};
  char *begin_ = nullptr;
  char *current_ = nullptr;
  char *end_ = nullptr;
};

} // namespace lexer

#endif /* end of include guard: LEXER_HPP */
