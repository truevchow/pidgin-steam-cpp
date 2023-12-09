#ifndef PIDGIN_STEAM_ENVIRON_H
#define PIDGIN_STEAM_ENVIRON_H

#include <optional>
#include <string>

class EnvVars {
    struct value_proxy {
        std::string key;
        explicit value_proxy(std::string key);
        std::optional<std::string> operator()() const;
        [[nodiscard]] std::string value() const;
        [[nodiscard]] bool has_value() const;
        value_proxy &operator=(const std::string &value);
        value_proxy &operator=(const std::nullopt_t);
    };
public:
    static value_proxy get(const std::string &key);
    static std::optional<std::string> _get(const std::string &key);
    static void _set(const std::string &key, const std::string &value);
    static void _unset(const std::string &key);
};

#endif //PIDGIN_STEAM_ENVIRON_H
