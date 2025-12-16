#include "cli.h"

int main(int argc, char** argv)
{
    // First, parse command line flags.
    auto maybe_ops = parse_cli(argc, argv);

    if (!maybe_ops)
    {
        return 1;
    }

    CLIOptions c_ops = std::move(*maybe_ops);

    CLI cli(std::move(c_ops));

    return cli.run();
}
