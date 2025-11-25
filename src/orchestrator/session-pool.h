#pragma once

#include <type_traits>
#include <cstddef>
#include <vector>
#include <boost/asio.hpp>

#include "session-config.h"

namespace asio = boost::asio;

template<typename Session>
class SessionPool
{
    // Assert that the session type implements the required API.
    static_assert(std::is_invocable_v<decltype(&Session::start),
                                      Session &,
                                      const typename Session::Endpoints &>,
                  "Session is missing start()");

    static_assert(std::is_invocable_v<decltype(&Session::stop),
                                      Session &>,
                  "Session is missing stop()");

    static_assert(std::is_invocable_v<decltype(&Session::stop),
                                      Session &>,
                  "Session is missing halt()");

public:
    SessionPool(asio::io_context & cntx,
                const SessionConfig & config)
    :cntx_(cntx),
    config_(config)
    {
    }

    // Prevent copy/move since it makes no sense.
    SessionPool(const SessionPool &) = delete;
    SessionPool & operator=(const SessionPool &) = delete;
    SessionPool(SessionPool &&) = delete;
    SessionPool & operator=(const SessionPool &&) = delete;

    // This should never be called before spinning down each session.
    ~SessionPool()
    {
        // Destroy each session.
        for (size_t i = 0; i < pool_size_; i++)
        {
            sessions_[i].~Session();
        }

        ::operator delete(sessions_);
    }

    template<typename... Args>
    void create_sessions(size_t session_count, Args&&... args)
    {
        // Prevent creating a new pool if one is in use.
        if (sessions_)
        {
            throw std::runtime_error("SessionPool called create_sessions twice! "
                                     "This is not supported!");
        }

        pool_size_ = session_count;

        // Try to allocate on the heap for session_count session objects.
        sessions_ = static_cast<Session*>(::operator new(sizeof(Session) * pool_size_));

        on_done_callback_ = [this](){ disconnect_callback(); };

        for (size_t i = 0; i < pool_size_; i++)
        {
            new (&sessions_[i]) Session(std::forward<Args>(args)..., on_done_callback_);
        }

        active_sessions_ = session_count;

    }

    void start_all_sessions(const Session::Endpoints & endpoints)
    {
        for (size_t i = 0; i < pool_size_; i++)
        {
            sessions_[i].start(endpoints);
        }
    }

    void stop_all_sessions()
    {
        for (size_t i = 0; i < pool_size_; i++)
        {
            sessions_[i].stop();
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
        }
    }

private:
    // Store a reference to the io context to give to new sessions.
    asio::io_context & cntx_;

    SessionConfig config_;
    Session::DisconnectCallback on_done_callback_;

    // std::vector does not allow us to store classes that are not movable and not copyable.
    //
    // We must carefully manage raw memory to get a contiguous layout of Sessions.
    size_t pool_size_{0};
    Session* sessions_{nullptr};

    // Count the number of active sessions.
    std::atomic<size_t> active_sessions_{0};

};
