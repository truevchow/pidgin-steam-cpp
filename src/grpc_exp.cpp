#include "grpc_client_wrapper.h"
#include "environ.h"
#include "grpc_client_wrapper_async.h"
#include "cppcoro/sync_wait.hpp"
#include <iostream>
#include <string>
#include <optional>
#include <fstream>
#include <thread>
#include "json/json.h"
#include "cppcoro/cancellation_source.hpp"
#include "cppcoro/async_scope.hpp"
#include "cppcoro/when_all_ready.hpp"

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

struct Driver {
    cppcoro::io_service ioService;
    // cppcoro::async_scope scope;
    cppcoro::cancellation_source cancelTokenSource;
    cppcoro::cancellation_token cancelToken = cancelTokenSource.token();
};


cppcoro::task<void>
async_task(Driver &driver, SteamClient::AsyncClientWrapper &client, std::string username, std::string password) {
    cppcoro::io_work_scope ioScope(driver.ioService);

    std::cout << "async_task start" << std::endl;
    std::cout << "auth result: " << co_await client.authenticate(username, password, std::nullopt) << "\n";

    std::cout << "Get friends list" << std::endl;
    auto friends = co_await client.getFriendsList();
    std::cout << "Get all messages" << std::endl;
    for (auto &x: friends.buddies) {
        std::cout << x.nickname << " " << x.id << std::endl;
        std::vector<SteamClient::Message> messages = co_await client.getMessages(x.id);
        for (auto &y: messages) {
            std::cout << y.senderId << " " << y.message << " " << y.timestamp_ns << std::endl;
        }
    }

    std::cout << "Get active chat sessions" << std::endl;
    auto sessions = co_await client.getActiveMessageSessions();
    for (auto &x: sessions.session) {
        std::cout << x.id << " " << x.lastMessageTimestampNs << " " << x.lastViewedTimestampNs << " "
                  << x.unreadMessageCount << std::endl;
    }

    std::cout << "async_task shutdown" << std::endl;
    client.shutdown();
    driver.cancelTokenSource.request_cancellation();

    std::cout << "async_task end" << std::endl;
    co_return;
}

cppcoro::task<void> async_driver() {
    std::ifstream file("/tmp/credentials.json", std::ifstream::binary);
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(file, root)) {
        std::cout << "Failed to parse configuration\n" << reader.getFormattedErrorMessages();
        co_return;
    }

    SteamClient::AsyncClientWrapper client("localhost:8080");
    auto username = EnvVars::get("STEAM_USERNAME")().value_or(root["username"].asString());
    auto password = EnvVars::get("STEAM_PASSWORD")().value_or(root["password"].asString());
    std::cout << "username: " << username << std::endl;
    std::cout << "password: " << password << std::endl;
    std::cout << "async start" << std::endl;

    Driver driver;
    co_await cppcoro::when_all_ready(
            [&]() -> cppcoro::task<void> {
                cppcoro::io_work_scope ioScope(driver.ioService);
                co_await client.run_cq(driver.ioService);
                co_return;
            }(),
            async_task(driver, client, username, password),
            [&driver]() -> cppcoro::task<void> {
                driver.ioService.process_events();
                co_return;
            }());
    co_return;

    /*
    Driver driver;
    driver.scope.spawn(client.run_cq(driver.ioService));
    driver.scope.spawn([=, &driver, &client]() -> cppcoro::task<void> {
        co_await async_task(driver, client, username, password);
        std::cout << "async_task shutdown start" << std::endl;
        client.shutdown();
        driver.cancelTokenSource.request_cancellation();
        driver.ioService.stop();
        std::cout << "async_task end" << std::endl;
    }());
    std::thread thread([&]() {
        std::cout << "thread start" << std::endl;
        driver.ioService.process_events();
        std::cout << "thread end" << std::endl;
    });
    // cppcoro::sync_wait(std::move(async_task(driver, client, username, password)));
    std::cout << "Joining async scope" << std::endl;
    co_await driver.scope.join();
    std::cout << "async end" << std::endl;
    */
}

void async() {
    cppcoro::sync_wait(async_driver());
}

int main() {
    async();
    return 0;
}