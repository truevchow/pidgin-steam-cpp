#ifndef PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H
#define PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H

#include <string>
#include <optional>
#include <vector>
#include <memory>
#include "cppcoro/task.hpp"
#include "cppcoro/io_service.hpp"

namespace SteamClient {
    class AsyncClientWrapper {
        struct impl;
        std::unique_ptr<impl> pImpl;

        void _check_session_key();

    public:
        explicit AsyncClientWrapper(const std::string &address);

        ~AsyncClientWrapper();

        cppcoro::task<void> run_cq(cppcoro::io_service &ioService);

        void shutdown();

        cppcoro::task <AuthResponseState> authenticate(const std::string &username, const std::string &password,
                                                       const std::optional<std::string> &steamGuardCode);

        cppcoro::task <FriendsList> getFriendsList();

        cppcoro::task <std::vector<Message>>
        getMessages(const std::string &id, std::optional<int64_t> startTimestampNs = std::nullopt,
                    std::optional<int64_t> lastTimestampNs = std::nullopt);

        cppcoro::task <SendMessageCode> sendMessage(const std::string &id, const std::string &message);

        void resetSessionKey();

        bool shouldReset();

        bool isSessionKeySet();
    };
} // SteamClient

#endif //PIDGIN_STEAM_GRPC_CLIENT_WRAPPER_ASYNC_H
