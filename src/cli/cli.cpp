#include "cli.h"

#include "interpreter.h"
#include "resolver.h"
#include "execution-plan.h"
#include "logger.h"

#include "all-transports.h"

CLI::CLI(CLIOptions ops)
:cli_ops_(std::move(ops)),
arena_(cli_ops_.arena_init_mb * (1024 * 1024),
       std::pmr::get_default_resource())
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
        Logger::error(i_res.reason);
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
            auto plan = generate_execution_plan<TCPSession>(script,
                                                            &arena_);

            // Handle unexpected value.
            if (!plan)
            {
                std::string e_msg = plan.error();
                Logger::error(std::move(e_msg));
                return 1;
            }

            // TODO: use the plan
            break;
        }
        // Error in script protocol.
        default:
        {
            std::string e_msg = "Unrecognized transport "
                                + script.settings.session_protocol
                                + " was specified.";
            Logger::error(std::move(e_msg));
            return 1;
        }
    }

    return 0;

}




