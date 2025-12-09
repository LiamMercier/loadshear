#include "interpreter.h"

#include "lexer.h"
#include "parser.h"

#include <fstream>

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

    script_ = std::move(unvalidated_script);

    if (!parser_res.success)
    {
        return parser_res;
    }

    // TODO: check result has data.

    // TODO: validate this before moving.
    // script_ = std::move(unvalidated_script);

    return {1, ""};
}
