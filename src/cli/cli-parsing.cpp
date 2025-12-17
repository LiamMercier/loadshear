#include "cli-parsing.h"

#include <iostream>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

CLIParseResult parse_cli(int argc, char** argv)
{
    CLIParseResult res;
    CLIOptions & cli_ops = res.options;

    po::options_description op_desc("Options");
    op_desc.add_options()
        ("help,h",
         "Show options.")
        ("script,s",
         po::value<std::string>(&cli_ops.script_file),
         "Path to your script.")
        ("dry-run,dry,d",
         po::bool_switch(&cli_ops.dry_run)->default_value(false),
         "Show runtime plan generated from your script and options.")
        ("expand-envs,e",
         po::bool_switch(&cli_ops.expand_envs)->default_value(false),
         "Expand environment variables in script paths.")
        ("arena-init-mb",
         po::value<uint64_t>(&cli_ops.arena_init_mb),
         "Initial size of arena allocator for packet data.");

    po::positional_options_description pos_desc;
    pos_desc.add("script", 1);

    po::variables_map var_map;
    try
    {
        po::store(po::command_line_parser(argc, argv)
                        .options(op_desc)
                        .positional(pos_desc)
                        .run(),
                  var_map);

        po::notify(var_map);

        if (var_map.count("help") || cli_ops.script_file.empty())
        {
            std::cout << "\nUsage: "
                      << "loadshear"
                      << " <script_file> [options]\n\n"
                      << op_desc
                      << "\n";

            res.status = CLIParseResult::ParseStatus::Help;
            return res;
        }
    }
    catch (const po::error & p_err)
    {
        std::cerr << "Error: " << p_err.what() << "\n";
        res.status = CLIParseResult::ParseStatus::Error;
        return res;
    }

    return res;
}
