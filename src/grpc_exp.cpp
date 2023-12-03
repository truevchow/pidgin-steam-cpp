//
// Created by vincent on 3/12/23.
//

#include <grpcpp/grpcpp.h>
#include "../protobufs/comm_protobufs/message.pb.h"
#include "../protobufs/comm_protobufs/message.grpc.pb.h"
#include "../protobufs/comm_protobufs/auth.pb.h"
#include "../protobufs/comm_protobufs/auth.grpc.pb.h"
//#include "grpc_exp.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <optional>
#include <vector>

class EnvVars {
    struct value_proxy {
        std::string key;
        value_proxy(const std::string& key) : key(key) {}
        operator std::optional<std::string>() const {
            return get(key);
        }
        value_proxy& operator=(const std::string& value) {
            set(key, value);
            return *this;
        }
        value_proxy& operator=(const std::nullopt_t) {
            unset(key);
            return *this;
        }
    };
public:
    static std::optional<std::string> get(const std::string& key) {
        char* value = std::getenv(key.c_str());
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    }
    static void set(const std::string& key, const std::string& value) {
        setenv(key.c_str(), value.c_str(), 1);
    }
    static void unset(const std::string& key) {
        unsetenv(key.c_str());
    }
};

struct Buddy {
    std::string nickname;
    std::string id;
};

struct Message {
    std::string senderId;
    std::string message;
    int64_t timestamp_ns;
};

struct ClientWrapper {
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<steam::AuthService::Stub> authStub;
    std::unique_ptr<steam::MessageService::Stub> messageStub;

    std::optional<std::string> sessionKey;

    explicit ClientWrapper(std::string url) {
        channel = grpc::CreateChannel(url, grpc::InsecureChannelCredentials());
        authStub = steam::AuthService::NewStub(channel);
        messageStub = steam::MessageService::NewStub(channel);
    }

    bool authenticate(std::string username, std::string password) {
        steam::AuthRequest request;
        request.set_username(username);
        request.set_password(password);

        steam::AuthResponse response;
        grpc::ClientContext context;
        grpc::Status status = authStub->Authenticate(&context, request, &response);
        if (!status.ok()) {
            std::cout << "Auth failed (gRPC failure)" << std::endl;
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
        if (!response.success()) {
            std::cout << "Auth failed (Steam failure): " << response.reasonstr() << std::endl;
            return false;
        }
        std::cout << "Auth successful" << std::endl;
        std::cout << "Session key " << response.sessionkey() << std::endl;
        sessionKey = response.sessionkey();
        return true;
    }

    std::vector<Buddy> getFriendsList() {
        steam::FriendsListRequest request;
        request.set_sessionkey(sessionKey.value());

        steam::FriendsListResponse response;
        grpc::ClientContext context;
        grpc::Status status = messageStub->GetFriendsList(&context, request, &response);
        if (!status.ok()) {
            std::cout << "GetFriendsList failed (gRPC failure)" << std::endl;
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return {};
        }
//        if (!response.success()) {
//            std::cout << "GetFriendsList failed (Steam failure): " << response.reasonstr() << std::endl;
//            return {};
//        }
        std::cout << "GetFriends successful" << std::endl;
        std::cout << "Friends: " << response.friends_size() << std::endl;
        std::vector<Buddy> friends;
        for (auto &x : response.friends()) {
            friends.push_back({x.name(), x.id()});
        }
        return friends;
    }

    std::vector<Message> getMessages(std::string id, std::optional<int64_t> lastTimestampNs = std::nullopt) {
        steam::PollRequest request;
        request.set_sessionkey(sessionKey.value());
        request.set_targetid(id);
        if (lastTimestampNs.has_value()) {
            int64_t &val = lastTimestampNs.value();
            auto* timestamp = new google::protobuf::Timestamp();
            timestamp->set_seconds(val / 1000000000LL);  // Convert nanoseconds to seconds
            timestamp->set_nanos(val % 1000000000LL);  // Get remaining nanoseconds
            request.set_allocated_lasttimestamp(timestamp);
        }
        grpc::ClientContext context;
        auto clientReader = messageStub->PollChatMessages(&context, request);
        std::vector<Message> messages;
        steam::ResponseMessage response;
        while (clientReader->Read(&response)) {
            Message message;
            message.senderId = response.senderid();  // TODO: send persona info for mapping
            message.message = response.message();
            message.timestamp_ns = response.timestamp().nanos();
            messages.push_back(message);
        }
        return messages;
    }
};

int main() {
    ClientWrapper client("localhost:8080");
    client.authenticate(EnvVars::get("STEAM_USERNAME").value(), EnvVars::get("STEAM_PASSWORD").value());
    auto friends = client.getFriendsList();
    for (auto &x : friends) {
        std::cout << x.nickname << " " << x.id << std::endl;
        auto messages = client.getMessages(x.id);
        for (auto &y : messages) {
            std::cout << y.senderId << " " << y.message << " " << y.timestamp_ns << std::endl;
        }
    }
    return 0;
}