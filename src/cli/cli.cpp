#include "cli.h"

#include <iostream>

#include "interpreter.h"
#include "resolver.h"
#include "execution-plan.h"

#include "all-transports.h"

CLI::CLI(CLIOptions ops)
:cli_ops_(std::move(ops))
{

}

int CLI::run()
{
    ResolverOptions resolver_ops;
    resolver_ops.expand_envs = cli_ops_.expand_envs;
    Resolver::set_global_resolve_options(resolver_ops);

    // Try to parse the script file.
    Interpreter interpreter;
    ParseResult i_res = interpreter.parse_script(cli_ops_.script_file);

    if (!i_res.success)
    {
        std::cout << i_res.reason << "\n";
        return 1;
    }

    return execute_script(interpreter.script_);
}

int CLI::execute_script(const DSLData & script)
{
    // Our execution data depends on the Session type. Start here.
    ProtocolType protocol = ProtocolType::UNDEFINED;

    // TODO <feature>: Add more protocols here when implemented.
    if (script.settings.session_protocol == "TCP")
    {
        protocol = ProtocolType::TCP;
    }

    switch (protocol)
    {
        // Create TCP specific plan and execute.
        case ProtocolType::TCP:
        {
            auto plan = generate_execution_plan<TCPSession>(script);

            // Handle unexpected value.
            if (!plan)
            {

            }

            // TODO: use the plan

        }
        // Error in script protocol.
        default:
        {
            return 1;
        }
    }

    return 0;

}




