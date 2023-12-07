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

        // rich presence
        std::optional<int> gameid;
        std::string gameExtraInfo;
    };

    struct Message {
        std::string senderId;
        std::string message;
        int64_t timestamp_ns{};
    };

    enum AuthResponseState {
        AUTH_SUCCESS,
        AUTH_UNKNOWN_FAILURE,
        AUTH_INVALID_CREDENTIALS,
        AUTH_PENDING_STEAM_GUARD_CODE
    };

    enum SendMessageCode {
        SEND_SUCCESS,
        SEND_UNKNOWN_FAILURE,
        SEND_INVALID_SESSION_KEY,
        SEND_INVALID_TARGET_ID,
        SEND_INVALID_MESSAGE
    };

    class ClientWrapper {
        struct impl;
        std::unique_ptr<impl> pImpl;

    public:
        explicit ClientWrapper(std::string url);

        ~ClientWrapper();

        AuthResponseState authenticate(std::string username, std::string password, std::optional<std::string> steamGuardCode);

        std::vector<Buddy> getFriendsList();

        std::vector<Message> getMessages(std::string id, std::optional<int64_t> lastTimestampNs = std::nullopt);

        SendMessageCode sendMessage(std::string id, std::string message);

        void resetSessionKey();

        bool shouldReset();
    };
}

#endif //PIDGIN_STEAM_GRPC_EXP_H
