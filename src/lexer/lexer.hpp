#ifndef LEXER_HPP
#define LEXER_HPP

#include "lexer/lexer_exception.hpp"
#include "lexer/lexer_traits.hpp"
#include "lexer/location.hpp"
#include "lexer/rule_set.hpp"

#include <iterator>
#include <sstream>
#include <string>

namespace lexer {

template <typename Rules, typename CharT = char,
          typename Traits = LexerTraits<CharT>>
class Lexer {
public:
  using char_type = CharT;
  using Token = typename Rules::Token;

  // This iterator is used to extract the tokens. New tokens will be read when
  // incrementing the iterator.
  template <typename CharIterator> class TokenIterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Token;
    using difference_type = std::ptrdiff_t;
    using pointer = const Token *;
    using reference = const Token &;

    friend class Lexer;

    TokenIterator(Lexer *lexer, const CharIterator &from,
                  const CharIterator &to, const CharIterator &end,
                  const value_type &token, const Location &location)
        : lexer_{lexer}, from_{from}, to_{to}, end_{end}, token_{token},
          location_{location} {}

    TokenIterator &operator++() {
      lexer_->get_next_token(*this);
      return *this;
    }

    TokenIterator operator++(int) {
      TokenIterator old = *this;
      lexer_->get_next_token(*this);
      return old;
    }

    bool operator==(const TokenIterator &other) const {
      return lexer_ == other.lexer_ && from_ == other.from_ &&
             to_ == other.to_ && end_ == other.end_;
    }

    bool operator!=(const TokenIterator &other) const {
      return !(*this == other);
    }

    template <typename TokenType> bool has_type() {
      return std::holds_alternative<TokenType>(token_);
    }

    std::string to_string() {
      return std::visit([](auto &t) { return std::string(t.printable_name); },
                        token_);
    }

    reference token() const { return token_; }
    const Location &location() const { return location_; }
    reference operator*() const { return token(); }
    bool end() const { return from_ == end_; }

  private:
    Lexer *lexer_;
    CharIterator from_;
    CharIterator to_;
    const CharIterator end_;
    value_type token_;
    Location location_;
  };

  Lexer() {}

  template <typename CharIterator>
  TokenIterator<CharIterator> lex(const std::string &name, CharIterator begin,
                                  CharIterator end) {
    TokenIterator<CharIterator> iterator{this, begin,        begin,
                                         end,  ErrorToken{}, Location{name}};
    get_next_token(iterator);
    return iterator;
  }

private:
  template <typename CharIterator>
  void get_next_token(TokenIterator<CharIterator> &token_iterator) {
    // Skip blanks
    while (token_iterator.to_ != token_iterator.end_) {
      CharT current_char = *token_iterator.to_;
      if (Traits::is_blank(current_char)) {
        token_iterator.location_.advance_column();
      } else if (Traits::is_newline(current_char)) {
        token_iterator.location_.advance_line();
      } else {
        break;
      }
      token_iterator.to_++;
    }
    token_iterator.from_ = token_iterator.to_;
    token_iterator.location_.step();

    token_iterator.token_ = ErrorToken{};

    // Check if end is reached
    if (token_iterator.end()) {
      return;
    }

    std::basic_string<CharT> current_string;
    rules_.reset();
    CharIterator current_iterator = token_iterator.to_;
    Location current_location = token_iterator.location_;

    // Read the next char until it cannot be part of any token
    while (rules_.accepts() && current_iterator != token_iterator.end_) {
      CharT current_char = *current_iterator;
      if (Traits::end_at_newline && Traits::is_newline(current_char)) {
        break;
      }
      if (Traits::end_at_blank && Traits::is_blank(current_char)) {
        break;
      }
      rules_.next(current_char);
      current_string.push_back(current_char);
      current_location.advance_column();
      if (Traits::is_newline(current_char)) {
        current_location.advance_line();
      }
      if (!rules_.matching() && !rules_.accepts()) {
        break;
      }
      current_iterator++;
      if (rules_.matching()) {
        token_iterator.to_ = current_iterator;
        token_iterator.location_ = current_location;
        token_iterator.token_ = rules_.get_token(current_string);
      }
    }
    // If no rule matched and no rule accepts chars, emit an error
    if (std::holds_alternative<ErrorToken>(token_iterator.token_)) {
      token_iterator.from_ = token_iterator.end_;
      token_iterator.to_ = token_iterator.end_;
      token_iterator.location_ = current_location;
      std::basic_string<CharT> msg =
          "Could not match token: \"" + current_string + '\"';
      // Location is one off because of early exit
      throw LexerException(current_location, msg);
    }
  }

private:
  Rules rules_;
};

} // namespace lexer

#endif /* end of include guard: LEXER_HPP */
