#include <utility>
#include <optional>
#include <cstdlib>
#include <string>
#include "environ.h"

EnvVars::value_proxy::value_proxy(std::string key) : key(std::move(key)) {}

std::optional<std::string> EnvVars::value_proxy::operator()() const {
    return _get(key);
}

[[nodiscard]] std::string EnvVars::value_proxy::value() const {
    return operator()().value();
}

[[nodiscard]] bool EnvVars::value_proxy::has_value() const {
    return operator()().has_value();
}

EnvVars::value_proxy &EnvVars::value_proxy::operator=(const std::string &value) {
    _set(key, value);
    return *this;
}

EnvVars::value_proxy &EnvVars::value_proxy::operator=(const std::nullopt_t) {
    _unset(key);
    return *this;
}

EnvVars::value_proxy EnvVars::get(const std::string &key) {
    return value_proxy(key);
}

std::optional<std::string> EnvVars::_get(const std::string &key) {
    char *value = std::getenv(key.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

void EnvVars::_set(const std::string &key, const std::string &value) {
    setenv(key.c_str(), value.c_str(), 1);
}

void EnvVars::_unset(const std::string &key) {
    unsetenv(key.c_str());
}