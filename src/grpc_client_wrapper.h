#ifndef PIDGIN_STEAM_GRPC_EXP_H
#define PIDGIN_STEAM_GRPC_EXP_H

#include <iostream>
#include <string>
#include <cstdlib>
#include <optional>
#include <vector>
#include <memory>

namespace SteamClient {
    enum PersonaState : int {
        OFFLINE = 0,
        ONLINE = 1,
        BUSY = 2,
        AWAY = 3,
        SNOOZE = 4,
        LOOKING_TO_TRADE = 5,
        LOOKING_TO_PLAY = 6,
        INVISIBLE = 7,
    };

    struct AvatarUrl {
        std::string icon;
        std::string medium;
        std::string full;
    };

    struct Buddy {
        std::string nickname;
        std::string id;
        PersonaState personaState;

        // rich presence
        std::optional<int> gameid;
        std::string gameExtraInfo;

        AvatarUrl avatarUrl;
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

    struct FriendsList {
        std::optional<Buddy> me;
        std::vector<Buddy> buddies;
    };

    struct ActiveMessageSessions {
        struct Session {
            std::string id;
            int64_t lastMessageTimestampNs;
            int64_t lastViewedTimestampNs;
            int unreadMessageCount;
        };
        std::vector<Session> session;
        std::optional<int64_t> timestamp;
    };

    class ClientWrapper {
        struct impl;
        std::unique_ptr<impl> pImpl;

    public:
        explicit ClientWrapper(const std::string &url);

        ~ClientWrapper();

        AuthResponseState authenticate(const std::string &username, const std::string &password,
                                       const std::optional<std::string> &steamGuardCode);

        FriendsList getFriendsList();

        std::vector<Message> getMessages(const std::string &id, std::optional<int64_t> startTimestampNs = std::nullopt,
                                         std::optional<int64_t> lastTimestampNs = std::nullopt);

        SendMessageCode sendMessage(const std::string &id, const std::string &message);

        ActiveMessageSessions getActiveMessageSessions(int64_t sinceTimestampMs);

        bool ackFriendMessage(const std::string &id, int64_t timestampNs);

        void resetSessionKey();

        bool shouldReset();
    };
}

#endif //PIDGIN_STEAM_GRPC_EXP_H
