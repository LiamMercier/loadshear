// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <type_traits>
#include <cstddef>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

#include <boost/asio.hpp>

#include "session-config.h"
#include "shard-metrics.h"

#ifdef DEV_BUILD
#include "logger.h"
#endif

namespace asio = boost::asio;

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
                ShardMetrics & shard_metrics,
                NotifyClosed notify_closed)
    :cntx_(cntx),
    metrics_(shard_metrics),
    config_(config),
    notify_closed_(notify_closed)
    {
    }

    // Prevent copy/move since it makes no sense.
    SessionPool(const SessionPool &) = delete;
    SessionPool & operator=(const SessionPool &) = delete;
    SessionPool(SessionPool &&) = delete;
    SessionPool & operator=(SessionPool &&) = delete;

    void shutdown()
    {
        bool expected = false;

        if (closed_.compare_exchange_strong(expected, true))
        {
            stop_all_sessions();

            if (active_sessions_ == 0)
            {
                notify_closed_();
            }
        }
    }

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
    void disconnect_callback()
    {
        active_sessions_ -= 1;

        if (active_sessions_ == 0 && closed_)
        {
            // Handle anything that should happen when all sessions are done.
            notify_closed_();
            return;
        }
    }

private:
    // Store a reference to the io context to give to new sessions.
    asio::io_context & cntx_;

    ShardMetrics & metrics_;
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
