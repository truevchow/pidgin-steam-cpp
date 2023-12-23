#include "grpc_client_wrapper.h"
#include "grpc_client_wrapper_async.h"
#include "coro_utils.h"
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <thread>
#include "../protobufs/comm_protobufs/message.pb.h"
#include "../protobufs/comm_protobufs/message.grpc.pb.h"
#include "../protobufs/comm_protobufs/auth.pb.h"
#include "../protobufs/comm_protobufs/auth.grpc.pb.h"
#include "cppcoro/sync_wait.hpp"
#include "cppcoro/io_service.hpp"

namespace SteamClient {
    struct AsyncClientWrapper::impl {
        std::shared_ptr<grpc::Channel> channel;
        std::unique_ptr<steam::AuthService::Stub> authStub;
        std::unique_ptr<steam::MessageService::Stub> messageStub;

        grpc::CompletionQueue completionQueue;
        std::atomic<size_t> tagCounter{0};
        std::atomic<bool> _shutdown{false};
        std::map<size_t, TaskCompletionSource<bool>> callbacks;

        SteamClient::AuthResponseState lastAuthResponseState = AUTH_UNKNOWN_FAILURE;
        bool lastSuccessState = false;
        std::optional<std::string> sessionKey;

        explicit impl(const std::string &address) {
            channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
            authStub = steam::AuthService::NewStub(channel);
            messageStub = steam::MessageService::NewStub(channel);
        }

        void shutdown() {
            _shutdown = true;
            completionQueue.Shutdown();  // TODO: how to ensure no additional gRPC events are enqueued?
        }

        ~impl() = default;

        cppcoro::task<void> run_cq(cppcoro::io_service &ioService) {
            std::cout << "starting completion queue" << std::endl;
            void *tag;
            bool ok;
            while (true) {
                auto status = completionQueue.AsyncNext(&tag, &ok, gpr_time_0(GPR_CLOCK_REALTIME));
                if (status == grpc::CompletionQueue::SHUTDOWN) {
                    break;
                }
                if (status == grpc::CompletionQueue::TIMEOUT) {
                    co_await ioService.schedule_after(std::chrono::milliseconds(50));
                    continue;
                }
                // std::cout << "got completion queue event #" << tag << " => " << (ok ? "ok" : "not ok") << std::endl;
                auto &token = callbacks.at(reinterpret_cast<size_t>(tag));
                co_await token.sequenced_set_result(ok);
            }
            std::cout << "stopping completion queue" << std::endl;
            co_return;
        }

        template<typename Rpc, typename Response>
        cppcoro::task<bool> run_call(Rpc &rpc, Response &response, grpc::Status &status) {
            auto tag = tagCounter++;
            auto &token = callbacks.emplace(std::piecewise_construct, std::forward_as_tuple(tag),
                                            std::forward_as_tuple()).first->second;
            rpc->Finish(&response, &status, reinterpret_cast<void *>(tag));
            bool ok = co_await token.get_task();
            callbacks.erase(tag);
            co_return ok;
        }

        cppcoro::task<std::tuple<AuthResponseState, std::string>>
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

            grpc::ClientContext context;
            steam::AuthResponse response;
            auto rpc = authStub->AsyncAuthenticate(&context, request, &completionQueue);
            if (grpc::Status status; !co_await run_call(rpc, response, status) || !status.ok()) {
                std::cout << "Auth failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                co_return std::make_tuple(AUTH_UNKNOWN_FAILURE, "");
            }

            switch (response.reason()) {
                case steam::AuthResponse_AuthState_SUCCESS:
                    std::cout << "Auth successful" << std::endl;
                    std::cout << "Session key " << response.sessionkey() << std::endl;
                    co_return std::make_tuple(AUTH_SUCCESS, response.sessionkey());
                case steam::AuthResponse_AuthState_INVALID_CREDENTIALS:
                    std::cout << "Auth failed (invalid credentials)" << std::endl;
                    co_return std::make_tuple(AUTH_INVALID_CREDENTIALS, response.sessionkey());
                case steam::AuthResponse_AuthState_STEAM_GUARD_CODE_REQUEST:
                    std::cout << "Auth failed (pending Steam Guard code)" << std::endl;
                    co_return std::make_tuple(AUTH_PENDING_STEAM_GUARD_CODE, response.sessionkey());
                default:
                    std::cout << "Auth failed (unknown failure)" << std::endl;
                    co_return std::make_tuple(AUTH_UNKNOWN_FAILURE, response.sessionkey());
            }
        }

        cppcoro::task<AuthResponseState>
        authenticate(const std::string &username, const std::string &password,
                     const std::optional<std::string> &steamGuardCode) {
            auto [state, newSessionKey] = co_await _authenticate(username, password, steamGuardCode);
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
            co_return state;
        }

