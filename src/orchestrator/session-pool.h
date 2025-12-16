#pragma once

#include <type_traits>
#include <cstddef>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#include <boost/asio.hpp>

#include "session-config.h"

#ifdef DEV_BUILD
#include <iostream>
#endif

namespace asio = boost::asio;

// TODO: make this class lifetime safe
template<typename Session>
class SessionPool
{
    // Assert that the session type implements the required API.
    static_assert(std::is_invocable_v<decltype(&Session::start),
                                      Session &,
                                      const typename Session::Endpoints &>,
                  "Session is missing start()");

    static_assert(std::is_invocable_v<decltype(&Session::flood),
                                      Session &>,
                  "Session is missing flood()");

    static_assert(std::is_invocable_v<decltype(&Session::send),
                                      Session &,
                                      size_t>,
                  "Session is missing send()");

    static_assert(std::is_invocable_v<decltype(&Session::drain),
                                      Session &>,
                  "Session is missing drain()");

    static_assert(std::is_invocable_v<decltype(&Session::stop),
                                      Session &>,
                  "Session is missing stop()");

public:
    using NotifyClosed = std::function<void()>;

public:
    SessionPool(asio::io_context & cntx,
                const SessionConfig & config,
                NotifyClosed notify_closed)
    :cntx_(cntx),
    config_(config),
    notify_closed_(notify_closed)
    {
    }

    // Prevent copy/move since it makes no sense.
    SessionPool(const SessionPool &) = delete;
    SessionPool & operator=(const SessionPool &) = delete;
    SessionPool(SessionPool &&) = delete;
    SessionPool & operator=(SessionPool &&) = delete;

    template<typename... Args>
    bool create_sessions(size_t session_count, Args&&... args)
    {
        // Prevent creating a new pool if one is in use.
        if (!sessions_.empty())
        {
            return false;
        }

        // Create session_count new Session objects.
        sessions_.reserve(session_count);

        on_done_callback_ = [this](){ disconnect_callback(); };

        for (size_t i = 0; i < session_count; i++)
        {
            sessions_.emplace_back(
                std::make_shared<Session>(std::forward<Args>(args)...,
                                          on_done_callback_)
            );
        }

        return true;

    }

    // call start on index values [start, end)
    void start_sessions_range(const Session::Endpoints & endpoints,
                              size_t start,
                              size_t end)
    {
        if (closed_)
        {
            return;
        }

        active_sessions_ += end - start;

        for (size_t i = start; i < end; i++)
        {
            sessions_[i]->start(endpoints);
        }
    }

    // call send on index values [start, end)
    void send_sessions_range(size_t start, size_t end, size_t N)
    {
        if (closed_)
        {
            return;
        }

        for (size_t i = start; i < end; i++)
        {
            sessions_[i]->send(N);
        }
    }

    // call flood on index values [start, end)
    void flood_sessions_range(size_t start, size_t end)
    {
        if (closed_)
        {
            return;
        }

        for (size_t i = start; i < end; i++)
        {
            sessions_[i]->flood();
        }
    }

    // call drain on index values [start, end)
    void drain_sessions_range(size_t start, size_t end)
    {
        if (closed_)
        {
            return;
        }

        for (size_t i = start; i < end; i++)
        {
            sessions_[i]->drain();
        }
    }

    // call stop on index values [start, end)
    void stop_sessions_range(size_t start, size_t end)
    {
        if (closed_)
        {
            return;
        }

        for (size_t i = start; i < end; i++)
        {
            sessions_[i]->stop();
        }
    }

    void start_all_sessions(const Session::Endpoints & endpoints)
    {
        start_sessions_range(0, sessions_.size());
    }

    void stop_all_sessions()
    {
        stop_sessions_range(0, sessions_.size());
    }

    size_t active_sessions() const
    {
        return active_sessions_;
    }

private:

    // TODO: perhaps send back analytics here? But, if we want live analytics, need something else.
    void disconnect_callback()
    {
        active_sessions_ -= 1;

        if (active_sessions_ == 0)
        {
            // Handle anything that should happen when all sessions are done.
            //
            // This should not be synchronous with the callback.
            bool expected = false;
            if(closed_.compare_exchange_strong(expected, true))
            {
                notify_closed_();
                return;
            }


// Warn anyone who compiled a debug version of the code.
#ifdef DEV_BUILD
// TODO <feature>: replace with logger.
            else
            {
                std::cout << "SessionPool had two disconnect callbacks at 0 active sessions!\n";
            }
#endif


        }
    }

private:
    // Store a reference to the io context to give to new sessions.
    asio::io_context & cntx_;

    SessionConfig config_;
    Session::DisconnectCallback on_done_callback_;

    // TODO <optimization>: We are allocating the pointers contiguously, but not Session memory.
    //                      In theory, we should be able to allocate the Session objects and
    //                      then point to this, but we likely need a custom shared_ptr type.
    //
    //                      This shouldn't be an intrusive re-write if we need to do it later.
    std::vector<std::shared_ptr<Session>> sessions_;

    // Count the number of active sessions.
    std::atomic<size_t> active_sessions_{0};

    // Callback function for when all session's are closed.
    std::atomic<bool> closed_{false};
    NotifyClosed notify_closed_;
};
