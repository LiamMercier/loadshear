#include "execution-plan.h"

#include "all-transports.h"
#include "wasm-message-handler.h"
#include "nop-message-handler.h"
#include "resolver.h"
#include "logger.h"

#include <wasmtime.hh>

#include <unordered_map>
#include <unordered_set>

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


        // Vector index for our std::pmr values.
        std::unordered_map<std::string, size_t> identity_map;

        // Duplicate detection based on file paths.
        std::unordered_set<std::filesystem::path> all_packets;

        // Reserve now, we have good estimates for how many elements we need.
        all_packets.reserve(settings.packet_identifiers.size());
        identity_map.reserve(settings.packet_identifiers.size());
        plan.packet_data.reserve(settings.packet_identifiers.size());

        // Read every packet we need into our arena allocator.
        for (const auto & [identifier, filename] : settings.packet_identifiers)
        {
            // Resolve the file
            std::string error_str;
            auto path = Resolver::resolve_file(filename, error_str);

            if (!error_str.empty())
            {
                std::string error_msg = "Failed to resolve "
                                        + filename
                                        + " (got error: "
                                        + error_str
                                        + ")";
                return std::unexpected(std::move(error_msg));
            }

            // If we have already read the packet, map the identifier
            // to this packet data.
            if (all_packets.contains(path))
            {
                auto dupe_iter = identity_map.find(filename);

                // Prevent running the application if we found a dupe
                // but also never read this into memory. This should
                // never happen so we should abort now.
                if (dupe_iter == identity_map.end())
                {
                    std::string error_msg = "Application identified a packet "
                                            "as a duplicate but no memory "
                                            "was allocated. This is an error "
                                            "with the application code.";
                    return std::unexpected{std::move(error_msg)};
                }

                // identity for packet -> index into vector we already read.
                identity_map[identifier] = dupe_iter->second;
                continue;
            }

            // If we haven't seen this packet, insert into the map,
            // read the packet into our vector, and set the index.
            all_packets.insert(path);

            size_t this_index = plan.packet_data.size();

            // We need to get enough space for this vector's data.
            uintmax_t file_size = Resolver::get_file_size(path);

            if (file_size == 0)
            {
                std::string error_msg = "File "
                                        + filename
                                        + " has zero bytes to read!";
                return std::unexpected{std::move(error_msg)};
            }

            if (file_size > static_cast<uintmax_t>
                            (std::numeric_limits<size_t>::max()))
            {
                std::string error_msg = "File "
                                        + filename
                                        + " is too large for your platform's "
                                          "memory (exceeds size_t)";
                return std::unexpected{std::move(error_msg)};
            }

            // Allocate memory in the pmr vector.
            std::pmr::vector<uint8_t> buf{memory};

            // If this fails, we were OOM.
            try
            {
                buf.resize(static_cast<size_t>(file_size));
            }
            catch (const std::exception & error)
            {
                std::string error_msg = "Failed to allocate memory ("
                                        + std::to_string(file_size)
                                        + " bytes) for packet file "
                                        + filename;
                return std::unexpected{std::move(error_msg)};
            }

            // Try to write to the contiguous buffer.
            std::string error_msg = Resolver::read_bytes_to_contiguous
                                        (
                                            path,
                                            {buf.data(), buf.size()}
                                        );

            if (!error_msg.empty())
            {
                std::string e_msg = "Failed to read data for packet "
                                    + path.string()
                                    + " (got error: "
                                    + error_msg
                                    + ")";
                return std::unexpected{std::move(e_msg)};
            }

            // Place this vector into our vector of packets.
            plan.packet_data.push_back(buf);

            // Save the packet ID to data mapping.
            identity_map[identifier] = this_index;
        }

        // // Go through the action data and prepare the payloads.
        for (const auto & action : script.orchestrator.actions)
        {
            // TODO:
        }

        return plan;
    }
    else
    {
        static_assert(always_false<Session>,
                      "generate_execution_plan() not implemented for this type!");
    }
}
