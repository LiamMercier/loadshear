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
#include "resolver.h"

namespace Resolver
{
    std::string expand_env_variables(const std::string & in_path);

    std::string expand_tilde(std::string in_path);
}

TEST(ResolverTests, EnvVariablesTest)
{
    ResolverOptions options;
    options.expand_envs = true;
    Resolver::set_global_resolve_options(options);

    {
        // Should not resolve to anything.
        std::string example_path = "$HOME/test/$HOME/no-file-here.txt";
        std::string error_string;

        std::filesystem::path file = Resolver::resolve_file(example_path,
                                                            error_string);

        EXPECT_TRUE(error_string.size() > 0);

        std::string expanded_env = Resolver::expand_env_variables(example_path);

        std::string comp_string = std::getenv("HOME");
        comp_string.append("/test/");
        comp_string.append(std::getenv("HOME"));
        comp_string.append("/no-file-here.txt");

        EXPECT_EQ(expanded_env, comp_string);
    }

    {
        std::string example_path = "$HOME/test/no-file-here.txt";
        std::string error_string;

        std::filesystem::path file = Resolver::resolve_file(example_path,
                                                            error_string);

        EXPECT_TRUE(error_string.size() > 0);

        std::string expanded_env = Resolver::expand_env_variables(example_path);

        std::string comp_string = std::getenv("HOME");
        comp_string.append("/test/no-file-here.txt");

        EXPECT_EQ(expanded_env, comp_string);
    }
}

TEST(ResolverTests, TildesTest)
{
    {
        std::string example_path = "~/test/no-file-here.txt";
        std::string expanded_env = Resolver::expand_tilde(example_path);

        std::string comp_string = std::getenv("HOME");
        comp_string.append("/test/no-file-here.txt");

        EXPECT_EQ(expanded_env, comp_string);
    }
}

TEST(ResolverTests, ResolveFilesTest)
{
    {
        std::string valid_script = "tests/scripts/simple-valid-script.ldsh";
        std::string error_string;

        std::filesystem::path path = Resolver::resolve_file(valid_script,
                                                            error_string);

        // Ensure file was found.
        EXPECT_NE(path, std::filesystem::path());
        EXPECT_TRUE(error_string.size() == 0);
    }

    {
        std::string valid_script = "tests/packets/test-packet-1.bin";
        std::string error_string;

        std::filesystem::path path = Resolver::resolve_file(valid_script,
                                                            error_string);

        // Ensure file was found.
        EXPECT_NE(path, std::filesystem::path());
        EXPECT_TRUE(error_string.size() == 0);
    }

    {
        std::string valid_script = "tests/packets/test-packet-heavy.bin";
        std::string error_string;

        std::filesystem::path path = Resolver::resolve_file(valid_script,
                                                            error_string);

        // Ensure file was found.
        EXPECT_NE(path, std::filesystem::path());
        EXPECT_TRUE(error_string.size() == 0);
    }
}
