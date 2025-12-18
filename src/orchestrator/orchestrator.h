#pragma once

#include "action-descriptor.h"
#include "orchestrator-config.h"
#include "shard.h"
#include "logger.h"

#include <utility>

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
    :work_guard_((asio::make_work_guard(cntx_))),
    dispatch_timer_(cntx_),
    actions_(std::move(actions)),
    config_(std::move(config)),
    payload_manager_(std::make_shared<PayloadManager>(payloads, counter_steps))
    {
        // Try to create shards.
        shards_.reserve(config_.shard_count);
        shard_ranges_.reserve(config_.shard_count);

        for (size_t i = 0; i < config_.shard_count; i++)
        {
            typename Shard<Session>::NotifyShardClosed on_shard_callback =
                [this](){
                    this->shard_exit_callback();
                };

            active_shards_ += 1;

            // make pointer to shard
            shards_.emplace_back(std::make_unique<Shard<Session>>
                                    (payload_manager_,
                                        config_.handler_factory_,
                                        config_.session_config,
                                        config_.host_info,
                                        on_shard_callback));
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

        // Begin dispatch loop.
        asio::post(cntx_, [this](){
            dispatch_pending_actions();
        });

        // Run io_context on this thread.
        cntx_.run();

        // We only get here after each shard has closed.

        for (auto & shard : shards_)
        {
            if (shard)
            {
                shard->join();
            }
        }
    }

private:

    // Dispatch actions and then schedule next action timer.
    void dispatch_pending_actions()
    {
        auto now = std::chrono::steady_clock::now();

        while (current_action_indx_ < actions_.size())
        {
            const auto & action = actions_[current_action_indx_];

            auto target_time = startup_time_ + action.offset;

            // If next action is in the future, schedule the timer.
            if (target_time > now)
            {
                schedule_next_action(target_time);
                return;
            }

            // If action is due now, process it immediately.
            distribute_action_to_shards(action);
            current_action_indx_++;
        }

        // If we get here, there are no more actions, thread will go idle.
        do_shutdown();
    }

    void schedule_next_action(std::chrono::steady_clock::time_point deadline)
    {
        dispatch_timer_.expires_at(deadline);
        dispatch_timer_.async_wait([this](const boost::system::error_code & ec){
            if (ec)
            {
                return;
            }

            dispatch_pending_actions();
        });
    }

    void distribute_action_to_shards(const ActionDescriptor & action)
    {
        // We expect one create to be called per set of actions.
        //
        // We compute the shard ranges on create by splitting N session's
        // that we are creating across K shards.
        if (action.type == ActionType::CREATE)
        {
            // Prepare the shard ranges.
            {
                // Defensive (prevent duplicate calls from causing errors, should never happen).
                shard_ranges_.clear();

                uint32_t N = action.count;
                uint32_t K = shards_.size();

                // Grab quotient and remainder of splitting N session's over K shards.
                uint32_t s_split_base = N / K;
                uint32_t s_split_rem = N % K;

                // Start at index 0 and keep incrementing.
                uint32_t start = 0;

                // Compute index pairs for K shards.
                for (uint32_t i = 0; i < K; i++)
                {
                    // the remainder is less than K, calculate if we should distribute
                    // some of the remainder to this shard.
                    uint32_t rem_distributed = (i < s_split_rem) ? 1 : 0;
                    uint32_t count = s_split_base + rem_distributed;

                    // Note: end is exclusive.
                    uint32_t end = start + count;

                    shard_ranges_.emplace_back(start, end);

                    start = end;
                }

                // Check we distributed the range properly.
                if (start != action.count)
                {
                    std::string e_msg = "Not all session index values "
                                        "were distributed! start: "
                                        + std::to_string(start)
                                        + " count: "
                                        + std::to_string(action.count);
                    Logger::warn(std::move(e_msg));
                }
            }

            // Now, send the actual creation calls like any other action.
        }

        // We take [sessions_start, sessions_end) and compute the overlapping
        // range for each shard K's range [shard_ranges_[k].first, shard_ranges_[k].second)
        for (size_t k = 0; k < shards_.size(); k++)
        {
            uint32_t intersect_lower = std::max(action.sessions_start,
                                                shard_ranges_[k].first);
            uint32_t intersect_upper = std::min(action.sessions_end,
                                                shard_ranges_[k].second);

            // No overlap.
            if (intersect_lower >= intersect_upper)
            {
                continue;
            }

            // Compute local offset using these intersections.
            uint32_t local_start = intersect_lower - shard_ranges_[k].first;
            uint32_t session_count = intersect_upper - intersect_lower;

            // Give modified action based on these bounds.
            ActionDescriptor shard_action = action;
            shard_action.sessions_start = local_start;
            shard_action.sessions_end = local_start + session_count;

            // Send to this shard, repeat.
            bool success = shards_[k]->submit_work(shard_action);

            if (!success)
            {
                std::string e_msg = "Tried to submit work to shard "
                                    + std::to_string(k)
                                    + " and failed!";
                Logger::warn(std::move(e_msg));
            }
        }
    }

    void shard_exit_callback()
    {
        // subtraction only happens after fetch.
        size_t remaining = active_shards_.fetch_sub(1) - 1;

        // if we are the last shard, exit.
        if (remaining == 0)
        {
            asio::post(cntx_, [this](){
                work_guard_.reset();

                boost::system::error_code ignored;
                dispatch_timer_.cancel(ignored);
            });
        }
    }

    void do_shutdown()
    {
        if (shutdown_)
        {
            return;
        }

        shutdown_ = true;

        Logger::info("All actions executed, program will spin down.");

        for (const auto & shard : shards_)
        {
            if (shard)
            {
                shard->stop();
            }
        }
    }

private:
    // io context, timer, timepoint for scheduling.
    asio::io_context cntx_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::steady_timer dispatch_timer_;
    std::chrono::steady_clock::time_point startup_time_;
    std::atomic<size_t> active_shards_{0};
    bool shutdown_{false};

    // Action loop and config for this class.
    std::vector<ActionDescriptor> actions_;
    size_t current_action_indx_{0};
    OrchestratorConfig<Session> config_;

    // Data for shards.
    std::shared_ptr<PayloadManager> payload_manager_;
    std::vector<std::unique_ptr<Shard<Session>>> shards_;
    // pairs of [start, end) ranges.
    std::vector<std::pair<uint32_t, uint32_t>> shard_ranges_;
};
