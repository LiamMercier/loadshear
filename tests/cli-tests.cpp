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

#include <gtest/gtest.h>

#include "test-helpers.h"
#include "test-fixtures.h"

TEST(CLITests, ValidTCPPlanGeneration)
{
    DSLData correct_data = get_simple_valid_script_data();

    std::pmr::monotonic_buffer_resource arena(8 * 1024 * 1024,
                                              std::pmr::get_default_resource());

    auto correct_plan = get_simple_valid_script_plan(&arena);

    std::pmr::monotonic_buffer_resource arena2(8 * 1024 * 1024,
                                               std::pmr::get_default_resource());

    auto temporary_plan = generate_execution_plan<TCPSession>(correct_data,
                                                              &arena2);
    if (!temporary_plan)
    {
        FAIL() << "Couldn't generate plan!";
    }

    auto issues = EXPECT_PLAN_EQ<TCPSession>(correct_plan, *temporary_plan);

    if (!issues.empty())
    {
        std::string issues_str = "Got errors in comparison:\n";

        for (auto & issue : issues)
        {
            issues_str.append(issue);
            issues_str.append("\n");
        }

        FAIL() << issues_str;
    }
}
