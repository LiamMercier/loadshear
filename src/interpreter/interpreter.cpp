#include "interpreter.h"

#include "lexer.h"
#include "parser.h"

#include <fstream>
#include <thread>

// TODO: remove
#include <iostream>

ParseResult Interpreter::parse_script(std::filesystem::path script_path)
{
    // Check the file exists.
    std::error_code ec;
    if (!std::filesystem::exists(script_path, ec))
    {
        ParseResult error;
        error.success = false;

        if (ec)
        {
            error.reason = ec.message();
        }
        else
        {
            error.reason = "Failed to open file "
                           + script_path.string()
                           + " (could not be found)";
        }

        return error;
    }

    std::ifstream script_file(script_path);

    // Ensure we can read the file and get the size.
    if (!script_file.is_open())
    {
        ParseResult error;
        error.success = false;
        error.reason = "Failed to open file "
                       + script_path.string();

        return error;
    }

    size_t filesize = static_cast<size_t>(std::filesystem::file_size(script_path, ec));

    if (ec)
    {
        ParseResult error;
        error.success = false;
        error.reason = ec.message();

        return error;
    }

    // Dump entire file into the string to pass to the lexer.
    std::string script_raw;
    script_raw.resize(filesize);

    if (!script_file.read(script_raw.data(), filesize))
    {
        ParseResult error;
        error.success = false;
        error.reason = "Failed to read all "
                       + std::to_string(filesize)
                       + " bytes from "
                       + script_path.string();

        return error;
    }

    // From here, we have the entire file read and can start parsing.
    Lexer lexer(std::move(script_raw));
    ParseResult lexer_res = lexer.tokenize(tokens_);

    if (!lexer_res.success)
    {
        return lexer_res;
    }

    if (tokens_.size() == 0)
    {
        ParseResult error;
        error.success = false;
        error.reason = "Lexer returned zero tokens! Your script might be empty?";
        return error;
    }

    // TODO: remove this print
    for (const auto & t : tokens_)
    {
        std::cout << "[" << ttype_to_string(t.type)
                  << ", " << t.text
                  << ", " << t.line << ":" << t.col << "]\n";
    }

    // Now we hand these tokens over to the parser.
    Parser parser(tokens_);
    DSLData unvalidated_script;

    std::cout << "\nStarting parse on tokens!\n\n";

    ParseResult parser_res = parser.parse(unvalidated_script);

    // Parser:
    if (!parser_res.success)
    {
        return parser_res;
    }

    script_ = std::move(unvalidated_script);

    set_script_defaults();

    ParseResult verification_res = verify_script();

    // Validator:

    return {1, ""};
}

void Interpreter::set_script_defaults()
{
    // For each setting, we check if the current value is the data type maximum if it should be set.

    // identifier, session_protocol, header_size, body_max, cannot be defaulted. read, repeat are
    // already defaulted.

    auto & settings = script_.settings;

    // First, go over each option in the SETTING block.
    if (settings.shards == 0)
    {
        int hw_conc = std::thread::hardware_concurrency();

        // If we get no good value, and the user doesn't override, default to single threaded.
        if (hw_conc <= 0)
        {
            hw_conc = 1;
        }

        settings.shards = hw_conc;
    }

    // If we have an empty handler_value, set it to "NOP" as a default.
    if (settings.handler_value.empty())
    {
        settings.handler_value = "NOP";
    }
    
    // We already default the orchestrator actions during parse since we validate the data is
    // possibly correct (but not validated yet).
}

