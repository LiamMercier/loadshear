#include <iostream>

#include "cli.h"
#include "logger.h"

int main(int argc, char** argv)
{
    // First, parse command line flags.
    CLIParseResult parse_res = parse_cli(argc, argv);

    if (!parse_res.good_parse())
    {
        return parse_res.status_code();
    }

    CLIOptions c_ops = std::move(parse_res.options);

    // Start the logger.
    Logger::init(LogLevel::INFO);

    CLI cli(std::move(c_ops));

    int cli_res = cli.run();

    // Turn the logger off.
    Logger::shutdown();

    return cli_res;
}
