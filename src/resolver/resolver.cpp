#include "resolver.h"

#include <iostream>

namespace Resolver
{
    // Only set at startup.
    ResolverOptions global_options;

    void set_global_resolve_options(ResolverOptions options)
    {
        global_options = std::move(options);
    }

    // Turn every $ENV into the corresponding variable.
    std::string expand_env_variables(const std::string & in_path)
    {
        std::string out_path;

        bool finding_env = false;
        std::string current_env_str;

        for (char c : in_path)
        {
            if (c == '$')
            {
                // Set finding_env and start looking for the variable name.
                finding_env = true;
                current_env_str.clear();
                continue;
            }
            if (c == '/')
            {
                if (finding_env)
                {
                    finding_env = false;
                    const char* env_str = std::getenv(current_env_str.data());
                    // If this is empty, we will fail to resolve anyways.
                    if (env_str)
                    {
                        out_path.append(env_str);
                    }
                }
            }

            if (!finding_env)
            {
                out_path.push_back(c);
            }
            else
            {
                current_env_str.push_back(c);
            }
        }

        return out_path;
    }

    // Expand ~/ to the user's home.
    std::string expand_tilde(std::string in_path)
    {
        if (in_path.empty())
        {
            return in_path;
        }

        // If we should expand, do it.
        if (in_path.substr(0, 2) == "~/")
        {
            const char* home = std::getenv("HOME");

            // If no $HOME then we append nothing, should usually not happen.
            if (home)
            {
                // Convert to string and combine.
                //
                // We will need to include the / since $HOME doesn't have it.
                return std::string(home) + in_path.substr(1);
            }
        }

        // If we didn't replace anything, just return.
        return in_path;
    }

    fs::path resolve_file(const std::string & raw_file,
                          std::string & error_string)
    {
        std::string expanded = raw_file;

        // Expand env variables only if we set this true at startup.
        if (global_options.expand_envs)
        {
            expanded = expand_env_variables(expanded);
        }

        // Always expand ~ and ~/
        expanded = expand_tilde(expanded);

        std::error_code ec;

        fs::path out_path = fs::canonical(fs::path(expanded),
                                          ec);

        if (ec)
        {
            error_string = ec.message();
        }

        return out_path;
    }

    size_t get_file_size(fs::path file)
    {
        std::error_code ec;

        auto size = fs::file_size(file, ec);

        if (ec)
        {
            return 0;
        }

        return size;
    }
}