        cppcoro::task<FriendsList> getFriendsList() {
            steam::FriendsListRequest request;
            request.set_sessionkey(sessionKey.value());

            steam::FriendsListResponse response;
            grpc::ClientContext context;
            auto rpc = messageStub->AsyncGetFriendsList(&context, request, &completionQueue);
            if (grpc::Status status; !co_await run_call(rpc, response, status) || !status.ok()) {
                std::cout << "GetFriendsList failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                co_return FriendsList{std::nullopt, {}};
            }
            std::cout << "GetFriends successful" << std::endl;
            std::cout << "Friends: " << response.friends_size() << std::endl;
            std::vector<Buddy> friends;
            for (auto &x: response.friends()) {
                friends.emplace_back(x.name(), x.id(), (PersonaState) (int) x.personastate());
            }
            auto me = response.user();
            co_return FriendsList{std::optional<Buddy>({me.name(), me.id(), (PersonaState) (int) me.personastate()}),
                                  friends};
        }

        static google::protobuf::Timestamp *
        set_timestamp_protobuf(google::protobuf::Timestamp *timestamp, int64_t timestamp_ns) {
            timestamp->set_seconds(timestamp_ns / 1000000000LL);  // Convert nanoseconds to seconds
            timestamp->set_nanos((int32_t) (timestamp_ns % 1000000000LL));  // Get remaining nanoseconds
            return timestamp;
        }

        static google::protobuf::Timestamp *
        make_timestamp_protobuf(int64_t timestamp_ns) {
            auto *timestamp = new google::protobuf::Timestamp();
            return set_timestamp_protobuf(timestamp, timestamp_ns);
        }

        static int64_t to_timestamp_ns(const google::protobuf::Timestamp &timestamp) {
            return timestamp.seconds() * 1000000000LL + timestamp.nanos();
        }

        // TODO: handle streaming responses in completion queue thread
        cppcoro::task<std::vector<Message>>
        getMessages(const std::string &id, std::optional<int64_t> startTimestampNs = std::nullopt,
                    std::optional<int64_t> lastTimestampNs = std::nullopt) {
            steam::PollRequest request;
            request.set_sessionkey(sessionKey.value());
            request.set_targetid(id);
            if (startTimestampNs.has_value()) {
                request.set_allocated_starttimestamp(make_timestamp_protobuf(startTimestampNs.value()));
            }
            if (lastTimestampNs.has_value()) {
                request.set_allocated_lasttimestamp(make_timestamp_protobuf(lastTimestampNs.value()));
            }
            grpc::ClientContext context;
            grpc::Status status;
            auto tag = tagCounter++;
            auto stream = messageStub->AsyncPollChatMessages(&context, request, &completionQueue,
                                                             reinterpret_cast<void *>(tag));
            auto &token = callbacks.emplace(std::piecewise_construct, std::forward_as_tuple(tag),
                                            std::forward_as_tuple()).first->second;
            std::vector<Message> messages;
            if (co_await token.get_task()) {  // StartCall response
                while (true) {
                    steam::ResponseMessage response;
                    stream->Read(&response, reinterpret_cast<void *>(tag));
                    if (!co_await token.get_task()) {
                        break;
                    }

                    Message message;
                    message.senderId = response.senderid();  // TODO: send persona info for mapping
                    message.message = response.message();
                    message.timestamp_ns = to_timestamp_ns(response.timestamp());
                    std::cout << "message: " << message.senderId << " " << message.message << " "
                              << message.timestamp_ns << std::endl;
                    messages.push_back(message);
                }
            } else {
                stream->Finish(&status, reinterpret_cast<void *>(tag));
            }
            // TODO: check exception handling
            callbacks.erase(tag);
            co_return messages;
        }

        cppcoro::task<SendMessageCode> sendMessage(const std::string &id, const std::string &message) {
            steam::MessageRequest request;
            request.set_sessionkey(sessionKey.value());
            request.set_targetid(id);
            request.set_message(message);
            grpc::ClientContext context;
            steam::SendMessageResult response;
            auto rpc = messageStub->AsyncSendChatMessage(&context, request, &completionQueue);
            if (grpc::Status status; !co_await run_call(rpc, response, status) || !status.ok()) {
                std::cout << "SendMessage failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                co_return SEND_UNKNOWN_FAILURE;
            }
            switch (response.reason()) {
                case steam::SendMessageResult_SendMessageResultCode_SUCCESS:
                    std::cout << "SendMessage successful" << std::endl;
                    co_return SEND_SUCCESS;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_SESSION_KEY:
                    std::cout << "SendMessage failed (invalid session key)" << std::endl;
                    co_return SEND_INVALID_SESSION_KEY;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_TARGET_ID:
                    std::cout << "SendMessage failed (invalid target ID)" << std::endl;
                    co_return SEND_INVALID_TARGET_ID;
                case steam::SendMessageResult_SendMessageResultCode_INVALID_MESSAGE:
                    std::cout << "SendMessage failed (invalid message)" << std::endl;
                    co_return SEND_INVALID_MESSAGE;
                default:
                    std::cout << "SendMessage failed (unknown failure)" << std::endl;
                    co_return SEND_UNKNOWN_FAILURE;
            }
        }

