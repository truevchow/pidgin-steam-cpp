#include "grpc_client_wrapper.h"
#include "environ.h"
#include <iostream>
#include <string>
#include <optional>

int main() {
    SteamClient::ClientWrapper client("localhost:8080");
    client.authenticate(EnvVars::get("STEAM_USERNAME").value(), EnvVars::get("STEAM_PASSWORD").value(), std::nullopt);
    auto friends = client.getFriendsList();
    for (auto &x: friends.buddies) {
        std::cout << x.nickname << " " << x.id << std::endl;
        auto messages = client.getMessages(x.id);
        for (auto &y: messages) {
            std::cout << y.senderId << " " << y.message << " " << y.timestamp_ns << std::endl;
        }
    }
    return 0;
}