ParseResult Interpreter::verify_script()
{
    // First, ensure each setting is in a valid state before we proceed.
    auto & settings = script_.settings;

    // This basically should never happen.
    if (settings.identifier.empty())
    {
        std::string e_msg = "SETTINGS block had empty identifier.";
        return arbitrary_error(std::move(e_msg));
    }
    
    // Ensure the session protocol is a valid protocol.
    // TODO: fill this with more protocols when relevant.
    if (VALID_PROTOCOLS.find(settings.session_protocol)
        == VALID_PROTOCOLS.end())
    {
        std::string e_msg = "SETTINGS block had invalid protocol: "
                            + settings.session_protocol
                            + "(expected one of TCP)";
        return arbitrary_error(std::move(e_msg));
    }

    // We can have header size be zero, but only if read is false.
    if (settings.header_size == 0 && settings.read)
    {
        std::string e_msg = "SETTINGS block had header size as 0 with reading enabled.";
        return arbitrary_error(std::move(e_msg));
    }

    // We can have body size be zero, but only if read is false.
    if (settings.body_max == 0 && settings.read)
    {
        std::string e_msg = "SETTINGS block had body size as 0 with reading enabled.";
        return arbitrary_error(std::move(e_msg));
    }

    // Prevent having 0 shards.
    if (settings.shards == 0)
    {
        std::string e_msg = "SETTINGS block has shards set to 0.";
        return arbitrary_error(std::move(e_msg));
    }

    // Check that at least one endpoint exists.
    if (settings.endpoints.empty())
    {
        std::string e_msg = "SETTINGS block has no endpoints.";
        return arbitrary_error(std::move(e_msg));
    }

    // Check that at least one packet was defined.
    if (settings.packet_identifiers.empty())
    {
        std::string e_msg = "SETTINGS block has no packets defined.";
        return arbitrary_error(std::move(e_msg));
    }

    // TODO: Check that we can resolve each packet.

    // "could not resolve packet identity <p_ident> corresponding to fs::path"

    // Check that the MessageHandler selected is valid if read is enabled.
    if (settings.read && (VALID_MESSAGE_HANDLERS.find(settings.handler_value)
        == VALID_MESSAGE_HANDLERS.end()))
    {
        // If we have a .wasm file, see that we can resolve it.
        if (settings.handler_value.ends_with(".wasm"))
        {
            // TODO: try to resolve the .wasm file.
            // Assert file exists
        }
        // No .wasm file and no alternative message handler, abort since we can't read.
        else
        {
            std::string e_msg = "SETTINGS block has no valid message handler for READ.";
            return arbitrary_error(std::move(e_msg));
        }
    }

    // Check we have a SETTINGS for this ORCHESTRATOR block.
    auto & orchestrator = script_.orchestrator;

    if (orchestrator.settings_identifier != settings.identifier)
    {
        std::string e_msg = "No matching SETTINGS block for ORCHESTRATOR requesting identifier "
                            + orchestrator.settings_identifier;
        return arbitrary_error(std::move(e_msg));
    }

    // Now, we must check the validity of each action we issue.
    //
    // - For CREATE, we must have at least as many session's as shards
    // - We may only call CREATE once per block, it is a preallocation mechanism
    // - Other commands must have a valid range, it may not exceed the size from CREATE
    // - We will not call CONNECT on connected objects, DISCONNECT on disconnected objects, etc
    // - SEND will not have payload operations that overwrite one another.
    uint32_t pool_size = 0;

    // We need to track which connections are active. At most, we probably
    // only expect 60k sessions, which is basically trivial at startup, but in theory
    // we could also manage maps of int -> int to speed this up at startup.
    std::vector<uint8_t> session_active;
    std::vector<uint8_t> session_disconnect_called;

    for (size_t i = 0; i < orchestrator.actions.size(); i++)
    {
        const auto & action = orchestrator.actions[i];

        switch (action.type)
        {
            case ActionType::CREATE:
            {
                if (pool_size != 0)
                {
                    std::string e_msg = "CREATE [action "
                                        + std::to_string(i)
                                        +"] was called twice in ORCHESTRATOR block.";
                    return arbitrary_error(std::move(e_msg));
                }

                // Shards is always positive.
                if (action.count < settings.shards)
                {
                    std::string e_msg = "CREATE [action "
                                        + std::to_string(i)
                                        + "] has count less than SHARD value.";
                    return arbitrary_error(std::move(e_msg));
                }

                pool_size = action.count;
                session_active.resize(pool_size);
                session_disconnect_called.resize(pool_size);
                break;
            }
            case ActionType::CONNECT:
            {
                // Prevent CONNECT before CREATE calls.
                if (pool_size == 0)
                {
                    std::string e_msg = "CONNECT [action "
                                        + std::to_string(i)
                                        + "] called before CREATE.";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = "CONNECT [action "
                                        + std::to_string(i)
                                        + "] tried to connect "
                                        + std::to_string(action.range.second)
                                        + " sessions (pool only holds "
                                        + std::to_string(pool_size)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Mark as connected.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    // If already connected, error found.
                    if (session_active[j] == 1)
                    {
                        std::string e_msg = "CONNECT [action "
                                            + std::to_string(i)
                                            + "] was scheduled for "
                                            + std::to_string(j)
                                            + " despite being previously scheduled.";
                        return arbitrary_error(std::move(e_msg));
                    }

                    session_active[j] = 1;
                }

                break;
            }
            case ActionType::SEND:
            {
                // Check the count is positive
                if (action.count == 0)
                {
                    std::string e_msg = "SEND [action "
                                        + std::to_string(i)
                                        + "] is trying to send 0 copies.";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = "SEND [action "
                                        + std::to_string(i)
                                        + "] tried to connect "
                                        + std::to_string(action.range.second)
                                        + " sessions (pool only holds "
                                        + std::to_string(pool_size)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the identifier is in the settings.
                if (settings.packet_identifiers.find(action.packet_identifier) ==
                    settings.packet_identifiers.end())
                {
                    std::string e_msg = "SEND [action "
                                        + std::to_string(i)
                                        + "] has undefined packet identifier ("
                                        + action.packet_identifier
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the modifications are accounted for.
                if (action.mod_order.size()
                    != action.timestamp_mods.size() + action.counter_mods.size())
                {
                    std::string e_msg = "SEND [action "
                                        + std::to_string(i)
                                        + "] has "
                                        + std::to_string(action.timestamp_mods.size()
                                                         + action.counter_mods.size())
                                        + " modifications but only "
                                        + std::to_string(action.mod_order.size())
                                        + " were accounted for.";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check each modification is valid, and doesn't overwrite one another.
                std::map<uint32_t, uint32_t> mod_ranges;

                for (const auto & time_mod : action.timestamp_mods)
                {
                    // If we are trying to overwrite another modification, stop.
                    ParseResult map_res = insert_mod_range(mod_ranges,
                                                           time_mod.timestamp_bytes,
                                                           i);

                    if (!map_res.success)
                    {
                        return map_res;
                    }

                    if (VALID_TIME_FORMATS.find(time_mod.format_name)
                        == VALID_TIME_FORMATS.end())
                    {
                        std::string e_msg = "SEND [action "
                                            + std::to_string(i)
                                            + "] has invalid time format "
                                            + time_mod.format_name
                                            + " (expected one of: seconds, milliseconds, "
                                            + "microseconds, nanoseconds)";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // If more than 8 bytes are being sent, this is undefined behavior.
                    if (time_mod.timestamp_bytes.second > 8)
                    {
                        std::string e_msg = "SEND [action "
                                            + std::to_string(i)
                                            + "] has timestamp of size "
                                            + std::to_string(time_mod.timestamp_bytes.second)
                                            + " (should be at most 8)";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // TODO: we need the packet size for bounds checking.
                }

                for (const auto & counter_mod : action.counter_mods)
                {
                    // If we are trying to overwrite another modification, stop.
                    ParseResult map_res = insert_mod_range(mod_ranges,
                                                           counter_mod.counter_bytes,
                                                           i);

                    if (!map_res.success)
                    {
                        return map_res;
                    }

                    if (counter_mod.counter_step == 0)
                    {
                        std::string e_msg = "SEND [action "
                                        + std::to_string(i)
                                        + "] has COUNTER step equal to zero.";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // If more than 8 bytes are being sent, this is undefined behavior.
                    if (counter_mod.counter_bytes.second > 8)
                    {
                        std::string e_msg = "SEND [action "
                                            + std::to_string(i)
                                            + "] has timestamp of size "
                                            + std::to_string(counter_mod.counter_bytes.second)
                                            + " (should be at most 8)";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // TODO: we need the packet size.
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = "SEND [action "
                                            + std::to_string(i)
                                            + "] was scheduled for "
                                            + std::to_string(j)
                                            + " despite not being connected.";
                        return arbitrary_error(std::move(e_msg));
                    }
                }

                break;
            }
            case ActionType::FLOOD:
            {
                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = "FLOOD [action "
                                        + std::to_string(i)
                                        + "] tried to connect "
                                        + std::to_string(action.range.second)
                                        + " sessions (pool only holds "
                                        + std::to_string(pool_size)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = "FLOOD [action "
                                            + std::to_string(i)
                                            + "] was scheduled for "
                                            + std::to_string(j)
                                            + " despite not being connected.";
                        return arbitrary_error(std::move(e_msg));
                    }
                }

                break;
            }
            case ActionType::DRAIN:
            {
                // We should have a positive timeout.
                if (action.count == 0)
                {
                    std::string e_msg = "DRAIN [action "
                                        + std::to_string(i)
                                        + "] has timeout set to 0 "
                                        + "and would immediately evict sessions. Use "
                                        + "DISCONNECT if this is desired.";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = "DRAIN [action "
                                        + std::to_string(i)
                                        + "] tried to connect "
                                        + std::to_string(action.range.second)
                                        + " sessions (pool only holds "
                                        + std::to_string(pool_size)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = "DRAIN [action "
                                            + std::to_string(i)
                                            + "] was scheduled for "
                                            + std::to_string(j)
                                            + " despite not being connected.";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // Set that we are not connecting.
                    session_active[j] = 0;
                }
                break;
            }
            case ActionType::DISCONNECT:
            {
                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = "DISCONNECT [action "
                                        + std::to_string(i)
                                        + "] tried to connect "
                                        + std::to_string(action.range.second)
                                        + " sessions (pool only holds "
                                        + std::to_string(pool_size)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check disconnect was not already called.
                //
                // We cannot check if the session is active since we might DRAIN then DISCONNECT
                // certain ranges differently for some behaviors.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_disconnect_called[j] == 1)
                    {
                        std::string e_msg = "DISCONNECT [action "
                                            + std::to_string(i)
                                            + "] was scheduled for "
                                            + std::to_string(j)
                                            + " despite already being called.";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // Set disconnected.
                    session_disconnect_called[j] = 1;
                }
                break;
            }
        }
    }

    return good_parse();
}

ParseResult Interpreter::insert_mod_range(std::map<uint32_t, uint32_t> & map,
                                          Range to_insert,
                                          size_t action_id)
{
    // Find the lowest starting range.
    auto it = map.lower_bound(to_insert.start);
    if (it != map.end() && it->first <= to_insert.second)
    {
        return bad_range_error(Range(it->first, it->second),
                               to_insert,
                               action_id);
    }

    // Check prev range and compare for violations.
    if (it != map.begin())
    {
        auto prev = std::prev(it);
        if (prev->second >= to_insert.start)
        {
            return bad_range_error(Range(prev->first, prev->second),
                                   to_insert,
                                   action_id);
        }
    }

    // Otherwise, everything is good, insert this range for future checks.
    map[to_insert.start] = to_insert.second;
    return good_parse();
}

ParseResult Interpreter::arbitrary_error(std::string reason)
{
    ParseResult res;
    res.success = false;
    res.reason = std::move(reason);
    return res;
}

ParseResult Interpreter::bad_range_error(Range overlapped,
                                         Range violating_range,
                                         size_t action_id)
{
    std::string e_msg = "SEND [action "
                        + std::to_string(action_id)
                        + "] has modification of range {"
                        + std::to_string(violating_range.start)
                        + " "
                        + std::to_string(violating_range.second)
                        + "} overlapping previous modification of range {"
                        + std::to_string(overlapped.start)
                        + " "
                        + std::to_string(overlapped.second)
                        + "}";
    return arbitrary_error(std::move(e_msg));
}

ParseResult Interpreter::good_parse()
{
    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}
