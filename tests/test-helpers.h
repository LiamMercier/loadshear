#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <boost/asio.hpp>

#include "script-structs.h"
#include "execution-plan.h"

namespace asio = boost::asio;

inline std::vector<uint8_t> read_binary_file(const std::string & path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("File " + path + " does not exist\n");
    }

    std::error_code ec;
    size_t filesize = static_cast<size_t>(std::filesystem::file_size(path, ec));

    if (ec)
    {
        throw std::runtime_error("Call file_size for " + path + " failed\n");
    }

    std::vector<uint8_t> buffer(filesize);

    if (!file.read(reinterpret_cast<char *>(buffer.data()), filesize))
    {
        throw std::runtime_error("File " + path + " read failed\n");
    }

    return buffer;
}

inline std::vector<uint8_t> slices_to_vector(const std::vector<boost::asio::const_buffer> & slices)
{
    std::vector<uint8_t> out;

    size_t total = 0;

    for (auto const & buffer : slices)
    {
        total += buffer.size();
    }

    out.reserve(total);

    for (const auto & buffer : slices)
    {
        const uint8_t *ptr = static_cast<const uint8_t*>(buffer.data());
        out.insert(out.end(), ptr, ptr + buffer.size());
    }

    return out;
}

inline std::string hexdump(const std::vector<uint8_t> & vec)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < vec.size(); i++)
    {
        oss << std::setw(2) << static_cast<int>(vec[i]);
        if (i + 1 < vec.size())
        {
            oss << ' ';
        }
    }

    return oss.str();
}

inline void EXPECT_VECTOR_EQ(const std::vector<uint8_t> & expected,
                      const std::vector<uint8_t> & actual)
{
    if (expected == actual)
    {
        return;
    }

    // Control failure message for bytes.
    ADD_FAILURE() << "Vectors differ! Expected: "
                  << hexdump(expected)
                  << "\n Actual: "
                  << hexdump(actual);
}

inline std::string action_type_to_string(ActionType a)
{
    switch (a)
    {
        case ActionType::CREATE:
        {
            return "CREATE";
        }
        case ActionType::CONNECT:
        {
            return "CONNECT";
        }
        case ActionType::SEND:
        {
            return "SEND";
        }
        case ActionType::FLOOD:
        {
            return "FLOOD";
        }
        case ActionType::DRAIN:
        {
            return "DRAIN";
        }
        case ActionType::DISCONNECT:
        {
            return "DISCONNECT";
        }
        default:
        {
            return "Error";
        }
    }
}

inline std::string dump_DSL_data(const DSLData & data)
{
    std::ostringstream data_str;

    std::string endpoints_list;
    std::string packet_ids;

    endpoints_list += "[";

    for (const auto & ep : data.settings.endpoints)
    {
        endpoints_list += ep;
        endpoints_list += " ";
    }

    endpoints_list += "]";

    packet_ids += "[";

    for (const auto & mapping : data.settings.packet_identifiers)
    {
        packet_ids += " {";
        packet_ids += mapping.first;
        packet_ids += "->";
        packet_ids += mapping.second;
        packet_ids += "} ";
    }

    packet_ids += "]";

    std::string data_actions;

    data_actions += "{ ";

    // Gather actions list
    for (const auto & action : data.orchestrator.actions)
    {
        data_actions += "{ ";
        data_actions += "Type: ";
        data_actions += action_type_to_string(action.type);
        data_actions += " count: ";
        data_actions += std::to_string(+action.count);
        data_actions += " range: {";
        data_actions += std::to_string(+action.range.start);
        data_actions += " ";
        data_actions += std::to_string(+action.range.second);
        data_actions += "} offset: ";
        data_actions += std::to_string(+action.offset_ms);
        data_actions += " ";

        if (action.type == ActionType::SEND)
        {
            data_actions += "packet_id: ";
            data_actions += action.packet_identifier;

            data_actions += " timestamps: [";

            std::string time_modstring;
            for (const auto & time_mod : action.timestamp_mods)
            {
                time_modstring += " {";
                time_modstring += std::to_string(+time_mod.timestamp_bytes.start);
                time_modstring += " ";
                time_modstring += std::to_string(+time_mod.timestamp_bytes.second);
                time_modstring += "} ";
            }

            data_actions += time_modstring;
            data_actions += "] ";

            data_actions += " counters: [";

            std::string counter_modstring;
            for (const auto & counter_mod : action.counter_mods)
            {
                counter_modstring += " {";
                counter_modstring += std::to_string(+counter_mod.counter_bytes.start);
                counter_modstring += " ";
                counter_modstring += std::to_string(+counter_mod.counter_bytes.second);
                counter_modstring += "} ";
            }

            data_actions += counter_modstring;
            data_actions += "] ";

            data_actions += " order: [ ";

            for (const auto & i : action.mod_order)
            {
                data_actions += std::to_string(+static_cast<uint8_t>(i));
                data_actions += " ";
            }

            data_actions += "] \n";
        }

        data_actions += "} ";

    }

    data_str << "SETTINGS : {\n"
             << "id: " << data.settings.identifier << " "
             << "SESSION: " << data.settings.session_protocol << "\n"
             << "PORT: " << data.settings.port << "\n"
             << "HEADERSIZE: " << data.settings.header_size << "\n"
             << "BODYMAX: " << data.settings.body_max << " "
             << "READ: " << data.settings.read << " "
             << "REPEAT: " << data.settings.repeat << "\n"
             << "SHARDS: " << data.settings.shards << " "
             << "HANDLER: " << data.settings.handler_value << " "
             << "ENDPOINTS: " << endpoints_list << "\n"
             << "PACKETS: " << packet_ids
             << "\n} \n"
             << "ORCHESTRATOR : {\n"
             << "id: " << data.orchestrator.settings_identifier << " "
             << "Actions: [" << data_actions << "] "
             << " \n}";

    return data_str.str();
}

