#ifndef OPTIONS_HPP
#define OPTIONS_HPP

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
  BaseOption(std::string name, std::optional<char> short_name,
             std::string description, std::string default_value)
      : name{std::move(name)}, short_name{short_name},
        description{std::move(description)}, default_value{
                                                 std::move(default_value)} {
    if (this->name.length() == 0) {
      throw OptionException("Name must not be empty");
    }
  }

  virtual ~BaseOption() {}
  virtual void parse(const std::string &text) = 0;

  std::string name;
  std::optional<char> short_name;
  std::string description;
  std::string default_value;

protected:
  BaseOption(const BaseOption &) = default;
  BaseOption(BaseOption &&) = default;
  BaseOption &operator=(const BaseOption &) = default;
  BaseOption &operator=(BaseOption &&) = default;
};

template <typename T> class Option : public BaseOption {
public:
  Option(std::string name, std::optional<char> short_name,
         std::string description, std::string default_value)
      : BaseOption{std::move(name), short_name, std::move(description),
                   std::move(default_value)} {}

  void parse(const std::string &value) override {
    std::stringstream ss{value};
    ss >> t_;
  }

  inline const T &get() const { return t_; }

private:
  T t_;
};

class Options {
public:
  template <typename T>
  void add_option(std::string name, std::optional<char> short_name,
                  std::string description = "",
                  std::string default_value = "") {
    auto option = std::make_unique<Option<T>>(std::move(name), short_name,
                                              std::move(description),
                                              std::move(default_value));
    options.push_back(std::move(option));
  }

  template <typename T>
  void add_option(std::string name, std::string description = "",
                  std::string default_value = "") {
    add_option<T>(std::move(name), std::nullopt, std::move(description),
                  std::move(default_value));
  }

  template <typename T>
  void add_positional_option(std::string name, std::string description = "",
                             std::string default_value = "") {
    auto option = std::make_unique<Option<T>>(std::move(name), std::nullopt,
                                              std::move(description),
                                              std::move(default_value));
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
      std::string arg{argv[counter]};
      if (arg[0] != '-') {
        throw OptionException{"Expected \'-\' to indicate option"};
      }
      if (arg.length() == 1) {
        throw OptionException{"Single \'-\' found"};
      }
      auto match = options.cbegin();
      if (arg[1] == '-') {
        // Single option
        match = std::find_if(options.cbegin(), options.cend(),
                             [&arg](const auto &option) {
                               return option->name == arg.substr(2);
                             });
        if (match == options.cend()) {
          throw OptionException{"Could not match option \"" + arg + "\""};
        }
      } else {
        // Short options
        for (auto it = arg.cbegin() + 1; it != arg.cend(); ++it) {
          match = std::find_if(options.cbegin(), options.cend(),
                               [&it](const auto &option) {
                                 if (option->short_name) {
                                   return *option->short_name == *it;
                                 }
                                 return false;
                               });
          if (match == options.cend()) {
            throw OptionException{"Could not match short option \"" +
                                  std::to_string(*it) + "\""};
          }
          if (it != arg.cend() - 1) {
            (*match)->parse((*match)->default_value);
          }
        }
      }
      if (counter + 1 < static_cast<unsigned int>(argc) &&
          argv[counter + 1][0] != '-') {
        ++counter;
        (*match)->parse(argv[counter]);
      } else {
        (*match)->parse((*match)->default_value);
      }
      ++counter;
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

#endif /* end of include guard: OPTIONS_HPP */
