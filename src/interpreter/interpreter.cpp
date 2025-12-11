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
                           + " (could not be found).";
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

    if (!parser_res.success)
    {
        return parser_res;
    }

    script_ = std::move(unvalidated_script);

    set_script_defaults();

    // TODO: check result has data for required fields.
    ParseResult verification_res = verify_script();

    return {1, ""};
}

void Interpreter::set_script_defaults()
{
    // For each setting, we check if the current value is the data type maximum if it should be set.

    // identifier, session_protocol, header_size, body_max, cannot be defaulted. read, repeat are
    // already defaulted.

    auto & settings = script_.settings;

    // First, go over each option in the SETTING block.
    if (settings.shards == UINT32_MAX)
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
    // TODO: verify the script is correct.
    
    // First, ensure each setting is in a valid state before we proceed.
    auto & settings = script_.settings;

    if (settings.identifier.empty())
    {
        // TODO: bad settings
        return;
    }
    
    // Ensure the session protocol is a valid protocol.
    if (VALID_PROTOCOLS.find(settings.session_protocol) 
        == VALID_PROTOCOLS.end())
    {
        // TODO: bad settings
        return;
    }

    return good_parse();
}

ParseResult Interpreter::good_parse()
{
    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}
