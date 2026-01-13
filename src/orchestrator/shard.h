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

#include <thread>
#include <memory>
#include <functional>
#include <memory>

#include <boost/asio.hpp>

#include "session-pool.h"
#include "message-handler-interface.h"
#include "payload-manager.h"
#include "action-descriptor.h"
#include "host-info.h"
#include "logger.h"
#include "shard-metrics.h"

namespace asio = boost::asio;

template<typename Session>
class Shard
{
public:
    using MessageHandlerFactory = std::function<std::unique_ptr<MessageHandler>()>;
    using NotifyShardClosed = std::function<void()>;

    using NotifyMetricsWrite = std::function<void()>;

public:

    Shard(std::shared_ptr<const PayloadManager> manager_ptr,
          MessageHandlerFactory handler_factory,
          SessionConfig config,
          HostInfo<Session> host_info_copy,
          NotifyShardClosed on_shard_closed)
    :work_guard_(asio::make_work_guard(cntx_)),
    running_(false),
    stop_timer_(cntx_),
    handler_factory_(std::move(handler_factory)),
    payload_manager_(std::move(manager_ptr)),
    config_(config),
    session_pool_(cntx_,
                  config_,
                  metrics_,
                  [this](){ this->on_pool_closed(); }),
    host_info_(std::move(host_info_copy)),
    on_shard_closed_(std::move(on_shard_closed))
    {
    }

    void start()
    {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true))
        {
            return;
        }

        // Start thread work.
        thread_ = std::thread([this]{
            thread_entry();
        });
    }

    bool submit_work(ActionDescriptor action)
    {
        if (!running_.load(std::memory_order_acquire))
        {
            return false;
        }

        asio::post(cntx_, [this, action = std::move(action)](){
            handle_action(std::move(action));
        });

        return true;
    }

    // Schedule a write into the list of metrics for this shard.
    // This is decided on by the orchestrator.
    void schedule_metrics_pull(SnapshotList & shard_history,
                               NotifyMetricsWrite write_cb)
    {
        auto history_ptr = &shard_history;

        asio::post(cntx_,
            [this,
             history_ptr,
             cb = std::move(write_cb)]() mutable {

                record_metrics(*history_ptr);
                cb();
            });
    }

    // The orchestrator calls stop if the shard is taking too long to shutdown.
    void stop()
    {
        bool expected = true;

        if (!running_.compare_exchange_strong(expected, false))
        {
            return;
        }

        asio::post(cntx_, [this](){
            // Start force shutdown timer for if pool refuses to close everything.
            start_force_stop_timer(30*1000);

            session_pool_.shutdown();
        });
    }

    // For external use only. Do not call from the shard's thread (deadlock).
    void join()
    {
        // Orchestrator thread calls this thread to join it.
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    ~Shard()
    {
        stop();
        join();
    }

private:
    void thread_entry()
    {
        try
        {
            // Make our message handler with whatever the Orchestrator
            // decided we should use.
            message_handler_ = handler_factory_();

            // Start the thread's work loop.
            cntx_.run();
        }
        catch (const std::exception & error)
        {
            std::string e_msg = "Shard got exception: "
                                + std::string(error.what());

            Logger::warn(std::move(e_msg));

            work_guard_.reset();
            cntx_.stop();

            // We do not return so our orchestrator can hear the callback.
            //
            // Shard will be closed after this anyways.
        }

        // Our io context ran out of work, we must be closing.
        if (on_shard_closed_)
        {
            on_shard_closed_();
        }
#ifdef DEV_BUILD
        else
        {
            Logger::warn(std::move("Shard did not have callback on close!"));
        }
#endif
    }

    void handle_action(ActionDescriptor action)
    {
        switch (action.type)
        {
            case ActionType::CREATE:
            {
                // Create requested number of sessions.
                session_pool_.create_sessions(action.sessions_end
                                              - action.sessions_start,
                                              cntx_,
                                              config_,
                                              *message_handler_,
                                              *payload_manager_,
                                              metrics_);
                break;
            }
            case ActionType::CONNECT:
            {
                // Connect the requested range of sessions.
                session_pool_.start_sessions_range(host_info_.endpoints,
                                                   action.sessions_start,
                                                   action.sessions_end);
                break;
            }
            case ActionType::SEND:
            {
                session_pool_.send_sessions_range(action.sessions_start,
                                                  action.sessions_end,
                                                  action.count);
                break;
            }
            case ActionType::FLOOD:
            {
                session_pool_.flood_sessions_range(action.sessions_start,
                                                   action.sessions_end);
                break;
            }
            case ActionType::DRAIN:
            {
                session_pool_.drain_sessions_range(action.sessions_start,
                                                   action.sessions_end);
                break;
            }
            case ActionType::DISCONNECT:
            {
                session_pool_.stop_sessions_range(action.sessions_start,
                                                  action.sessions_end);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Prevent the shard from hanging if we can't close session's.
    void start_force_stop_timer(size_t timeout_ms)
    {
        try
        {
            stop_timer_.cancel();
        }
        catch (...)
        {
        }

        stop_timer_.expires_after(std::chrono::milliseconds(timeout_ms));
        stop_timer_.async_wait(
            [this](const boost::system::error_code & ec){
                if (!ec && !cntx_.stopped())
                {
                    Logger::warn(std::move("Shard shutdown timed out. "
                                           "Forcing shutdown."));
                    cntx_.stop();
                }
        });
    }

    // Called by the session pool callback. Could be used to collect analytics.
    void on_pool_closed()
    {
        try
        {
            stop_timer_.cancel();
        }
        catch (...)
        {
        }

        // Reset work guard now.
        work_guard_.reset();
    }

    // On this shard's thread, we write into the orchestrator's metric history.
    void record_metrics(SnapshotList & shard_history)
    {
        // Generate a snapshot from our metrics.
        MetricsSnapshot snapshot = metrics_.fetch_snapshot();

        // We also need to grab the current number of session's from the pool.
        snapshot.connected_sessions = session_pool_.active_sessions();

        shard_history.push_back(std::move(snapshot));
    }

private:

    //
    // Generic execution flow members for a shard.
    //
    asio::io_context cntx_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    // Shutdown
    asio::steady_timer stop_timer_;

    //
    // Session specific members.
    //

    // We need a thread specific message handler depending on the user's settings.
    MessageHandlerFactory handler_factory_;
    std::unique_ptr<MessageHandler> message_handler_;
    std::shared_ptr<const PayloadManager> payload_manager_;

    // We need to setup our metrics logging object.
    ShardMetrics metrics_;

    // SessionPool + configuration, holds every network related object.
    SessionConfig config_;
    SessionPool<Session> session_pool_;

    // Orchestrator specific host information. This can be simply endpoints,
    // or possibly TLS status, etc.
    HostInfo<Session> host_info_;

    // Orchestrator callback. We let the orchestrator know the shard is finished so it
    // can wake up and close itself if every other shard has closed.
    NotifyShardClosed on_shard_closed_;
};
