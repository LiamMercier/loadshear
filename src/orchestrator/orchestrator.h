#pragma once

#include "orchestrator-config.h"
#include "shard.h"

template<typename Session>
class Orchestrator
{
public:
    Orchestrator(std::vector<ActionDescriptor> actions,
                 std::vector<PayloadDescriptor> payloads,
                 std::vector<uint16_t> steps,
                 OrchestratorConfig<Session> config)
    :actions_(std::move(actions)),
    config_(std::move(config)),
    payload_manager_(std::make_shared<PayloadManager>(payloads, steps))
    {
        // Try to compile the module
        try
        {
            // If WASM enabled.
            if ()
            {
                wasm_engine_ = std::make_shared<wasmtime::Engine>(config_.wasm_config);

                auto module_tmp = wasmtime::Module::compile(*engine, wasm_bytes);

                if (!module_tmp)
                {
                    std::cerr << "Failed to construct orchestrator. Closing.\n";
                    return;
                }

                wasm_module_ = std::make_shared<wasmtime::Module>(module_tmp.unwrap());
            }

            // Try to create shards.
            shards_.reserve(config_.shard_count);

            for (size_t i = 0; i < config_.shard_count; i++)
            {
                Shard<Session>::MessageHandlerFactory msg_factory_;

                if ()
                {
                    // If we use WASM, vs if we use __ vs ?
                    msg_factory_ = [wasm_engine_, wasm_module_]() -> std::unique_ptr<MessageHandler>
                    {
                        // TODO:
                    };
                }

                // TODO: callback
                Shard<Session>::NotifyShardClosed on_shard_callback =
                    [](){

                    };

                // make pointer to shard
                shards_.emplace_back(std::make_unique<Shard<TCPSession>>
                                        (payload_manager,
                                         factory,
                                         config,
                                         host_info,
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

    std::shared_ptr<wasmtime::Engine> wasm_engine_;
    std::shared_ptr<wasmtime::Module> wasm_module_;
};
