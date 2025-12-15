#include "interpreter.h"

#include "lexer.h"
#include "parser.h"
#include "resolver.h"

#include <fstream>
#include <thread>

ParseResult Interpreter::parse_script(std::string script_name)
{
    // Check the file exists.
    std::string error_string;
    auto script_path = Resolver::resolve_file(script_name,
                                              error_string);

    if (script_path.empty())
    {
        std::string e_str = styled_string("[Error]: ",
                                          PrintStyle::Error)
                            + "Failed to resolve file "
                            + script_name
                            + " (got error: "
                            + error_string
                            + ")";

        return arbitrary_error(std::move(e_str));
    }

    std::ifstream script_file(script_path);

    // Ensure we can read the file and get the size.
    if (!script_file.is_open())
    {
        std::string e_str = styled_string("[Error]: ",
                                          PrintStyle::Error)
                            + "Failed to open file "
                            + styled_string(script_name, PrintStyle::BadValue)
                            + " which resolved to path "
                            + styled_string(script_path.string(),
                                            PrintStyle::Reference);

        return arbitrary_error(std::move(e_str));
    }

    size_t filesize = Resolver::get_file_size(script_path);

    if (filesize == 0)
    {
        std::string e_str = styled_string("[Error]: ",
                                          PrintStyle::Error)
                            + "File size for "
                            + styled_string(script_path.string(),
                                            PrintStyle::Reference)
                            + " was "
                            + styled_string("0", PrintStyle::BadValue);

        return arbitrary_error(std::move(e_str));
    }

    // Dump entire file into the string to pass to the lexer.
    std::string script_raw;
    script_raw.resize(filesize);

    if (!script_file.read(script_raw.data(), filesize))
    {
        std::string e_str = styled_string("[Error]: ",
                                          PrintStyle::Error)
                            + "Failed to read all "
                            + styled_string(std::to_string(filesize),
                                            PrintStyle::Context)
                            + " bytes from "
                            + styled_string(script_path.string(),
                                            PrintStyle::Reference);

        return arbitrary_error(std::move(e_str));
    }

    // From here, we have the entire file read and can start parsing.
    Lexer lexer(std::move(script_raw));
    ParseResult lexer_res = lexer.tokenize(tokens_);

    if (!lexer_res.success)
    {
        lexer_res.reason = styled_string("[Lexer Error]: ",
                                         PrintStyle::Error)
                           + lexer_res.reason;
        return lexer_res;
    }

    if (tokens_.size() == 0)
    {
        ParseResult error;
        error.success = false;
        error.reason = styled_string("[Lexer Error]: ", PrintStyle::Error)
                       + "Lexer returned zero tokens! "
                       "Your script might be empty?";
        return error;
    }

    // Now we hand these tokens over to the parser.
    Parser parser(tokens_);
    DSLData unvalidated_script;

    ParseResult parser_res = parser.parse(unvalidated_script);

    // Parser:
    if (!parser_res.success)
    {
        parser_res.reason = styled_string("[Parser Error]: ", PrintStyle::Error)
                            + parser_res.reason;
        return parser_res;
    }

    script_ = std::move(unvalidated_script);

    set_script_defaults();

    ParseResult verification_res = verify_script();

    if (!verification_res.success)
    {
        verification_res.reason = styled_string("[Validator Error]: ",
                                                PrintStyle::Error)
                                  + verification_res.reason;
        return verification_res;
    }

    return good_parse();
}

