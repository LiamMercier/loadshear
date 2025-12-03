#pragma once

#include <thread>
#include <memory>

#include <boost/asio.hpp>

namespace asio = boost::asio;

// TODO: consider all lifetime related issues that must be solved during destruction.

template<typename Session>
class Shard
{
public:
    using MessageHandlerFactory = std::function<std::unique_ptr<MessageHandler()>;

public:

    Shard(std::shared_ptr<const PayloadManager> manager_ptr,
          MessageHandlerFactory handler_factory,
          HostInfo host_info_copy)
    :payload_manager_(std::move(manager_ptr)),
    work_guard_(asio::make_work_guard(cntx_)),
    running_(false),
    handler_factory_(std::move(handler_factory)),
    host_info_(std::move(host_info_copy))
    {

    }

    void start()
    {
        bool expected = false;
        if (running_.compare_exchange_strong(expected, true))
        {
            return;
        }

        // Start thread work.
        thread_ = std::thread([this]{
            thread_entry();
        });
    }

    bool submit_work()
    {
        if (!running_.load())
        {
            return false;
        }

        asio::post(cntx_, [this](){
            // TODO: action post logic
        })

        return true;
    }

    // TODO: overload submit_work to take a list of work instead of many posts
    //       if we want to do the same thing many times.

    void stop()
    {
        bool expected = true;

        if (!running_.compare_exchange_strong(expected, false))
        {
            return;
        }

        // Calls to stop the io_context are thread safe.
        cntx_.stop();

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
    // TODO: make this function try/catch the entire block for safety?
    void thread_entry()
    {
        try
        {
            message_handler_ = handler_factory_();
        }
        catch (const std::exception & error)
        {
            std::cerr << error.what() << "\n";
            return;
        }

        // Start the thread's work loop.
        cntx_.run();

        // TODO: consider what to do when we exit the loop.
    }

    // TODO: based on action
    void handle_action()
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

    //
    // Session specific members.
    //
    SessionPool<Session> session_pool_;
    std::shared_ptr<const PayloadManager> payload_manager_;

    // We need a thread specific message handler depending on the user's settings.
    std::unique_ptr<MessageHandler> message_handler_;
    MessageHandlerFactory handler_factory_;

    // TODO: decide if we need to convert this to a list, or just one HostInfo for each orchestrator
    //       that we spin up.
    HostInfo<Session> host_info_;
    uint32_t last_host_id{UINT32_MAX};
};
