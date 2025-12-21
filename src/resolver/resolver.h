#pragma once

#include <filesystem>
#include <cstdlib>
#include <vector>

#include "resolver-options.h"

// Resolver is used at startup to find files and read them.
namespace Resolver
{
    namespace fs = std::filesystem;

    fs::path resolve_file(const std::string & raw_file,
                          std::string & error_string);

    void set_global_resolve_options(ResolverOptions options);

    uintmax_t get_file_size(const fs::path & path);

    std::vector<uint8_t> read_binary_file(const fs::path & path,
                                          std::string & error_string);

    std::string read_bytes_to_contiguous(fs::path path,
                                         std::span<uint8_t> buffer);
}