void Interpreter::set_script_defaults()
{
    // For each setting, we check if the current value is the data
    // type maximum if it should be set.

    // identifier, session_protocol, header_size, body_max, cannot
    // be defaulted. read, repeat are already defaulted.

    auto & settings = script_.settings;

    // First, go over each option in the SETTING block.
    if (settings.shards == 0)
    {
        int hw_conc = std::thread::hardware_concurrency();

        // If we get no good value, and the user doesn't override,
        // default to single threaded.
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
    
    // We already default the orchestrator actions during parse since we
    // validate the data is possibly correct (but not validated yet).
}

ParseResult Interpreter::verify_script()
{
    // First, ensure each setting is in a valid state before we proceed.
    auto & settings = script_.settings;

    // This basically should never happen.
    if (settings.identifier.empty())
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::BadField)
                            + " block had empty identifier";
        return arbitrary_error(std::move(e_msg));
    }
    
    // Ensure the session protocol is a valid protocol.
    // TODO: fill this with more protocols when relevant.
    if (VALID_PROTOCOLS.find(settings.session_protocol)
        == VALID_PROTOCOLS.end())
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::Keyword)
                            + " block had invalid "
                            + styled_string("SESSION", PrintStyle::BadField)
                            + " "
                            + styled_string(settings.session_protocol,
                                            PrintStyle::BadValue)
                            + " (expected one of "
                            + styled_string("TCP", PrintStyle::Expected)
                            + ")";
        return arbitrary_error(std::move(e_msg));
    }

    // We can have header size be zero, but only if read is false.
    if (settings.header_size == 0 && settings.read)
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::Keyword)
                            + " block had "
                            + styled_string("HEADERSIZE", PrintStyle::BadField)
                            + " "
                            + styled_string("0", PrintStyle::BadValue)
                            + " with reading enabled";
        return arbitrary_error(std::move(e_msg));
    }

    // TODO: continue
    // We can have body size be zero, but only if read is false.
    if (settings.body_max == 0 && settings.read)
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::Keyword)
                            + " block had "
                            + styled_string("BODYMAX", PrintStyle::BadField)
                            + " set to "
                            + styled_string("0", PrintStyle::BadValue)
                            + " with reading enabled";
        return arbitrary_error(std::move(e_msg));
    }

    // Prevent having 0 shards.
    if (settings.shards == 0)
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::Keyword)
                            + " block has "
                            + styled_string("SHARD", PrintStyle::BadField)
                            + " set to "
                            + styled_string("0", PrintStyle::BadValue);
        return arbitrary_error(std::move(e_msg));
    }

    // Check that at least one endpoint exists.
    if (settings.endpoints.empty())
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::BadField)
                            + " block has no endpoints";
        return arbitrary_error(std::move(e_msg));
    }

    // Check that at least one packet was defined.
    if (settings.packet_identifiers.empty())
    {
        std::string e_msg = styled_string("SETTINGS", PrintStyle::BadField)
                            + " block has no packets defined";
        return arbitrary_error(std::move(e_msg));
    }

    // Check that we can resolve each packet.
    for (const auto & packet_id : settings.packet_identifiers)
    {
        std::string error_string;
        auto path = Resolver::resolve_file(packet_id.second,
                                           error_string);

        if (path.empty())
        {
            std::string e_msg = "The "
                                + styled_string("SETTINGS", PrintStyle::Keyword)
                                + " "
                                + styled_string("PACKETS", PrintStyle::BadField)
                                + " block has unresolvable packet "
                                + styled_string(packet_id.first,
                                                PrintStyle::BadValue)
                                + " with identifier "
                                + styled_string(packet_id.second,
                                                PrintStyle::Reference)
                                + " (got error: "
                                + styled_string(error_string,
                                                PrintStyle::Error)
                                + ")";
            return arbitrary_error(std::move(e_msg));
        }
    }

    // Check that the MessageHandler selected is valid if read is enabled.
    if (settings.read && (VALID_MESSAGE_HANDLERS.find(settings.handler_value)
        == VALID_MESSAGE_HANDLERS.end()))
    {
        // If we have a .wasm file, see that we can resolve it.
        if (settings.handler_value.ends_with(".wasm"))
        {
            std::string error_string;
            auto path = Resolver::resolve_file(settings.handler_value,
                                               error_string);

            if (path.empty())
            {
                std::string e_msg = styled_string("SETTINGS",
                                                  PrintStyle::Keyword)
                                    + " block has message "
                                    + styled_string("HANDLER",
                                                    PrintStyle::BadField)
                                    + " ("
                                    + styled_string(settings.handler_value,
                                                    PrintStyle::BadValue)
                                    + ") that cannot be resolved "
                                    "(got error: "
                                    + styled_string(error_string,
                                                    PrintStyle::Error)
                                    + ")";
                return arbitrary_error(std::move(e_msg));
            }
        }
        // No .wasm file and no alternative message handler,
        // abort since we can't read yet read is enabled.
        else
        {
            std::string e_msg = styled_string("SETTINGS",
                                              PrintStyle::Keyword)
                                + " block has no valid message "
                                + styled_string("HANDLER",
                                                PrintStyle::BadField)
                                + " for "
                                + styled_string("READ", PrintStyle::Keyword)
                                + " to use for reading";
            return arbitrary_error(std::move(e_msg));
        }
    }

    // Check we have a SETTINGS for this ORCHESTRATOR block.
    auto & orchestrator = script_.orchestrator;

    // Check we have an orchestrator.
    if (orchestrator.settings_identifier == "")
    {
        std::string e_msg = styled_string("ORCHESTRATOR", PrintStyle::BadField)
                            + " is undefined or has no "
                            + styled_string("SETTINGS", PrintStyle::Keyword)
                            + " identifier. Expected: "
                            + styled_string("ORCHESRTATOR <settings_id> { ... }",
                                            PrintStyle::Expected);
        return arbitrary_error(std::move(e_msg));
    }

    if (orchestrator.settings_identifier != settings.identifier)
    {
        std::string e_msg = "No matching "
                            + styled_string("SETTINGS", PrintStyle::Keyword)
                            + " block for "
                            + styled_string("ORCHESTRATOR", PrintStyle::BadField)
                            + " requesting identifier "
                            + styled_string(orchestrator.settings_identifier,
                                            PrintStyle::Reference);
        return arbitrary_error(std::move(e_msg));
    }

    // Now, we must check the validity of each action we issue.
    //
    // - For CREATE, we must have at least as many session's as shards
    // - We may only call CREATE once per block, it is a preallocation mechanism
    // - Other commands must have a valid range, it may not exceed the size
    //   from CREATE
    // - We will not call CONNECT on connected objects
    // - We will not call DISCONNECT on disconnected objects
    // - SEND will not have payload operations that overwrite one another.
    // - SEND will not have modifications past the packet's last index
    //   value (size - 1).
    uint32_t pool_size = 0;

    // We need to track which connections are active. At most, we probably
    // only expect 60k sessions, which is basically trivial at startup.
    //
    // We could also manage maps of int -> int to speed this up at startup.
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
                    std::string e_msg = styled_string("CREATE",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        +"] was called twice in "
                                        + styled_string("ORCHESTRATOR",
                                                        PrintStyle::Keyword)
                                        + " block";
                    return arbitrary_error(std::move(e_msg));
                }

                // Shards is always positive.
                if (action.count < settings.shards)
                {
                    std::string e_msg = styled_string("CREATE",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has count less than "
                                        + styled_string("SHARD",
                                                        PrintStyle::Keyword)
                                        + " value "
                                        + styled_string(
                                            std::to_string(action.count),
                                            PrintStyle::Limits);
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
                    std::string e_msg = styled_string("CONNECT",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] called before "
                                        + styled_string("CREATE",
                                                        PrintStyle::Keyword);
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = styled_string("CONNECT",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] was scheduled for session "
                                        + styled_string(
                                            std::to_string(action.range.second),
                                            PrintStyle::BadValue)
                                        + " (pool only holds "
                                        + styled_string(
                                            std::to_string(pool_size),
                                            PrintStyle::Limits)
                                        + " sessions)";
                    return arbitrary_error(std::move(e_msg));
                }

                // Mark as connected.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    // If already connected, error found.
                    if (session_active[j] == 1)
                    {
                        std::string e_msg = styled_string("CONNECT",
                                                          PrintStyle::BadField)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] was scheduled for session "
                                            + styled_string(std::to_string(j),
                                                            PrintStyle::BadValue)
                                            + " while already scheduled";
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
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] is trying to send "
                                        + styled_string(
                                            std::to_string(action.count),
                                            PrintStyle::BadValue)
                                        + " copies";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] was scheduled for session "
                                        + styled_string(
                                            std::to_string(action.range.second),
                                            PrintStyle::BadValue)
                                        + " (pool only holds "
                                        + styled_string(
                                            std::to_string(pool_size),
                                            PrintStyle::Limits)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the identifier is in the settings.
                if (settings.packet_identifiers.find(action.packet_identifier) ==
                    settings.packet_identifiers.end())
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has undefined packet identifier ("
                                        + styled_string(
                                            action.packet_identifier,
                                            PrintStyle::BadValue)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the modifications are accounted for.
                if (action.mod_order.size()
                    != action.timestamp_mods.size() + action.counter_mods.size())
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                        + std::to_string(i), PrintStyle::Reference)
                                        + "] has "
                                        + styled_string(
                                            std::to_string(action.timestamp_mods
                                                                    .size()
                                            + action.counter_mods.size()),
                                            PrintStyle::BadValue)
                                        + " modifications but only "
                                        + styled_string(
                                            std::to_string(
                                                action.mod_order.size()),
                                            PrintStyle::Limits)
                                        + " were accounted for";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the packet file exists and can be used.
                std::string error_string;
                auto iter = settings.packet_identifiers.find
                                                (
                                                    action.packet_identifier
                                                );

                if (iter == settings.packet_identifiers.end())
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has no packet file for identity"
                                        + styled_string(action.packet_identifier,
                                                        PrintStyle::BadValue);
                    return arbitrary_error(std::move(e_msg));
                }

                auto packet_path = Resolver::resolve_file(iter->second,
                                                          error_string);

                if (packet_path.empty())
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has unresolvable packet file "
                                        + styled_string(iter->second,
                                                        PrintStyle::BadValue)
                                        + " corresponding to identity "
                                        + styled_string(action.packet_identifier,
                                                        PrintStyle::Context)
                                        + " (got error: "
                                        + styled_string(error_string,
                                                        PrintStyle::Error)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                size_t packet_size = Resolver::get_file_size(packet_path);

                if (packet_size == 0)
                {
                    std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has packet resolving to path "
                                        + styled_string(packet_path.string(),
                                                        PrintStyle::Context)
                                        + " with "
                                        + styled_string("0", PrintStyle::BadValue)
                                        + " bytes of data";
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
                        std::string e_msg = styled_string("SEND",
                                                          PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has invalid "
                                            + styled_string("TIMESTAMP",
                                                            PrintStyle::BadField)
                                            + " format "
                                            + styled_string(time_mod.format_name,
                                                            PrintStyle::BadValue)
                                            + " (expected one of: "
                                            + styled_string(
                                                "seconds, milliseconds, "
                                                "microseconds, nanoseconds",
                                                PrintStyle::Expected)
                                            + ")";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // If more than 8 bytes are being sent,
                    // this is undefined behavior.
                    if (time_mod.timestamp_bytes.second > 8)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has "
                                            + styled_string("TIMESTAMP",
                                                            PrintStyle::BadField)
                                            + " of size "
                                            + styled_string(
                                                std::to_string(
                                                    time_mod
                                                    .timestamp_bytes
                                                    .second),
                                                PrintStyle::BadValue)
                                            + " (should be at most "
                                            + styled_string("8",
                                                            PrintStyle::Limits)
                                            + ")";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // Check we do not exceed the payload bounds.
                    if (time_mod.timestamp_bytes.end_from_length() >= packet_size)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has "
                                            + styled_string("TIMESTAMP",
                                                            PrintStyle::BadField)
                                            + " ending at index "
                                            + styled_string(
                                                std::to_string(
                                                    time_mod
                                                    .timestamp_bytes
                                                    .end_from_length()),
                                                PrintStyle::BadValue)
                                            + " exceeding end of packet "
                                            + styled_string(
                                                action.packet_identifier,
                                                PrintStyle::Reference)
                                            + " which has maximum index "
                                            + styled_string(
                                                std::to_string(packet_size - 1),
                                                PrintStyle::Limits);
                        return arbitrary_error(std::move(e_msg));
                    }
                }

                for (const auto & counter_mod : action.counter_mods)
                {
                    // If we are trying to overwrite another modification, stop.
                    ParseResult map_res = insert_mod_range
                                                (
                                                    mod_ranges,
                                                    counter_mod.counter_bytes,
                                                    i
                                                );

                    if (!map_res.success)
                    {
                        return map_res;
                    }

                    if (counter_mod.counter_step == 0)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has "
                                            + styled_string("COUNTER",
                                                            PrintStyle::BadField)
                                            + " step set to "
                                            + styled_string("0",
                                                            PrintStyle::BadValue);
                        return arbitrary_error(std::move(e_msg));
                    }

                    // If more than 8 bytes are being sent,
                    // this is undefined behavior.
                    if (counter_mod.counter_bytes.second > 8)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has "
                                            + styled_string("COUNTER",
                                                            PrintStyle::BadField)
                                            + " of size "
                                            + styled_string(
                                                std::to_string(
                                                    counter_mod
                                                    .counter_bytes
                                                    .second),
                                                PrintStyle::BadValue)
                                            + " (should be at most "
                                            + styled_string("8",
                                                            PrintStyle::Limits)
                                            + ")";
                        return arbitrary_error(std::move(e_msg));
                    }

                    // Check we do not exceed the payload bounds.
                    if (counter_mod.counter_bytes.end_from_length()
                        >= packet_size)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::Keyword)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] has "
                                            + styled_string("COUNTER",
                                                            PrintStyle::BadField)
                                            + " ending at index "
                                            + styled_string(
                                                std::to_string(
                                                    counter_mod
                                                    .counter_bytes
                                                    .end_from_length()),
                                                PrintStyle::BadValue)
                                            + " exceeding end of packet "
                                            + styled_string(
                                                action.packet_identifier,
                                                PrintStyle::Reference)
                                            + " which has maximum index "
                                            + styled_string(
                                                std::to_string(packet_size - 1),
                                                PrintStyle::Limits);
                        return arbitrary_error(std::move(e_msg));
                    }
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = styled_string("SEND",
                                                      PrintStyle::BadField)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] was scheduled for session "
                                            + styled_string(std::to_string(j),
                                                            PrintStyle::BadValue)
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
                    std::string e_msg = styled_string("FLOOD",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] was scheduled for session "
                                        + styled_string(
                                            std::to_string(action.range.second),
                                            PrintStyle::BadValue)
                                        + " (pool only holds "
                                        + styled_string(
                                            std::to_string(pool_size),
                                            PrintStyle::Limits)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = styled_string("FLOOD",
                                                          PrintStyle::BadField)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] was scheduled for session "
                                            + styled_string(std::to_string(j),
                                                            PrintStyle::BadValue)
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
                    std::string e_msg = styled_string("DRAIN",
                                                      PrintStyle::Keyword)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] has "
                                        + styled_string("TIMEOUT",
                                                        PrintStyle::BadField)
                                        + " set to "
                                        + styled_string("0",
                                                        PrintStyle::BadValue)
                                        + styled_string(
                                            " and would immediately "
                                            "evict sessions. Use "
                                            "DISCONNECT if this is desired.",
                                            PrintStyle::Context);
                    return arbitrary_error(std::move(e_msg));
                }

                // Check the action is in range.
                if (action.range.second > pool_size)
                {
                    std::string e_msg = styled_string("DRAIN",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] was scheduled for session "
                                        + styled_string(
                                            std::to_string(action.range.second),
                                            PrintStyle::BadValue)
                                        + " (pool only holds "
                                        + styled_string(
                                            std::to_string(pool_size),
                                            PrintStyle::Limits)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // If not connected, stop now.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_active[j] != 1)
                    {
                        std::string e_msg = styled_string("DRAIN",
                                                      PrintStyle::BadField)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] was scheduled for session "
                                            + styled_string(
                                                std::to_string(j),
                                                PrintStyle::BadValue)
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
                    std::string e_msg = styled_string("DISCONNECT",
                                                      PrintStyle::BadField)
                                        + " ["
                                        + styled_string("action "
                                            + std::to_string(i),
                                            PrintStyle::Reference)
                                        + "] was scheduled for session "
                                        + styled_string(
                                            std::to_string(action.range.second),
                                            PrintStyle::BadValue)
                                        + " sessions (pool only holds "
                                        + styled_string(
                                            std::to_string(pool_size),
                                            PrintStyle::Limits)
                                        + ")";
                    return arbitrary_error(std::move(e_msg));
                }

                // Check disconnect was not already called.
                //
                // We cannot check if the session is active since we
                // might DRAIN then DISCONNECT certain ranges
                // differently for some behaviors.
                for (size_t j = action.range.start; j < action.range.second; j++)
                {
                    if (session_disconnect_called[j] == 1)
                    {
                        std::string e_msg = styled_string("DISCONNECT",
                                                      PrintStyle::BadField)
                                            + " ["
                                            + styled_string("action "
                                                + std::to_string(i),
                                                PrintStyle::Reference)
                                            + "] was scheduled for session "
                                            + styled_string(
                                                std::to_string(j),
                                                PrintStyle::BadValue)
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
    std::string e_msg = styled_string("SEND", PrintStyle::BadField)
                        + " ["
                        + styled_string("action "
                        + std::to_string(action_id), PrintStyle::Reference)
                        + "] has modification of range {"
                        + styled_string(
                            std::to_string(violating_range.start)
                            + " "
                            + std::to_string(violating_range.second),
                            PrintStyle::BadValue)
                        + "} overlapping previous modification of range {"
                        + styled_string(
                            std::to_string(overlapped.start)
                            + " "
                            + std::to_string(overlapped.second),
                            PrintStyle::Limits)
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
