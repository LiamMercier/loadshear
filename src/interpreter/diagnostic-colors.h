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

#include <cstdint>
#include <string>

enum class PrintStyle : uint8_t
{
    // Error header.
    Error,

    // For expected values.
    Expected,

    // Language specific values that are referenced.
    Keyword,

    // Background information, i.e "in packet p1"
    Context,

    // The subject of the error (i.e ORCHESTRATOR missing something)
    BadField,

    // The offending value (i.e 13, bad file path, bad token/text)
    BadValue,

    // Identifier or other reference (i.e packet name)
    Reference,

    // For bounds or constraints (i.e exceeded max index)
    Limits
};

std::string styled_string(std::string input, PrintStyle style);
