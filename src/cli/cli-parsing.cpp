#include "cli-parsing.h"

#include <iostream>

#include <boost/program_options.hpp>

#include "logger.h"
#include "version.h"

namespace po = boost::program_options;

CLIParseResult parse_cli(int argc, char** argv)
{
    CLIParseResult res;
    CLIOptions & cli_ops = res.options;

    po::options_description op_desc("Options");
    op_desc.add_options()
        ("help,h",
         "Show options.")
        ("version,v",
         "Show version information")
        ("script,s",
         po::value<std::string>(&cli_ops.script_file),
         "Path to your script.")
        ("dry-run,d",
         po::bool_switch(&cli_ops.dry_run)
                          ->default_value(false),
         "Show runtime plan generated from your script and options.")
        ("expand-envs,e",
         po::bool_switch(&cli_ops.expand_envs)
                          ->default_value(false),
         "Expand environment variables in script paths.")
        ("acknowledge",
         po::bool_switch(&cli_ops.acknowledged_responsibility)
                          ->default_value(false),
         "Automatically acknowledge legal responsibility.")
        ("quiet",
         po::bool_switch(&cli_ops.quiet)
                          ->default_value(false),
         "Only show warnings/errors after acknowledgement")
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

        if (var_map.count("version"))
        {
            Logger::info(std::string{VERSION_PRINTSTRING});

            res.status = CLIParseResult::ParseStatus::Version;
            return res;
        }
        else if (var_map.count("help") || cli_ops.script_file.empty())
        {
            std::ostringstream oss;
            oss << op_desc;

            std::string msg = "\nUsage: "
                              "loadshear"
                              " <script_file> [options]\n\n"
                              + oss.str()
                              + "\n";
            Logger::info(std::move(msg));

            res.status = CLIParseResult::ParseStatus::Help;
            return res;
        }
    }
    catch (const po::error & p_err)
    {
        std::string e_string = "Error: "
                               + std::string(p_err.what());
        Logger::error(std::move(e_string));

        res.status = CLIParseResult::ParseStatus::Error;
        return res;
    }

    res.status = CLIParseResult::ParseStatus::Ok;
    return res;
}
