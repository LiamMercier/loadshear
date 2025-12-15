#include <iostream>

#include <boost/program_options.hpp>

#include "interpreter.h"
#include "resolver.h"

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    // Command line options.
    std::string script_file;

    bool dry_run = false;
    bool expand_envs = false;

    // First, parse command line flags.
    po::options_description op_desc("Options");
    op_desc.add_options()
        ("help,h",
         "Show options.")
        ("script,s",
         po::value<std::string>(&script_file),
         "Path to your script.")
        ("dry-run,dry,d",
         po::bool_switch(&dry_run)->default_value(false),
         "Show runtime plan generated from your script and options.")
        ("expand-envs,e",
         po::bool_switch(&expand_envs)->default_value(false),
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

        if (var_map.count("help") || script_file.empty())
        {
            std::cout << "\nUsage: "
                      << "loadshear"
                      << " <script_file> [options]\n\n"
                      << op_desc
                      << "\n";
            return 0;
        }
    }
    catch (const po::error & p_err)
    {
        std::cerr << "Error: " << p_err.what() << "\n";
        return 1;
    }

    // Handle anything we need to do with our options now.

    ResolverOptions resolver_ops;

    if (expand_envs)
    {
        resolver_ops.expand_env = true;
    }

    Resolver::set_global_resolve_options(resolver_ops);

    return 0;
}
