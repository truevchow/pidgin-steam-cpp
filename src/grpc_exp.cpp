//
// Created by vincent on 5/12/23.
//

#include "grpc_client_wrapper.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <optional>
#include <utility>

class EnvVars {
    struct value_proxy {
        std::string key;

        explicit value_proxy(std::string key) : key(std::move(key)) {}

        std::optional<std::string> operator()() const {
            return _get(key);
        }

        [[nodiscard]] std::string value() const {
            return operator()().value();
        }

        [[nodiscard]] bool has_value() const {
            return operator()().has_value();
        }

        value_proxy &operator=(const std::string &value) {
            _set(key, value);
            return *this;
        }

        value_proxy &operator=(const std::nullopt_t) {
            _unset(key);
            return *this;
        }
    };

public:
    static value_proxy get(const std::string &key) {
        return value_proxy(key);
    }

    static std::optional<std::string> _get(const std::string &key) {
        char *value = std::getenv(key.c_str());
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    }

    static void _set(const std::string &key, const std::string &value) {
        setenv(key.c_str(), value.c_str(), 1);
    }

    static void _unset(const std::string &key) {
        unsetenv(key.c_str());
    }
};

int main() {
    SteamClient::ClientWrapper client("localhost:8080");
    client.authenticate(EnvVars::get("STEAM_USERNAME").value(), EnvVars::get("STEAM_PASSWORD").value(), std::nullopt);
    auto friends = client.getFriendsList();
    for (auto &x : friends.buddies) {
        std::cout << x.nickname << " " << x.id << std::endl;
        auto messages = client.getMessages(x.id);
        for (auto &y : messages) {
            std::cout << y.senderId << " " << y.message << " " << y.timestamp_ns << std::endl;
        }
    }
    return 0;
}