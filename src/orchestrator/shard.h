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

namespace asio = boost::asio;

// TODO: consider all lifetime related issues that must be solved during destruction.

template<typename Session>
class Shard
{
public:
    using MessageHandlerFactory = std::function<std::unique_ptr<MessageHandler>()>;
    using NotifyShardClosed = std::function<void()>;

public:

    // TODO: implement NotifyClosed for SessionPool, currently no-op.
    Shard(std::shared_ptr<const PayloadManager> manager_ptr,
          MessageHandlerFactory handler_factory,
          SessionConfig config,
          HostInfo<Session> host_info_copy,
          NotifyShardClosed on_shard_closed)
    :work_guard_(asio::make_work_guard(cntx_)),
    running_(false),
    config_(config),
    session_pool_(cntx_, config_, [this](){ this->on_sessions_closed(); }),
    payload_manager_(std::move(manager_ptr)),
    handler_factory_(std::move(handler_factory)),
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

    // TODO: overload submit_work to take a list of work instead of many posts
    //       if we want to do the same thing many times.

    // The orchestrator calls stop if the shard is taking too long to shutdown.
    void stop()
    {
        bool expected = true;

        if (!running_.compare_exchange_strong(expected, false))
        {
            return;
        }

        asio::post(cntx_, [this](){
            session_pool_.stop_all_sessions();


        });

        // Reset work guard now.
        work_guard_.reset();
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
    }

private:
    void thread_entry()
    {
        try
        {
            // Make our message handler with whatever the Orchestrator decided we should use.
            message_handler_ = handler_factory_();

            // Start the thread's work loop.
            cntx_.run();
        }
        catch (const std::exception & error)
        {
            std::cerr << "Shard got exception: " << error.what() << "\n";
            work_guard_.reset();
            cntx_.stop();
            // TODO: callback that we're closing to orchestrator?
            return;
        }

        // Our io context ran out of work, we must be closing.
        if (on_shard_closed_)
        {
            on_shard_closed_();
        }
    }

    void handle_action(ActionDescriptor action)
    {
        switch (action.type)
        {
            case ActionType::CREATE:
            {
                // Create requested number of sessions.
                session_pool_.create_sessions(action.count,
                                              cntx_,
                                              config_,
                                              message_handler_,
                                              payload_manager_);
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
    void start_force_stop_timer()
    {
        auto timer = std::make_shared<asio::steady_timer>(cntx_);
        timer->expires_after(std::chrono::seconds(20));
        timer->async_wait(
            [this, timer](const boost::system::error_code & ec){
                if (!ec && !cntx_.stopped())
                {
                    cntx_.stop();
                }
        });
    }

    // Called by the session pool callback. Could be used to collect analytics.
    void on_sessions_closed()
    {
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
    bool sessions_closed_{false};

    //
    // Session specific members.
    //
    SessionConfig config_;
    SessionPool<Session> session_pool_;
    std::shared_ptr<const PayloadManager> payload_manager_;

    // We need a thread specific message handler depending on the user's settings.
    MessageHandlerFactory handler_factory_;
    std::unique_ptr<MessageHandler> message_handler_;

    // Orchestrator specific host information.
    HostInfo<Session> host_info_;
    uint32_t last_host_id{UINT32_MAX};

    // Orchestrator callback.
    NotifyShardClosed on_shard_closed_;
};