        cppcoro::task<ActiveMessageSessions> getActiveMessageSessions(std::optional<int64_t> sinceTimestampMs) {
            steam::ActiveMessageSessionsRequest request;
            request.set_sessionkey(sessionKey.value());
            if (sinceTimestampMs.has_value()) {
                request.set_allocated_since(make_timestamp_protobuf(sinceTimestampMs.value()));
            }

            grpc::ClientContext context;
            steam::ActiveMessageSessionResponse response;
            auto rpc = messageStub->AsyncGetActiveFriendMessageSessions(&context, request, &completionQueue);
            if (grpc::Status status; !co_await run_call(rpc, response, status) || !status.ok()) {
                std::cout << "GetActiveMessageSessions failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                co_return ActiveMessageSessions{{}, std::nullopt};
            }

            std::vector<ActiveMessageSessions::Session> sessions;
            for (auto &x: response.sessions()) {
                ActiveMessageSessions::Session session;
                session.id = x.targetid();
                session.lastMessageTimestampNs = to_timestamp_ns(x.lastmessagetimestamp());
                session.lastViewedTimestampNs = to_timestamp_ns(x.lastviewtimestamp());
                session.unreadMessageCount = x.unreadcount();
                sessions.push_back(session);
            }

            co_return ActiveMessageSessions{sessions, {to_timestamp_ns(response.timestamp())}};
        }

        cppcoro::task<bool> ackFriendMessage(const std::string &id, int64_t timestampNs) {
            steam::AckFriendMessageRequest request;
            request.set_sessionkey(sessionKey.value());
            request.set_targetid(id);
            request.set_allocated_lasttimestamp(make_timestamp_protobuf(timestampNs));
            grpc::ClientContext context;
            google::protobuf::Empty response;
            auto rpc = messageStub->AsyncAckFriendMessage(&context, request, &completionQueue);
            if (grpc::Status status; !co_await run_call(rpc, response, status) || !status.ok()) {
                std::cout << "AckFriendMessage failed (gRPC failure)" << std::endl;
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                co_return false;
            }
            std::cout << "AckFriendMessage successful" << std::endl;
            co_return true;
        }
    };

    AsyncClientWrapper::~AsyncClientWrapper() = default;

    cppcoro::task<void> AsyncClientWrapper::run_cq(cppcoro::io_service &ioService) {
        return pImpl->run_cq(ioService);
    };

    void AsyncClientWrapper::shutdown() {
        return pImpl->shutdown();
    };

    cppcoro::task<AuthResponseState>
    AsyncClientWrapper::authenticate(const std::string &username, const std::string &password,
                                     const std::optional<std::string> &steamGuardCode) {
        return pImpl->authenticate(username, password, steamGuardCode);
    }

    cppcoro::task<FriendsList> AsyncClientWrapper::getFriendsList() {
        _check_session_key();
        return pImpl->getFriendsList();
    }

    cppcoro::task<std::vector<Message>>
    AsyncClientWrapper::getMessages(const std::string &id, std::optional<int64_t> startTimestampNs,
                                    std::optional<int64_t> lastTimestampNs) {
        _check_session_key();
        return pImpl->getMessages(id, startTimestampNs, lastTimestampNs);
    }

    cppcoro::task<SendMessageCode>
    AsyncClientWrapper::sendMessage(const std::string &id, const std::string &message) {
        _check_session_key();
        return pImpl->sendMessage(id, message);
    }

    cppcoro::task<ActiveMessageSessions>
    AsyncClientWrapper::getActiveMessageSessions(std::optional<int64_t> sinceTimestampMs) {
        return pImpl->getActiveMessageSessions(sinceTimestampMs);
    }

    cppcoro::task<bool> AsyncClientWrapper::ackFriendMessage(const std::string &id, int64_t timestampNs) {
        return pImpl->ackFriendMessage(id, timestampNs);
    }

    void AsyncClientWrapper::_check_session_key() {
        if (!isSessionKeySet()) {
            throw std::runtime_error("session key not set");
        }
    }

    void AsyncClientWrapper::resetSessionKey() {
        pImpl->sessionKey = std::nullopt;
    }

    bool AsyncClientWrapper::shouldReset() {
        return !pImpl->lastSuccessState;
    }

    bool AsyncClientWrapper::isSessionKeySet() {
        return pImpl->sessionKey.has_value();
    }

    AsyncClientWrapper::AsyncClientWrapper::AsyncClientWrapper(const std::string &address) {
        pImpl = std::make_unique<impl>(address);
    }
} // SteamClient