inline bool actions_equal(Action a, Action b)
{
    // Go through the action fields and compare.
    if (a.type != b.type)
    {
        return false;
    }
    else if (a.range != b.range)
    {
        return false;
    }
    else if (a.offset_ms != b.offset_ms)
    {
        return false;
    }

    // Check count if relevant.
    if (a.type == ActionType::CREATE
        || a.type == ActionType::SEND
        || a.type == ActionType::DRAIN)
    {
        if (a.count != b.count)
        {
            return false;
        }
    }

    if (a.type == ActionType::SEND)
    {
        if (a.packet_identifier != b.packet_identifier)
        {
            return false;
        }

        // Compare vectors.
        if (a.timestamp_mods != b.timestamp_mods)
        {
            return false;
        }

        if (a.counter_mods != b.counter_mods)
        {
            return false;
        }

        if (a.mod_order != b.mod_order)
        {
            return false;
        }
    }

    return true;
}

inline void EXPECT_DSL_EQ(const DSLData & expected,
                          const DSLData & actual)
{
    bool fail = false;

    if (expected.settings.identifier
        != actual.settings.identifier)
    {
        fail = true;
    }
    else if (expected.settings.session_protocol
             != actual.settings.session_protocol)
    {
        fail = true;
    }
    else if (expected.settings.port
             != actual.settings.port)
    {
        fail = true;
    }
    else if (expected.settings.header_size
             != actual.settings.header_size)
    {
        fail = true;
    }
    else if (expected.settings.body_max
             != actual.settings.body_max)
    {
        fail = true;
    }
    else if (expected.settings.read
             != actual.settings.read)
    {
        fail = true;
    }
    else if (expected.settings.repeat
             != actual.settings.repeat)
    {
        fail = true;
    }
    else if (expected.settings.shards
             != actual.settings.shards)
    {
        fail = true;
    }
    else if (expected.settings.handler_value
             != actual.settings.handler_value)
    {
        fail = true;
    }
    else if (expected.settings.packet_identifiers != actual.settings.packet_identifiers)
    {
        fail = true;
    }

    // Terrible runtime, but at this scale it's essentially meaningless.
    for (const auto & inner_ep : expected.settings.endpoints)
    {
        bool found = false;

        for (const auto & outer_ep : actual.settings.endpoints)
        {
            if (outer_ep == inner_ep)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            fail = true;
            break;
        }
    }

    // Compare the orchestrator fields.
    if (expected.orchestrator.settings_identifier
        != actual.orchestrator.settings_identifier)
    {
        fail = true;
    }

    if (expected.orchestrator.actions.size()
        != actual.orchestrator.actions.size())
    {
        fail = true;
    }
    else
    {
        for (size_t i = 0; i < expected.orchestrator.actions.size(); i++)
        {
            if (!actions_equal(expected.orchestrator.actions[i],
                               actual.orchestrator.actions[i]))
            {
                fail = true;
                break;
            }
        }
    }

    if (fail)
    {
        // Control failure message for bytes.
        ADD_FAILURE() << "DSL data differs!\n\nExpected: "
                      << dump_DSL_data(expected)
                      << "\n\nActual: "
                      << dump_DSL_data(actual);
    }
}

