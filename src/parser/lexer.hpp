#ifndef LEXER_HPP
#define LEXER_HPP

#include "lexer_exception.hpp"
#include "lexer_traits.hpp"
#include "location.hpp"
#include "source.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace lexer {

template <typename Rules, typename CharT = char,
          typename Traits = LexerTraits<CharT>>
class Lexer {
public:
  using char_type = CharT;

  explicit Lexer() {}

  template <typename Iterator>
  std::vector<typename Rules::return_type> lex(std::string &name,
                                               Iterator begin, Iterator end) {
    std::vector<typename Rules::return_type> tokens;
    std::vector<CharT> current_token;
    Location location(name);

    while (begin != end) {
      std::vector<CharT> current_token;
      rules.reset();
      while (begin != end) {
        CharT current_char = *begin;
        std::cout << "Current char: " << current_char << '\n';
        if (!rules.valid(current_token, current_char)) {
          break;
        }
        current_token.push_back(current_char);
        location.advance_column();
        if (Traits::is_newline(current_char)) {
          location.advance_line();
        }
        begin++;
      }
      if (current_token.empty()) {
        std::stringstream ss;
        ss << "Unexpected character: " << *begin;
        throw LexerException(location, ss.str());
      }
      if (begin == end) {
        rules.end();
      }
      std::cout << "Token complete!" << std::endl;
      tokens.push_back(rules.get_token(current_token, location - 1));
      location.step();
    }
    return tokens;
  }

private:
  Rules rules;
};

} // namespace lexer

#endif /* end of include guard: LEXER_HPP */
