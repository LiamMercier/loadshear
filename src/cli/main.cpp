#include <iostream>

#include "cli.h"
#include "logger.h"
#include "version.h"

int main(int argc, char** argv)
{
    // Start the logger.
    Logger::init(LogLevel::INFO);

    Logger::info(std::string{VERSION_PRINTSTRING});

    // First, parse command line flags.
    CLIParseResult parse_res = parse_cli(argc, argv);

    if (!parse_res.good_parse())
    {
        return parse_res.status_code();
    }

    CLIOptions c_ops = std::move(parse_res.options);

    // Now try to run the tool.
    std::unique_ptr<CLI> cli;

    try
    {
        cli = std::make_unique<CLI>(std::move(c_ops));
    }
    catch (const std::exception & error)
    {
        std::string e_str = std::string("Tried to start program (got error: ")
                            + error.what()
                            + ")";

        Logger::error(std::move(e_str));
        Logger::shutdown();
        return 1;
    }

    int cli_res = cli->run();

    // Turn the logger off.
    Logger::shutdown();

    return cli_res;
}
