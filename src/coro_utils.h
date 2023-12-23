#ifndef PIDGIN_STEAM_CORO_UTILS_H
#define PIDGIN_STEAM_CORO_UTILS_H


#include <optional>
#include "cppcoro/task.hpp"
#include "cppcoro/async_auto_reset_event.hpp"

template<typename T>
class TaskCompletionSource {
public:
    TaskCompletionSource() {
        m_result = std::nullopt;
    }

    TaskCompletionSource(TaskCompletionSource &) = delete;

    TaskCompletionSource(TaskCompletionSource &&) noexcept = default;

    TaskCompletionSource &operator=(TaskCompletionSource &) = delete;

    TaskCompletionSource &operator=(TaskCompletionSource &&) noexcept = default;

    cppcoro::task<T> get_task() {
        m_reset_event.set();
        co_await m_event;
        co_return std::move(*m_result);
    }

    void set_result(T result) {
        m_result = std::move(result);
        m_event.set();
    }

    cppcoro::task<void> sequenced_set_result(T result) {
        co_await m_reset_event;
        set_result(std::move(result));
        co_return;
    }

private:
    cppcoro::async_auto_reset_event m_event, m_reset_event;
    std::optional<T> m_result;
};

#endif //PIDGIN_STEAM_CORO_UTILS_H
