#pragma once

#include <type_traits>
#include <cstddef>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#include <boost/asio.hpp>

#include "session-config.h"

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
                  "Session is missing flood()");

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

    void start_all_sessions(const Session::Endpoints & endpoints)
    {
        for (size_t i = 0; i < sessions_.size(); i++)
        {
            sessions_[i]->start(endpoints);
        }

        active_sessions_ = sessions_.size();
    }

    void stop_all_sessions()
    {
        for (size_t i = 0; i < sessions_.size(); i++)
        {
            sessions_[i]->stop();
        }
    }

    size_t active_sessions() const
    {
        return active_sessions_;
    }

private:

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
                // TODO: post callback to controlling class.
                notify_closed_();
            }


// Warn anyone who compiled a debug version of the code.
#ifdef DEV_BUILD
// TODO <feature>: replace with logger.
#include <iostream>
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
