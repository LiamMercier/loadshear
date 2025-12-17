#include <gtest/gtest.h>

#include "test-helpers.h"
#include "test-fixtures.h"

TEST(CLITests, ValidTCPPlanGeneration)
{
    DSLData correct_data = get_simple_valid_script_data();

    std::pmr::monotonic_buffer_resource arena(8 * 1024 * 1024,
                                              std::pmr::get_default_resource());

    ExecutionPlan correct_plan = get_simple_valid_script_plan(&arena);

    std::pmr::monotonic_buffer_resource arena2(8 * 1024 * 1024,
                                               std::pmr::get_default_resource());

    ExecutionPlan generated_plan = generate_execution_plan(correct_data,
                                                           &arena2);

    EXPECT_PLAN_EQ(correct_plan,
                   generated_plan) << "ExecutionPlan values not equal!";
}
