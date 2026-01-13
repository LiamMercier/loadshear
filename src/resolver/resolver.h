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

#pragma once

#include <filesystem>
#include <cstdlib>
#include <vector>
#include <span>

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
