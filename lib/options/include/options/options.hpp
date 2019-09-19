#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "logging/logging.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace options {

class OptionException : public std::exception {
public:
  explicit OptionException(std::string message) noexcept
      : message_{std::move(message)} {}

  const char *what() const noexcept override { return message_.c_str(); }

private:
  std::string message_;
};

struct BaseOption {
  virtual ~BaseOption() = default;

  virtual void parse(std::string_view text) = 0;

  std::string name;
  std::optional<char> short_name;
  std::string description;
  std::string default_value;
  bool is_present = false;

protected:
  explicit BaseOption(std::string name, std::optional<char> short_name,
                      std::string description, std::string default_value)
      : name{std::move(name)}, short_name{short_name},
        description{std::move(description)}, default_value{
                                                 std::move(default_value)} {
    if (this->name.length() == 0) {
      throw OptionException{"Name must not be empty"};
    }
    if (this->name[0] == '-') {
      throw OptionException{"Name must not start with '-'"};
    }
  }

  BaseOption(const BaseOption &) = default;
  BaseOption(BaseOption &&) = default;
  BaseOption &operator=(const BaseOption &) = default;
  BaseOption &operator=(BaseOption &&) = default;
};

template <typename T> class Option : public BaseOption {
public:
  explicit Option(std::string name, std::optional<char> short_name,
                  std::string description, std::string default_value)
      : BaseOption{std::move(name), short_name, std::move(description),
                   std::move(default_value)} {}

  void parse(std::string_view value) override {
    if (is_present) {
      throw OptionException{"Option \'" + std::string{name} +
                            "\' already specified"};
    }
    std::stringstream ss;
    ss << value;
    ss >> t_;
    is_present = true;
  }

  [[nodiscard]] inline const T &get() const { return t_; }

private:
  T t_;
};

class Options {
public:
  explicit Options(std::string name) noexcept : name_{std::move(name)} {}

  template <typename T>
  void add_option(std::string name, char short_name,
                  std::string description = "",
                  std::string default_value = "") {
    auto option = std::make_unique<Option<T>>(std::move(name), short_name,
                                              std::move(description),
                                              std::move(default_value));
    options_.push_back(std::move(option));
  }

  template <typename T>
  void add_option(std::string name, std::string description = "",
                  std::string default_value = "") {
    auto option = std::make_unique<Option<T>>(std::move(name), std::nullopt,
                                              std::move(description),
                                              std::move(default_value));
    options_.push_back(std::move(option));
  }

  template <typename T>
  void add_positional_option(std::string name, std::string description = "",
                             std::string default_value = "") {
    auto option = std::make_unique<Option<T>>(std::move(name), std::nullopt,
                                              std::move(description),
                                              std::move(default_value));
    positional_options_.push_back(std::move(option));
  }

  void parse(int argc, char *const argv[]) const {
    size_t counter = 0;
    // Parse positional options
    for (; counter < positional_options_.size(); ++counter) {
      if (counter == static_cast<size_t>(argc - 1) ||
          argv[counter + 1][0] == '-') {
        throw OptionException{"Positional argument \'" +
                              positional_options_[counter]->name +
                              "\' required"};
      }
      positional_options_[counter]->parse(argv[counter + 1]);
    }
    while (counter < static_cast<size_t>(argc - 1)) {
      std::string_view arg{argv[counter + 1]};
      if (arg[0] != '-') {
        throw OptionException{"Expected \'-\' to indicate option"};
      }

      arg.remove_prefix(1);

      if (arg.length() == 0) {
        throw OptionException{"Single \'-\' found"};
      }

      auto match = options_.cbegin();

      if (arg[0] == '-') {
        // Single option
        arg.remove_prefix(1);
        match = std::find_if(
            options_.cbegin(), options_.cend(),
            [&arg](const auto &option) { return option->name == arg; });
        if (match == options_.cend()) {
          throw OptionException{"Could not match option \'--" +
                                std::string{arg} + "\'"};
        }
      } else {
        // Short options
        for (auto it = arg.cbegin(); it != arg.cend(); ++it) {
          match = std::find_if(options_.cbegin(), options_.cend(),
                               [&it](const auto &option) {
                                 if (option->short_name) {
                                   return *option->short_name == *it;
                                 }
                                 return false;
                               });
          if (match == options_.cend()) {
            throw OptionException{"Could not match short option \'-" +
                                  std::string({*it}) + "\'"};
          }
          if (it != arg.cend() - 1) {
            (*match)->parse((*match)->default_value);
          }
        }
      }
      ++counter;
      if (counter < static_cast<size_t>(argc - 1) &&
          argv[counter + 1][0] != '-') {
        (*match)->parse(argv[counter + 1]);
        ++counter;
      } else {
        (*match)->parse((*match)->default_value);
      }
    }
  }

  template <typename T>[[nodiscard]] T get(std::string_view name) const {
    return find_option_<T>(name).get();
  }

  template <typename T>
  [[nodiscard]] bool is_present(std::string_view name) const {
    return find_option_<T>(name).is_present;
  }

  void print_usage() const noexcept {
    std::cout << "Synopsis:\n\t" << name_ << " ";
    for (auto it = positional_options_.cbegin();
         it != positional_options_.cend(); ++it) {
      if (it != positional_options_.cbegin()) {
        std::cout << ' ';
      }
      std::cout << (*it)->name;
    }
    for (auto it = options_.cbegin(); it != options_.cend(); ++it) {
      if (it != positional_options_.cbegin()) {
        std::cout << ' ';
      }
      std::cout << "[--" << (*it)->name;
      if ((*it)->short_name) {
        std::cout << ", -" << *(*it)->short_name;
      }
      std::cout << ']';
    }
    std::cout << "\n\nOptions:\n\t";
    for (auto it = positional_options_.cbegin();
         it != positional_options_.cend(); ++it) {
      if (it != positional_options_.cbegin()) {
        std::cout << '\n' << '\t';
      }
      std::cout << (*it)->name << '\t' << '\t' << (*it)->description;
      if ((*it)->default_value != "") {
        std::cout << " (Default: \'" << (*it)->default_value << "\')";
      }
    }
    std::cout << '\n';
    for (auto it = options_.cbegin(); it != options_.cend(); ++it) {
      if (it != options_.cbegin() || !positional_options_.empty()) {
        std::cout << '\n' << '\t';
      }
      std::cout << "--" << (*it)->name << '\t' << '\t' << (*it)->description;
      if ((*it)->default_value != "") {
        std::cout << " (Default: \'" << (*it)->default_value << "\')";
      }
    }
    std::cout << std::endl;
  }

private:
  template <typename T>
  [[nodiscard]] const Option<T> &find_option_(std::string_view name) const {
    auto match = std::find_if(
        positional_options_.cbegin(), positional_options_.cend(),
        [&name](const auto &option) { return option->name == name; });
    if (match == positional_options_.cend()) {
      match = std::find_if(
          options_.cbegin(), options_.cend(),
          [&name](const auto &option) { return option->name == name; });
      if (match == options_.cend()) {
        throw OptionException{"No option \'" + std::string{name} + "\'"};
      }
    }
    return static_cast<const Option<T> &>(**match);
  }

  std::string name_;
  std::vector<std::unique_ptr<BaseOption>> options_;
  std::vector<std::unique_ptr<BaseOption>> positional_options_;
};

} // namespace options

#endif /* end of include guard: OPTIONS_HPP */
