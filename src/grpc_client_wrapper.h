//
// Created by vincent on 3/12/23.
//

#ifndef PIDGIN_STEAM_GRPC_EXP_H
#define PIDGIN_STEAM_GRPC_EXP_H

#include <iostream>
#include <string>
#include <cstdlib>
#include <optional>
#include <vector>
#include <memory>

namespace SteamClient {
    struct Buddy {
        std::string nickname;
        std::string id;
    };

    struct Message {
        std::string senderId;
        std::string message;
        int64_t timestamp_ns{};
    };

    class ClientWrapper {
        struct impl;
        std::unique_ptr<impl> pImpl;

    public:
        explicit ClientWrapper(std::string url);

        ~ClientWrapper();

        bool authenticate(std::string username, std::string password);

        std::vector<Buddy> getFriendsList();

        std::vector<Message> getMessages(std::string id, std::optional<int64_t> lastTimestampNs = std::nullopt);
    };
}

#endif //PIDGIN_STEAM_GRPC_EXP_H
