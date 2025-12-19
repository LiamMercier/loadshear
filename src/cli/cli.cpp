#include "cli.h"

#include "interpreter.h"
#include "resolver.h"
#include "logger.h"
#include "orchestrator.h"

#include "all-transports.h"

//
// Helpers
//

// Create a string [MM:SS:mmm] to display the time offset.
std::string ms_to_timestring(uint32_t offset_ms)
{
    // Minutes, seconds, milliseconds
    uint32_t minutes = offset_ms / 60000;
    uint32_t seconds = (offset_ms % 60000) / 1000;
    uint32_t milliseconds = offset_ms % 1000;

    std::string min_str = std::to_string(minutes);
    std::string sec_str = std::to_string(seconds);
    std::string milli_str = std::to_string(milliseconds);

    if (min_str.size() < 2)
    {
        min_str = "0" + min_str;
    }

    if (sec_str.size() < 2)
    {
        sec_str = "0" + sec_str;
    }

    if (milli_str.size() == 1)
    {
        milli_str = "00" + milli_str;
    }
    else if (milli_str.size() == 2)
    {
        milli_str = "0" + milli_str;
    }

    std::string res = "["
                      + min_str
                      + ":"
                      + sec_str
                      + ":"
                      + milli_str
                      + "] ";
    return res;
}

// For dry run to print packet operation type.
std::string packet_op_to_str(PacketOperationType type)
{
    switch (type)
    {
        case PacketOperationType::IDENTITY:
        {
            return "IDENTITY";
        }
        case PacketOperationType::COUNTER:
        {
            return "COUNTER";
        }
        case PacketOperationType::TIMESTAMP:
        {
            return "TIMESTAMP";
        }
        default:
        {
            return "Error";
        }
    }
}

//
// Class functions.
//

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
            auto plan_tmp = generate_execution_plan<TCPSession>(script,
                                                                &arena_);

            // Handle unexpected value.
            if (!plan_tmp)
            {
                std::string e_msg = plan_tmp.error();
                Logger::error(std::move(e_msg));
                return 1;
            }

            ExecutionPlan<TCPSession> plan = *plan_tmp;

            // If we have dry_run set, do this and exit.
            if (cli_ops_.dry_run)
            {
                dry_run(plan, script);
                return 0;
            }

            bool ack = false;

            // Ensure the user knows what is about to happen.
            if (!cli_ops_.acknowledged_responsibility)
            {
                ack = request_acknowledgement(plan.dump_endpoint_list());
            }

            if (!ack)
            {
                return 0;
            }

            // Now, start the program's main loop
            return start_orchestrator_loop(std::move(plan));
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

    return 1;

}

template <typename Session>
int CLI::start_orchestrator_loop(ExecutionPlan<Session> plan)
{

    try
    {
        Orchestrator<Session> orchestrator(plan.actions,
                                           plan.payloads,
                                           plan.counter_steps,
                                           plan.config);

        orchestrator.start();
    }
    catch (const std::exception & error)
    {
        std::string e_msg = "Caught exception in orchestrator loop: "
                            + std::string(error.what());
        Logger::error(std::move(e_msg));

        return 1;
    }

    return 0;
}

template <typename Session>
void CLI::dry_run(const ExecutionPlan<Session> & plan,
                  const DSLData & data)
{
    const auto & actions_dsl = data.orchestrator.actions;

    Logger::info("            \033[1mStarting dry run\033[0m");

    uint32_t current_offset = 0;
    size_t current_payload_id = 0;

    for (size_t i = 0; i < plan.actions.size(); i++)
    {
        const auto & action = plan.actions[i];
        const auto & dsl_action = actions_dsl[i];

        current_offset += action.offset.count();

        std::string action_msg = ms_to_timestring(current_offset)
                                 + action.type_to_string()
                                 + " ";

        switch (action.type)
        {
            case ActionType::CREATE:
            {
                action_msg += std::to_string(action.count)
                              + " sessions";
                break;
            }
            case ActionType::CONNECT:
            {
                action_msg += "sessions indexed "
                              + std::to_string(action.sessions_start)
                              + " through "
                              + std::to_string(action.sessions_end);
                break;
            }
            case ActionType::SEND:
            {
                action_msg += "("
                              + std::to_string(action.count)
                              + "x) packet identity "
                              + dsl_action.packet_identifier
                              + " with payload data\n"
                              + "                ";

                // We need to store action.count copies of a payload
                // since the descriptors are cheap and this allows
                // straight up linear iteration versus branching.
                //
                // But, this must be accounted for when we parse the plan.
                if (current_payload_id > plan.payloads.size())
                {
                    Logger::warn("Application has a logic error");
                    break;
                }

                const auto & payload = plan.payloads[current_payload_id];

                for (const auto & op : payload.ops)
                {
                    action_msg += "<"
                                  + packet_op_to_str(op.type)
                                  + ", "
                                  + std::to_string(op.length)
                                  + "> ";
                }

                current_payload_id += action.count;
                break;
            }
            case ActionType::FLOOD:
            {
                action_msg += "sessions indexed "
                              + std::to_string(action.sessions_start)
                              + " through "
                              + std::to_string(action.sessions_end);
                break;
            }
            case ActionType::DRAIN:
            {
                action_msg += "sessions indexed "
                              + std::to_string(action.sessions_start)
                              + " through "
                              + std::to_string(action.sessions_end);
                break;
            }
            case ActionType::DISCONNECT:
            {
                action_msg += "sessions indexed "
                              + std::to_string(action.sessions_start)
                              + " through "
                              + std::to_string(action.sessions_end);
                break;
            }
            default:
            {
                break;
            }
        }

        Logger::info(std::move(action_msg));
    }

}

std::string_view ACKNOWLEDGEMENT_STRING_START =
            "\n"
            "\033[1;31mWARNING:\033[0m\n"
            "This tool can generate high network loads, rapid connection\n"
            "churn, and resource exhaustion if misused.\n"
            "\n"
            "The following endpoints are set to be used:\n";

std::string_view ACKNOWLEDGEMENT_STRING_END =
            "You \033[1mMUST\033[0m have explicit authorization to act "
                "on these systems.\n"
            "Unauthorized use of this tool can cause service disruption\n"
            "and may be illegal.\n"
            "\n"
            "If you are unsure whether you are authorized, stop now.\n"
            "\n"
            "By proceeding, you confirm that you:\n"
            " - Are authorized to act on these endpoints\n"
            " - Understand the behavior of this tool for your script\n"
            " - Accept full responsibility for its use\n"
            "\n"
            "To proceed, type \033[1mI UNDERSTAND\033[0m below.\n";

// Ensure the user knows what will happen before running.
bool CLI::request_acknowledgement(std::string endpoints_list)
{
    Logger::info(std::string(ACKNOWLEDGEMENT_STRING_START));

    Logger::info(endpoints_list);

    Logger::info(std::string(ACKNOWLEDGEMENT_STRING_END));

    // TODO: we will grab the user's response and check it is exactly equal.

    return false;
}
