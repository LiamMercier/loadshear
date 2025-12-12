#include <gtest/gtest.h>

#include "test-helpers.h"
#include "resolver.h"

namespace Resolver
{
    std::string expand_env_variables(const std::string & in_path);

    std::string expand_tilde(std::string in_path);
}

TEST(ResolverTests, EnvVariablesTest)
{
    ResolverOptions options;
    options.expand_env = true;
    Resolver::set_global_resolve_options(options);
    {
        // Should not resolve to anything.
        std::string example_path = "$HOME/test/$HOME/mytest.txt";
        std::string error_string;

        std::filesystem::path file = Resolver::resolve_file(example_path,
                                                            error_string);

        EXPECT_TRUE(error_string.size() > 0);
    }

    {
        std::string example_path = "$HOME/test/mytest.txt";
        std::string error_string;

        std::filesystem::path file = Resolver::resolve_file(example_path,
                                                            error_string);

        std::string comp_string = std::getenv("HOME");
        comp_string.append("/test/mytest.txt");

        EXPECT_EQ(file, comp_string);
    }

    Resolver::expand_tilde("");

}
