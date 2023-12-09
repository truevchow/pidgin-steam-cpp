#ifndef PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H
#define PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H

#include <string>
#include <optional>
#include <vector>
#include <memory>
#include "cppcoro/task.hpp"

namespace SteamClient {
    class grpc_client_wrapper_async {
        struct impl;
        std::unique_ptr<impl> pImpl;

    public:
        explicit grpc_client_wrapper_async(const std::string &address);

        ~grpc_client_wrapper_async();

        cppcoro::task <AuthResponseState> authenticate(const std::string &username, const std::string &password,
                                                       const std::optional<std::string> &steamGuardCode);

        cppcoro::task <FriendsList> getFriendsList();

        cppcoro::task <std::vector<Message>>
        getMessages(const std::string &id, std::optional<int64_t> startTimestampNs = std::nullopt,
                    std::optional<int64_t> lastTimestampNs = std::nullopt);

        cppcoro::task <SendMessageCode> sendMessage(const std::string &id, const std::string &message);

    };
} // SteamClient

#endif //PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H
