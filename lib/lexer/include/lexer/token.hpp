#ifndef TOKEN_HPP
#define TOKEN_HPP

namespace lexer {
  struct basic_token {};

  struct ErrorToken : basic_token {
    static constexpr auto printable_name = "<error>";
  };

  struct EndToken : basic_token {
    static constexpr auto printable_name = "<end>";
  };
}

#endif /* end of include guard: TOKEN_HPP */
