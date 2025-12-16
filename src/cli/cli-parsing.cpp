#include "cli-parsing.h"

#include <iostream>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::optional<CLIOptions> parse_cli(int argc, char** argv)
{
    CLIOptions cli_ops;

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
         "Expand environment variables in script paths.");

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
            return std::nullopt;
        }
    }
    catch (const po::error & p_err)
    {
        std::cerr << "Error: " << p_err.what() << "\n";
        return std::nullopt;
    }

    return cli_ops;
}