inline std::string op_type_to_string(PacketOperationType type)
{
    switch (type)
    {
        case PacketOperationType::IDENTITY:
        {
            return "IDENTITY";
        }
        case PacketOperationType::COUNTER:
        {
            return "COUNTER";
        }
        case PacketOperationType::TIMESTAMP:
        {
            return "TIMESTAMP";
        }
        default:
        {
            return "Error";
        }
    }
}

inline std::string time_format_to_string(TimestampFormat format)
{
    switch (format)
    {
        case TimestampFormat::Seconds:
        {
            return "Seconds";
        }
        case TimestampFormat::Milliseconds:
        {
            return "Milliseconds";
        }
        case TimestampFormat::Microseconds:
        {
            return "Microseconds";
        }
        case TimestampFormat::Nanoseconds:
        {
            return "Nanoseconds";
        }
        default:
        {
            return "Error";
        }
    }
}

template<typename Session>
inline std::vector<std::string>
EXPECT_PLAN_EQ(const ExecutionPlan<Session> & expected,
               const ExecutionPlan<Session> & actual)
{
    std::vector<std::string> issues;

    const auto & e_config = expected.config;
    const auto & a_config = actual.config;

    // First, compare shard count.
    if (e_config.shard_count != a_config.shard_count)
    {
        std::string issue = "Shard counts differ! Expected: "
                            + std::to_string(e_config.shard_count)
                            + " Actual: "
                            + std::to_string(a_config.shard_count);
        issues.push_back(issue);
    }

    // Compare session configs.
    const auto & e_config_s = e_config.session_config;
    const auto & a_config_s = a_config.session_config;

    if (e_config_s.header_size != a_config_s.header_size
        ||  e_config_s.payload_size_limit != a_config_s.payload_size_limit
        ||  e_config_s.read_messages != a_config_s.read_messages
        ||  e_config_s.loop_payloads != a_config_s.loop_payloads)
    {
        std::string issue = "Session configs differ! Expected: {"
                            + std::to_string(e_config_s.header_size)
                            + " "
                            + std::to_string(e_config_s.payload_size_limit)
                            + " "
                            + std::to_string(e_config_s.read_messages)
                            + " "
                            + std::to_string(e_config_s.loop_payloads)
                            + "}"
                            + " Actual: {"
                            + std::to_string(e_config_s.header_size)
                            + " "
                            + std::to_string(e_config_s.payload_size_limit)
                            + " "
                            + std::to_string(e_config_s.read_messages)
                            + " "
                            + std::to_string(e_config_s.loop_payloads)
                            + "}";
        issues.push_back(issue);
    }

    // if (e_config.host_info != a_config.host_info)
    // {
    //     // TODO:
    // }

    if (expected.actions.size() != actual.actions.size())
    {
        std::string issue = "Action sizes differ! Expected "
                            + std::to_string(expected.actions.size())
                            + " Actual "
                            + std::to_string(actual.actions.size());
        issues.push_back(issue);
    }

    // Compare actions.
    for (size_t i = 0; i < expected.actions.size(); i++)
    {
        if (i >= actual.actions.size())
        {
            break;
        }

        const auto & e_act = expected.actions[i];
        const auto & a_act = actual.actions[i];

        if (e_act.type != a_act.type
            || e_act.sessions_start != a_act.sessions_start
            || e_act.sessions_end != a_act.sessions_end
            || e_act.offset != a_act.offset)
        {
            std::string issue = "Action "
                                + std::to_string(i)
                                + " had values not equal! Expected {"
                                + action_type_to_string(e_act.type)
                                + " "
                                + std::to_string(e_act.sessions_start)
                                + " "
                                + std::to_string(e_act.sessions_end)
                                + " "
                                + std::to_string(e_act.offset.count())
                                + "} Actual {"
                                + action_type_to_string(a_act.type)
                                + " "
                                + std::to_string(a_act.sessions_start)
                                + " "
                                + std::to_string(a_act.sessions_end)
                                + " "
                                + std::to_string(a_act.offset.count())
                                + "}";
            issues.push_back(issue);
            //break;
        }

        if (e_act.type == a_act.type
            && (e_act.type == ActionType::SEND
                || e_act.type == ActionType::DRAIN))
        {
            if (e_act.count != a_act.count)
            {
                std::string issue = "Action "
                                + std::to_string(i)
                                + " had values not equal! Expected {"
                                + action_type_to_string(e_act.type)
                                + " "
                                + std::to_string(e_act.sessions_start)
                                + " "
                                + std::to_string(e_act.sessions_end)
                                + " "
                                + std::to_string(e_act.offset.count())
                                + "} Actual {"
                                + action_type_to_string(a_act.type)
                                + " "
                                + std::to_string(a_act.sessions_start)
                                + " "
                                + std::to_string(a_act.sessions_end)
                                + " "
                                + std::to_string(a_act.offset.count())
                                + "}";
                issues.push_back(issue);
                //break;
            }
        }
    }

    // Compare payloads.
    if (expected.payloads.size() != actual.payloads.size())
    {
        std::string issue = "Payload list sizes differ! Expected "
                            + std::to_string(expected.payloads.size())
                            + " Actual "
                            + std::to_string(actual.payloads.size());
        issues.push_back(issue);
    }

    for (size_t i = 0; i < expected.payloads.size(); i++)
    {
        if (i >= actual.payloads.size())
        {
            break;
        }

        const auto & e_payload = expected.payloads[i];
        const auto & a_payload = actual.payloads[i];

        if (e_payload.packet_data.size()
            != a_payload.packet_data.size())
        {
            std::string issue = "Payload packet sizes differ! Expected "
                            + std::to_string(e_payload.packet_data.size())
                            + " Actual "
                            + std::to_string(a_payload.packet_data.size());
            issues.push_back(issue);
        }

        if (e_payload.ops.size()
            != a_payload.ops.size())
        {
            std::string issue = "Payload op list sizes differ! Expected "
                            + std::to_string(e_payload.ops.size())
                            + " Actual "
                            + std::to_string(a_payload.ops.size());
            issues.push_back(issue);
        }

        for (size_t j = 0; j < e_payload.ops.size(); j++)
        {
            if (j >= e_payload.ops.size())
            {
                break;
            }

            auto e_op = e_payload.ops[j];
            auto a_op = a_payload.ops[j];

            if (e_op.type != a_op.type
                || e_op.length != a_op.length
                || e_op.little_endian != a_op.little_endian
                || e_op.time_format != a_op.time_format)
            {
                std::string issue = "Payload "
                                    + std::to_string(i)
                                    + " has operation "
                                    + std::to_string(j)
                                    + " with values not equal! Expected {"
                                    + op_type_to_string(e_op.type)
                                    + " "
                                    + std::to_string(e_op.length)
                                    + " "
                                    + std::to_string(e_op.little_endian)
                                    + " "
                                    + time_format_to_string(e_op.time_format)
                                    + "} Actual {"
                                    + op_type_to_string(a_op.type)
                                    + " "
                                    + std::to_string(a_op.length)
                                    + " "
                                    + std::to_string(a_op.little_endian)
                                    + " "
                                    + time_format_to_string(a_op.time_format)
                                    + "}";
                issues.push_back(issue);
                break;
            }
        }
    }

    // Check counters.
    if (expected.counter_steps.size() != actual.counter_steps.size())
    {
        std::string issue = "Counter step sizes differ! Expected "
                            + std::to_string(expected.counter_steps.size())
                            + " Actual "
                            + std::to_string(actual.counter_steps.size());
        issues.push_back(issue);
    }

    for (size_t i = 0; i < expected.counter_steps.size(); i++)
    {
        if (i >= actual.counter_steps.size())
        {
            break;
        }

        if (expected.counter_steps[i] != actual.counter_steps[i])
        {
            std::string issue = "Counter step "
                                + std::to_string(i)
                                + " had value mismatch! Expected "
                                + std::to_string(expected.counter_steps[i])
                                + " Actual "
                                + std::to_string(actual.counter_steps[i]);
            issues.push_back(issue);
            break;
        }
    }

    // Return our issues.
    return issues;
}
