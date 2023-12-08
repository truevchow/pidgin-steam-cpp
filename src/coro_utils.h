//
// Created by vincent on 25/8/23.
//

#ifndef PIDGIN_STEAM_CORO_UTILS_H
#define PIDGIN_STEAM_CORO_UTILS_H


#include <optional>
#include "cppcoro/task.hpp"
#include "cppcoro/async_auto_reset_event.hpp"

template<typename T>
class TaskCompletionSource {
public:
    TaskCompletionSource() = default;

    cppcoro::task<T> get_task() {
        co_await m_event;
        co_return std::move(*m_result);
    }

    void set_result(T result) {
        m_result = std::move(result);
        m_event.set();
    }

private:
    cppcoro::async_auto_reset_event m_event;
    std::optional<T> m_result;
};

#endif //PIDGIN_STEAM_CORO_UTILS_H
