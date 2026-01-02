#include "execution-plan.h"

#include "all-transports.h"
#include "wasm-message-handler.h"
#include "nop-message-handler.h"
#include "resolver.h"
#include "logger.h"

#include <wasmtime.hh>

#include <unordered_map>
#include <unordered_set>

// To map valid timestamp format strings to their enum values.
const std::unordered_map<std::string, TimestampFormat> ts_format_lookup
{
    {"seconds", TimestampFormat::Seconds},
    {"milliseconds", TimestampFormat::Milliseconds},
    {"microseconds", TimestampFormat::Microseconds},
    {"nanoseconds", TimestampFormat::Nanoseconds}
};

template std::expected<ExecutionPlan<TCPSession>, std::string>
generate_execution_plan<TCPSession>(const DSLData &,
                                    std::pmr::memory_resource* memory);

// TODO <optimization>: If we see FLOOD called, we can stop adding SEND actions
//                      and just read the packets and adding descriptors, since
//                      the SEND action just tells session's they may send, and
//                      FLOOD already does this.
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
                                     settings.repeat,
                                     settings.packet_sample_rate);

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

        // Remove duplicate endpoints.
        std::unordered_set<typename Session::Endpoint> endpoint_dupes;

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
                if (endpoint_dupes.contains(entry))
                {
                    continue;
                }

                host_data.endpoints.push_back(entry.endpoint());
                endpoint_dupes.insert(entry);
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
                                        + path.string()
                                        + " has zero bytes to read!";
                return std::unexpected{std::move(error_msg)};
            }

            if (file_size > static_cast<uintmax_t>
                            (std::numeric_limits<size_t>::max()))
            {
                std::string error_msg = "File "
                                        + path.string()
                                        + " is too large for your platform's "
                                          "memory (exceeds size_t)";
                return std::unexpected{std::move(error_msg)};
            }

            // Allocate memory in the pmr vector.
            std::pmr::vector<uint8_t> buf{memory};

            // If this fails, we are OOM.
            try
            {
                buf.resize(static_cast<size_t>(file_size));
            }
            catch (const std::exception & error)
            {
                std::string error_msg = "Failed to allocate memory ("
                                        + std::to_string(file_size)
                                        + " bytes) for packet file "
                                        + path.string();
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

        // Store the current offset.
        uint64_t curr_offset = 0;

        // Go through the action data and prepare the payloads.
        //
        // Packet data MUST NOT be changed after this.
        for (const auto & action : script.orchestrator.actions)
        {
            ActionDescriptor desc;

            curr_offset += action.offset_ms;

            // For each of these besides SEND we simply create an action
            // for the orchestrator. For SEND, we also need to add a payload.
            switch (action.type)
            {
                case ActionType::CREATE:
                {
                    desc.make_create(0,
                                     action.range.second,
                                     curr_offset);
                    break;
                }
                case ActionType::CONNECT:
                {
                    desc.make_connect(action.range.start,
                                      action.range.second,
                                      curr_offset);
                    break;
                }
                case ActionType::SEND:
                {
                    desc.make_send(action.range.start,
                                   action.range.second,
                                   action.count,
                                   curr_offset);

                    // For SEND, we must setup a payload.
                    PayloadDescriptor payload;

                    // Resolve the packet ID to one of
                    // our arena allocated packets.
                    auto p_iter = identity_map.find(action.packet_identifier);

                    // If we can't find the index, there is something wrong,
                    // under normal operation the program should not do this.
                    if (p_iter == identity_map.end())
                    {
                        std::string e_msg = "Failed to map packet identity "
                                            + action.packet_identifier
                                            + " to a read packet. This could "
                                            + "be an error with the application.";
                        return std::unexpected{std::move(e_msg)};
                    }

                    size_t p_index = p_iter->second;
                    const auto & buf = plan.packet_data[p_index];

                    // Create the const span to the data. This packet
                    // MUST NOT be modified from now on.
                    payload.packet_data = {buf.data(), buf.size()};

                    size_t ts_idx = 0;
                    size_t c_idx = 0;
                    size_t data_index = 0;

                    // If we have no modifications, mark the payload
                    // as one static read and continue to the next action.
                    if (action.mod_order.empty())
                    {
                        PacketOperation identity;
                        identity.make_identity(buf.size());

                        payload.ops.push_back(std::move(identity));

                        // Set counter step as none for this payload.
                        //
                        // We store a counter per payload to allow O(1)
                        // contiguous access during runtime at the
                        // cost of a few bytes.
                        //
                        // These are cheap descriptors, so we just dupe them
                        // because even at 1 million payloads (gross misuse)
                        // we would only have ~230 MiB of data.
                        //
                        // Users can reduce this by simply enabling looping.
                        //
                        // See packets/payload-manager.cpp
                        for (size_t i = 0; i < action.count; i++)
                        {
                            plan.counter_steps.push_back(0);
                            plan.payloads.push_back(payload);
                        }
                        break;
                    }

                    // TODO <refactor>: turn this into a helper function.

                    // Fetch the operations to be applied for this payload.

                    // Right now, we only store the last counter's step size.
                    //
                    // TODO <feature>: replace this when we can store
                    //                 multiple counter steps.
                    size_t last_counter_step = 0;

                    for (const auto & mod : action.mod_order)
                    {
                        if (mod == ModificationType::Counter)
                        {
                            const auto & c_mod = action.counter_mods[c_idx];
                            c_idx++;

                            // First, insert the previous "identity" payload
                            // of all bytes between this mod and the last.
                            //
                            // This is the counter start index minus the
                            // previous index from other operations.
                            size_t prev_bytes = c_mod.counter_bytes.start
                                                - data_index;

                            if (prev_bytes > 0)
                            {
                                PacketOperation identity;
                                identity.make_identity(prev_bytes);

                                payload.ops.push_back(std::move(identity));
                                data_index += prev_bytes;
                            }

                            // Turn this counter into a packet operation.
                            PacketOperation counter;
                            counter.make_counter(c_mod.counter_bytes.second,
                                                 c_mod.little_endian);

                            // Push these operations and the counter step.
                            payload.ops.push_back(std::move(counter));

                            last_counter_step = c_mod.counter_step;

                            // Increment the index.
                            data_index += c_mod.counter_bytes.second;
                        }
                        else if (mod == ModificationType::Timestamp)
                        {
                            const auto & ts_mod = action.timestamp_mods[ts_idx];
                            ts_idx++;

                            size_t prev_bytes = ts_mod.timestamp_bytes.start
                                                - data_index;

                            if (prev_bytes > 0)
                            {
                                PacketOperation identity;
                                identity.make_identity(prev_bytes);

                                payload.ops.push_back(std::move(identity));
                                data_index += prev_bytes;
                            }

                            // We need to resolve the time format here.
                            //
                            // This should be valid from previous checks when
                            // parsing the script, but it is good to ensure
                            // correctness here as well.
                            auto format_iter = ts_format_lookup
                                                    .find(ts_mod.format_name);

                            if (format_iter == ts_format_lookup.end())
                            {
                                std::string e_msg = "Failed to resolve "
                                                    "timestamp format "
                                                    "for value "
                                                    + ts_mod.format_name
                                                    + " (this should have "
                                                    "been caught by the "
                                                    "DSL validator)";

                                return std::unexpected{e_msg};
                            }

                            TimestampFormat ts_format = format_iter->second;

                            PacketOperation timestamp;
                            timestamp.make_timestamp(ts_mod.timestamp_bytes.second,
                                                     ts_mod.little_endian,
                                                     ts_format);

                            payload.ops.push_back(std::move(timestamp));

                            // Increment the index.
                            data_index += ts_mod.timestamp_bytes.second;
                        }
                    }

                    // Insert the remaining bytes if any exist.
                    if (data_index < buf.size())
                    {
                        size_t remaining = buf.size() - data_index;

                        PacketOperation identity;
                        identity.make_identity(remaining);
                        payload.ops.push_back(std::move(identity));
                    }

                    // See prior discussion on why we duplicate here.
                    for (size_t i = 0; i < action.count; i++)
                    {
                        plan.counter_steps.push_back(last_counter_step);
                        plan.payloads.push_back(payload);
                    }

                    break;
                }
                case ActionType::FLOOD:
                {
                    desc.make_flood(action.range.start,
                                    action.range.second,
                                    curr_offset);
                    break;
                }
                case ActionType::DRAIN:
                {
                    desc.make_drain(action.range.start,
                                    action.range.second,
                                    action.count,
                                    curr_offset);
                    break;
                }
                case ActionType::DISCONNECT:
                {
                    desc.make_disconnect(action.range.start,
                                         action.range.second,
                                         curr_offset);

                    break;
                }
            }

            // Push the results of parsing this action.
            plan.actions.push_back(std::move(desc));
        }

        // All data should be pushed, we can start our TCP orchestrator now.
        return plan;
    }
    // TODO <feature>: other session types.
    else
    {
        static_assert(always_false<Session>,
                      "generate_execution_plan() not "
                      "implemented for this type!");
    }
}

template std::string ExecutionPlan<TCPSession>::dump_endpoint_list();

template<typename Session>
inline std::string ExecutionPlan<Session>::dump_endpoint_list()
{
    if constexpr (std::is_same_v<Session, TCPSession>)
    {
        std::string endpoint_list;

        for (const auto & ep : config.host_info.endpoints)
        {
            endpoint_list += "  - ";
            endpoint_list += ep.address().to_string();
            endpoint_list += "\n";
        }

        return endpoint_list;
    }
    else
    {
        static_assert(always_false<Session>,
                      "generate_execution_plan() not "
                      "implemented for this type!");
    }
}
