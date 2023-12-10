#include "grpc_client_wrapper.h"
#include <grpcpp/grpcpp.h>

#include "../protobufs/comm_protobufs/message.pb.h"
#include "../protobufs/comm_protobufs/message.grpc.pb.h"
#include "../protobufs/comm_protobufs/auth.pb.h"
#include "../protobufs/comm_protobufs/auth.grpc.pb.h"

namespace SteamClient {
    struct ClientWrapper::impl {
        std::shared_ptr<grpc::Channel> channel;
        std::unique_ptr<steam::AuthService::Stub> authStub;
        std::unique_ptr<steam::MessageService::Stub> messageStub;

        SteamClient::AuthResponseState lastAuthResponseState = AUTH_UNKNOWN_FAILURE;
        bool lastSuccessState = false;
        std::optional<std::string> sessionKey;

        explicit impl(const std::string &url) {
            channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
            authStub = steam::AuthService::NewStub(channel);
            messageStub = steam::MessageService::NewStub(channel);
        }

        std::tuple<AuthResponseState, std::string>
        _authenticate(const std::string &username, const std::string &password,
                      const std::optional<std::string> &steamGuardCode) {
            steam::AuthRequest request;
            request.set_username(username);
            request.set_password(password);
            if (steamGuardCode.has_value()) {
                request.set_steamguardcode(steamGuardCode.value());
            }
            if (sessionKey.has_value()) {
                request.set_sessionkey(sessionKey.value());
            }

            steam::AuthResponse response;
            grpc::ClientContext context;
            grpc::Status status = authStub->Authenticate(&context, request, &response);
            if (!status.ok()) {
                std::cout << "Auth failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                return {AUTH_UNKNOWN_FAILURE, response.sessionkey()};
            }
            switch (response.reason()) {
                case steam::AuthResponse_AuthState_SUCCESS:
                    std::cout << "Auth successful" << std::endl;
                    std::cout << "Session key " << response.sessionkey() << std::endl;
                    return {AUTH_SUCCESS, response.sessionkey()};
                case steam::AuthResponse_AuthState_INVALID_CREDENTIALS:
                    std::cout << "Auth failed (invalid credentials)" << std::endl;
                    return {AUTH_INVALID_CREDENTIALS, response.sessionkey()};
                case steam::AuthResponse_AuthState_STEAM_GUARD_CODE_REQUEST:
                    std::cout << "Auth failed (pending Steam Guard code)" << std::endl;
                    return {AUTH_PENDING_STEAM_GUARD_CODE, response.sessionkey()};
                default:
                    std::cout << "Auth failed (unknown failure)" << std::endl;
                    return {AUTH_UNKNOWN_FAILURE, response.sessionkey()};
            }
        }

        AuthResponseState authenticate(const std::string &username, const std::string &password,
                                       const std::optional<std::string> &steamGuardCode) {
            auto [state, newSessionKey] = _authenticate(username, password, steamGuardCode);
            this->lastAuthResponseState = state;
            switch (state) {
                case AUTH_SUCCESS:
                    this->lastSuccessState = true;
                    this->sessionKey = newSessionKey;
                    break;
                case AUTH_INVALID_CREDENTIALS:
                    this->lastSuccessState = false;
                    this->sessionKey = std::nullopt;
                    break;
                case AUTH_PENDING_STEAM_GUARD_CODE:
                    this->lastSuccessState = true;
                    this->sessionKey = newSessionKey;
                    break;
                case AUTH_UNKNOWN_FAILURE:
                    this->lastSuccessState = false;
                    this->sessionKey = std::nullopt;
                    break;
            }
            return state;
        }

        FriendsList getFriendsList() {
            steam::FriendsListRequest request;
            request.set_sessionkey(sessionKey.value());

            steam::FriendsListResponse response;
            grpc::ClientContext context;
            grpc::Status status = messageStub->GetFriendsList(&context, request, &response);
            if (!status.ok()) {
                std::cout << "GetFriendsList failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                return {std::nullopt, {}};
            }
            std::cout << "GetFriends successful" << std::endl;
            std::cout << "Friends: " << response.friends_size() << std::endl;
            std::vector<Buddy> friends;
            for (auto &x: response.friends()) {
                friends.emplace_back(x.name(), x.id(), (PersonaState) (int) x.personastate());
            }
            auto me = response.user();
            return {std::optional<Buddy>({me.name(), me.id(), (PersonaState) (int) me.personastate()}), friends};
        }

