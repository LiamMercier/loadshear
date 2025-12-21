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
