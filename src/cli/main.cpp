#include "cli.h"
#include <iostream>

int main(int argc, char** argv)
{
    // First, parse command line flags.
    CLIParseResult parse_res = parse_cli(argc, argv);

    if (!parse_res.good_parse())
    {
        return parse_res.status_code();
    }

    CLIOptions c_ops = std::move(parse_res.options);

    CLI cli(std::move(c_ops));

    return cli.run();
}
