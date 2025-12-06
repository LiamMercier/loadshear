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
    payload_manager_(std::make_shared<PayloadManager>(payloads, counter_steps))
    {
        try
        {
            // Try to create shards.
            shards_.reserve(config_.shard_count);

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

    void start_all_shards()
    {
        for (auto & shard : shards_)
        {
            if (shard)
            {
                shard->start();
            }
        }
    }

    // We are possibly closing early, try our best to close the shards.
    ~Orchestrator()
    {
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

private:
    // Action loop and config for this class.
    std::vector<ActionDescriptor> actions_;
    OrchestratorConfig<Session> config_;

    // Data for shards.
    std::shared_ptr<PayloadManager> payload_manager_;
    std::vector<std::unique_ptr<Shard<Session>>> shards_;

    // std::shared_ptr<wasmtime::Engine> wasm_engine_;
    // std::shared_ptr<wasmtime::Module> wasm_module_;
};
