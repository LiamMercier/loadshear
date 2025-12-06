#pragma once

#include "action-descriptor.h"
#include "orchestrator-config.h"
#include "shard.h"

template<typename Session>
class Orchestrator
{
public:

using MessageHandlerFactory = Shard<Session>::MessageHandlerFactory;
using NotifyShardClosed = Shard<Session>::NotifyShardClosed;

public:
    Orchestrator(std::vector<ActionDescriptor> actions,
                 std::vector<PayloadDescriptor> payloads,
                 std::vector<uint16_t> counter_steps,
                 OrchestratorConfig<Session> config)
    :actions_(std::move(actions)),
    config_(std::move(config)),
    payload_manager_(std::make_shared<PayloadManager>(payloads, counter_steps)),
    timer_(cntx_)
    {
        try
        {
            // Try to create shards.
            shards_.reserve(config_.shard_count);

            // TODO: we should precompute the session ranges for shards here.

            for (size_t i = 0; i < config_.shard_count; i++)
            {
                // TODO: callback
                typename Shard<Session>::NotifyShardClosed on_shard_callback =
                    [](){

                    };

                // make pointer to shard
                shards_.emplace_back(std::make_unique<Shard<TCPSession>>
                                        (payload_manager_,
                                         config_.handler_factory_,
                                         config_.session_config,
                                         config_.host_info,
                                         on_shard_callback));
            }

        }
        catch (const std::exception & error)
        {
            std::cerr << "Failed to construct orchestrator. Closing.\n";
            return;
        }
    }

    void start()
    {
        // Start all shards.
        for (auto & shard : shards_)
        {
            if (shard)
            {
                shard->start();
            }
        }

        // Initialize start time.
        startup_time_ = std::chrono::steady_clock::now();

        // Start action dispatch loop.
        schedule_next_action();

        // Run io_context on this thread.
        cntx_.run();
    }

    // We are possibly closing early, try our best to close the shards.
    //
    // We would prefer to not have this happen.
    ~Orchestrator()
    {
        dispatch_timer_.cancel();
        cntx_.stop();

        for (auto & shard : shards_)
        {
            if (shard)
            {
                shard->stop();
            }
        }

        for (auto & shard : shards_)
        {
            if (shard)
            {
                shard->join();
            }
        }
    }

private:

    void schedule_next_action()
    {
        if (current_action_indx_ >= actions_.size())
        {
            // TODO: schedule wait with condition variable for shards to finish draining/stop.
        }

        const auto & action = actions_[current_action_indx_];

        auto deadline = start_time_ + action.offset;

        dispatch_timer_.expires_at(deadline);
        dispatch_timer_.async_wait([this](const boost::system::error_code & ec){
            if (ec)
            {
                return;
            }

            dispatch_pending_actions();
        });
    }

    // Dispatch actions and then schedule next action timer.
    void dispatch_pending_actions()
    {

    }

private:
    // io context, timer, timepoint for scheduling.
    asio::io_context cntx_;
    asio::steady_timer dispatch_timer_;
    std::chrono::steady_clock::timepoint startup_time_;
    size_t current_action_indx_{0};

    // Action loop and config for this class.
    std::vector<ActionDescriptor> actions_;
    OrchestratorConfig<Session> config_;

    // Data for shards.
    std::shared_ptr<PayloadManager> payload_manager_;
    std::vector<std::unique_ptr<Shard<Session>>> shards_;
};
