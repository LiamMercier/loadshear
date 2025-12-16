#include "cli.h"

#include "interpreter.h"
#include "resolver.h"

CLI::CLI(CLIOptions ops)
:cli_ops_(std::move(ops))
{

}

int CLI::run()
{
    // TODO: turn this into a function processing all options.
    ResolverOptions resolver_ops;
    resolver_ops.expand_envs = cli_ops_.expand_envs;
    Resolver::set_global_resolve_options(resolver_ops);

    return 0;
}
