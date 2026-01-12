// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include <iostream>

#include "cli.h"
#include "logger.h"
#include "version.h"

int main(int argc, char** argv)
{
    // Start the logger.
    Logger::init(LogLevel::INFO);

    // First, parse command line flags.
    CLIParseResult parse_res = parse_cli(argc, argv);

    if (!parse_res.good_parse())
    {
        return parse_res.status_code();
    }

    CLIOptions c_ops = std::move(parse_res.options);

    Logger::info(std::string{VERSION_PRINTSTRING});

    // Now try to run the tool.
    std::unique_ptr<CLI> cli;

    try
    {
        cli = std::make_unique<CLI>(std::move(c_ops));
    }
    catch (const std::exception & error)
    {
        std::string e_str = std::string("Tried to start program (got error: ")
                            + error.what()
                            + ")";

        Logger::error(std::move(e_str));
        Logger::shutdown();
        return 1;
    }

    int cli_res = cli->run();

    // Turn the logger off.
    Logger::shutdown();

    return cli_res;
}
