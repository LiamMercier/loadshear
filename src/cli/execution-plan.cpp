#include "execution-plan.h"

#include "all-transports.h"
#include "wasm-message-handler.h"
#include "nop-message-handler.h"
#include "resolver.h"
#include "logger.h"

#include <wasmtime.hh>

template std::expected<ExecutionPlan<TCPSession>, std::string>
generate_execution_plan<TCPSession>(const DSLData &,
                                    std::pmr::memory_resource* memory);

template<typename Session>
std::expected<ExecutionPlan<Session>, std::string>
generate_execution_plan(const DSLData & script,
                        std::pmr::memory_resource* memory)
{
    const auto & settings = script.settings;

    // Handle generating plan for TCPSession execution.
    if constexpr (std::is_same_v<Session, TCPSession>)
    {
        using tcp = asio::ip::tcp;

        // Store session configuration.
        SessionConfig session_config(settings.header_size,
                                     settings.body_max,
                                     settings.read,
                                     settings.repeat);

        // Create the message handler factory.
        typename Shard<Session>::MessageHandlerFactory factory;

        if (!settings.read
            || settings.handler_value == "NOP")
        {
            factory = []() -> std::unique_ptr<MessageHandler>
            {
                return std::make_unique<NOPMessageHandler>();
            };
        }
        else if (settings.handler_value.ends_with(".wasm"))
        {
            // Create the WASM engine and module.
            wasmtime::Config WASM_config;
            auto engine = std::make_shared<wasmtime::Engine>(
                                std::move(WASM_config));

            std::vector<uint8_t> wasm_bytes;
            std::string error_msg;

            auto path = Resolver::resolve_file(settings.handler_value,
                                               error_msg);

            if (!error_msg.empty())
            {
                // If we could not resolve, stop now.
                return std::unexpected{error_msg};
            }

            wasm_bytes = Resolver::read_binary_file(path, error_msg);

            if (!error_msg.empty())
            {
                // If we could not read, stop now.
                return std::unexpected{error_msg};
            }

            auto module_tmp = wasmtime::Module::compile(*engine, wasm_bytes);

            if (!module_tmp)
            {
                // If we can't make the module, stop now.
                error_msg = "Failed to compile WASM module for file "
                            + path.string();
                return std::unexpected{error_msg};
            }

            auto wasm_module = std::make_shared<wasmtime::Module>(
                                            module_tmp.unwrap());

            factory = [engine, wasm_module]() -> std::unique_ptr<MessageHandler>
                      {
                          return std::make_unique<WASMMessageHandler>(engine,
                                                                      wasm_module);
                      };
        }

        // Get host data.
        HostInfo<Session> host_data;

        asio::io_context temp_cntx;

        tcp::resolver ip_resolver(temp_cntx);

        for (const auto & endpoint : settings.endpoints)
        {
            boost::system::error_code ec;

            auto results = ip_resolver.resolve(endpoint,
                                               std::to_string(settings.port),
                                               ec);

            if (ec)
            {
                // warn then continue
                std::string e_msg = endpoint
                                    + " could not be resolved (got error: "
                                    + ec.message()
                                    + ")";

                Logger::warn(std::move(e_msg));
            }

            // Store results in host info.
            for (const auto & entry : results)
            {
                host_data.endpoints.push_back(entry.endpoint());
            }
        }

        // Ensure we have endpoints.
        if (host_data.endpoints.empty())
        {
            std::string error_msg = "No endpoints could be resolved!";
            return std::unexpected{error_msg};
        }

        // Put this all into our plan's orchestrator config.
        ExecutionPlan<Session> plan
                        (
                            OrchestratorConfig<Session>
                                (session_config,
                                    host_data,
                                    factory,
                                    settings.shards),
                            std::pmr::vector<std::pmr::vector<uint8_t>>(memory)
                        );

        // Go through the action data and prepare the payloads.
        // TODO: later.

        return plan;
    }
    else
    {
        static_assert(always_false<Session>,
                      "generate_execution_plan() not implemented for this type!");
    }
}
