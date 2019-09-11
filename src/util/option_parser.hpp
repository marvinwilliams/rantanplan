#ifndef OPTION_PARSER_HPP
#define OPTION_PARSER_HPP

#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace options {

class OptionException : public std::exception {
public:
  OptionException(const std::string &message) : message_{message} {}

  const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

struct BaseOption {
  std::string name;
  std::optional<char> short_name;
  std::string description;
  std::string default_value;

  virtual void parse(const std::string &text) = 0;

  virtual ~BaseOption() {}
};

template <typename T> struct Option : BaseOption {
  void parse(const std::string &test) override {
    std::stringstream ss{test};
    ss >> t;
  }
  const T &get() const { return t; }

  T t;
};

class Options {
public:
  explicit Options() {}

  template <typename T>
  void add_option(const std::string &name, const std::string &short_name = "",
                  const std::string &description = "",
                  const std::string &default_value = "") {
    auto option = std::make_unique<Option<T>>();
    if (name.length() == 0) {
      throw OptionException{"Option name cannot be empty"};
    }
    option->name = name;
    if (short_name.length() == 1) {
      option->short_name = short_name[0];
    } else if (short_name.length() > 1) {
      throw OptionException{"Short option " + short_name +
                            " has more than one character"};
    }
    option->description = description;
    option->default_value = default_value;
    options.push_back(std::move(option));
  }

  template <typename T>
  void add_positional_option(const std::string &name,
                             const std::string &description = "",
                             const std::string &default_value = "") {
    auto option = std::make_unique<Option<T>>();
    if (name.length() == 0) {
      throw OptionException{"Option name cannot be empty"};
    }
    option->name = name;
    option->description = description;
    option->default_value = default_value;
    positional_options.push_back(std::move(option));
  }

  void parse(int argc, char *argv[]) {
    unsigned int counter = 1;
    // Parse positional options
    while (counter < std::min(static_cast<size_t>(argc),
                              positional_options.size() + 1) &&
           argv[counter][0] != '-') {
      positional_options[static_cast<unsigned int>(counter - 1)]->parse(
          argv[counter]);
      ++counter;
    }
    while (counter < static_cast<unsigned int>(argc)) {
      if (argv[counter][0] != '-') {
        throw OptionException{"Option does not start with \'-\'"};
      }
      auto match = std::find_if(
          options.cbegin(), options.cend(),
          [&argv, &counter](const auto &option) {
            if (argv[counter][1] == '-') {
              return option->name == std::string{&argv[counter][2]};
            }
            if (!option->short_name) {
              return false;
            }
            return *(option->short_name) == argv[counter][1];
          });
      if (match == options.cend()) {
        throw OptionException{"Could not match option \"" +
                              std::string{argv[counter]} + "\"."};
      }
      ++counter;
      if (counter < static_cast<unsigned int>(argc) &&
          argv[counter][0] != '-') {
        (*match)->parse(argv[counter++]);
      } else {
        (*match)->parse((*match)->default_value);
      }
    }
  }

  template <typename T> T get(const std::string &name) const {
    auto match = std::find_if(
        positional_options.cbegin(), positional_options.cend(),
        [&name](const auto &option) { return option->name == name; });
    if (match == positional_options.cend()) {
      match = std::find_if(
          options.cbegin(), options.cend(),
          [&name](const auto &option) { return option->name == name; });
      if (match == options.cend()) {
        throw OptionException{"No option \"" + name + "\"."};
      }
    }
    return static_cast<Option<T> &>(**match).get();
  }

private:
  std::vector<std::unique_ptr<BaseOption>> options;
  std::vector<std::unique_ptr<BaseOption>> positional_options;
};

} // namespace options

#endif /* end of include guard: OPTION_PARSER_HPP */