        static google::protobuf::Timestamp *
        set_timestamp_protobuf(google::protobuf::Timestamp *timestamp, int64_t timestamp_ns) {
            timestamp->set_seconds(timestamp_ns / 1000000000LL);  // Convert nanoseconds to seconds
            timestamp->set_nanos((int32_t) (timestamp_ns % 1000000000LL));  // Get remaining nanoseconds
            return timestamp;
        }

        std::vector<Message> getMessages(const std::string &id, std::optional<int64_t> startTimestampNs = std::nullopt,
                                         std::optional<int64_t> lastTimestampNs = std::nullopt) {
            steam::PollRequest request;
            request.set_sessionkey(sessionKey.value());
            request.set_targetid(id);
            if (startTimestampNs.has_value()) {
                auto *timestamp = new google::protobuf::Timestamp();
                request.set_allocated_starttimestamp(set_timestamp_protobuf(timestamp, startTimestampNs.value()));
            }
            if (lastTimestampNs.has_value()) {
                auto *timestamp = new google::protobuf::Timestamp();
                request.set_allocated_lasttimestamp(set_timestamp_protobuf(timestamp, lastTimestampNs.value()));
            }
            grpc::ClientContext context;
            auto clientReader = messageStub->PollChatMessages(&context, request);
            std::vector<Message> messages;
            steam::ResponseMessage response;
            while (clientReader->Read(&response)) {
                Message message;
                message.senderId = response.senderid();  // TODO: send persona info for mapping
                message.message = response.message();
                message.timestamp_ns = response.timestamp().seconds() * 1000000000LL + response.timestamp().nanos();
                messages.push_back(message);
            }
            return messages;
        }

        SendMessageCode sendMessage(const std::string &id, const std::string &message) {
            steam::MessageRequest request;
            request.set_sessionkey(sessionKey.value());
            request.set_targetid(id);
            request.set_message(message);
            grpc::ClientContext context;
            steam::SendMessageResult response;
            grpc::Status status = messageStub->SendChatMessage(&context, request, &response);
            if (!status.ok()) {
                std::cout << "SendMessage failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                return SEND_UNKNOWN_FAILURE;
            }
            switch (response.reason()) {
                case steam::SendMessageResult_SendMessageResultCode_SUCCESS:
                    std::cout << "SendMessage successful" << std::endl;
                    return SEND_SUCCESS;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_SESSION_KEY:
                    std::cout << "SendMessage failed (invalid session key)" << std::endl;
                    return SEND_INVALID_SESSION_KEY;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_TARGET_ID:
                    std::cout << "SendMessage failed (invalid target ID)" << std::endl;
                    return SEND_INVALID_TARGET_ID;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_MESSAGE:
                    std::cout << "SendMessage failed (invalid message)" << std::endl;
                    return SEND_INVALID_MESSAGE;
                default:
                    std::cout << "SendMessage failed (unknown failure)" << std::endl;
                    return SEND_UNKNOWN_FAILURE;
            }
        }
    };

    ClientWrapper::~ClientWrapper() = default;

    ClientWrapper::ClientWrapper(const std::string &url) {
        pImpl = std::make_unique<impl>(url);
    }

    AuthResponseState ClientWrapper::authenticate(const std::string &username, const std::string &password,
                                                  const std::optional<std::string> &steamGuardCode) {
        return pImpl->authenticate(username, password, steamGuardCode);
    }

    FriendsList ClientWrapper::getFriendsList() {
        return pImpl->getFriendsList();
    }

    std::vector<Message> ClientWrapper::getMessages(const std::string &id, std::optional<int64_t> startTimestampNs,
                                                    std::optional<int64_t> lastTimestampNs) {
        return pImpl->getMessages(id, startTimestampNs, lastTimestampNs);
    }

    SendMessageCode ClientWrapper::sendMessage(const std::string &id, const std::string &message) {
        return pImpl->sendMessage(id, message);
    }

    void ClientWrapper::resetSessionKey() {
        pImpl->sessionKey = std::nullopt;
    }

    bool ClientWrapper::shouldReset() {
        return !pImpl->lastSuccessState;
    }
}