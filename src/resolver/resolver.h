#pragma once

#include <filesystem>
#include <cstdlib>

#include "resolver-options.h"

// Resolver is used at startup to find files and read them.
namespace Resolver
{
    namespace fs = std::filesystem;

    fs::path resolve_file(const std::string & raw_file,
                          std::string & error_string);

    void set_global_resolve_options(ResolverOptions options);
}
