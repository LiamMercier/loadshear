#include "resolver.h"

#include <iostream>
#include <fstream>

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

    uintmax_t get_file_size(const fs::path & path)
    {
        std::error_code ec;

        auto size = fs::file_size(path, ec);

        if (ec)
        {
            return 0;
        }

        return size;
    }

    std::vector<uint8_t> read_binary_file(const fs::path & path,
                                          std::string & error_string)
    {
        uintmax_t file_size = get_file_size(path);

        // Stop now if file has zero bytes, the program should
        // terminate if the file is important.
        if (file_size == 0)
        {
            error_string = "File "
                           + path.string()
                           + " had zero bytes to read!";
            return {};
        }
        else if (file_size >= SIZE_MAX)
        {
            error_string = "File "
                           + path.string()
                           + " has more than SIZE_T bytes!";
            return {};
        }

        std::vector<uint8_t> bytes(file_size);

        std::ifstream file(path, std::ios::binary);

        // Try to read, give error if we fail.
        if (!file.read(reinterpret_cast<char *>(bytes.data()), file_size))
        {
            error_string = "Failed to read file "
                           + path.string();
            return {};
        }

        return bytes;
    }

    std::string read_bytes_to_contiguous(fs::path path,
                                         std::span<uint8_t> buffer)
    {
        std::ifstream file(path, std::ios::binary);

        // Try to read, give error if we fail.
        if (!file.read(reinterpret_cast<char *>(buffer.data()),
                                                buffer.size()))
        {
            return "Failed to read file "
                   + path.string();
        }

        return {};
    }
}
