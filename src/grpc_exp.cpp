#include "grpc_client_wrapper.h"
#include "environ.h"
#include "grpc_client_wrapper_async.h"
#include "cppcoro/sync_wait.hpp"
#include <iostream>
#include <string>
#include <optional>
#include <fstream>
#include "json/json.h"

void sync() {
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
}

cppcoro::task<void>
async_task(SteamClient::grpc_client_wrapper_async &client, std::string username, std::string password) {
    std::cout << "async_task start" << std::endl;
    std::cout << "auth result: " << co_await client.authenticate(username, password, std::nullopt) << "\n";
    std::cout << "async_task end" << std::endl;
    auto friends = co_await client.getFriendsList();
    for (auto &x: friends.buddies) {
        std::cout << x.nickname << " " << x.id << std::endl;
        std::vector<SteamClient::Message> messages = co_await client.getMessages(x.id);
        for (auto &y: messages) {
            std::cout << y.senderId << " " << y.message << " " << y.timestamp_ns << std::endl;
        }
    }
    co_return;
}

void async() {
    std::ifstream file("/tmp/credentials.json", std::ifstream::binary);
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(file, root)) {
        std::cout << "Failed to parse configuration\n" << reader.getFormattedErrorMessages();
        return;
    }

    SteamClient::grpc_client_wrapper_async client("localhost:8080");
    auto username = EnvVars::get("STEAM_USERNAME")().value_or(root["username"].asString());
    auto password = EnvVars::get("STEAM_PASSWORD")().value_or(root["password"].asString());
    std::cout << "username: " << username << std::endl;
    std::cout << "password: " << password << std::endl;
    std::cout << "async start" << std::endl;
    cppcoro::sync_wait(std::move(async_task(client, username, password)));
}

int main() {
    async();
    return 0;
}