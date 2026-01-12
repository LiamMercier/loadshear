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

#include <string>
#include <sstream>

#include <ftxui/dom/elements.hpp>

#include "color-scheme.h"
#include "numeric-strings.h"

inline ftxui::Element create_bytes_display(std::string title,
                                           uint64_t value,
                                           int64_t diff)
{
    using namespace ftxui;

    // Show the change in bytes written.
    Element diff_element;

    if (diff > 0)
    {
        diff_element = text(" +" + bytes_display_string(diff))
                    | color(scheme_light_teal);
    }
    else
    {
        diff_element = text(" ");
    }

    auto numeric_element = hbox({
        text(title),
        text(bytes_display_string(value)),
        diff_element
    });

    return numeric_element;
}

inline ftxui::Element create_numeric_display(std::string title,
                                             uint64_t value,
                                             int64_t diff)
{
    using namespace ftxui;

    // Show the change in bytes written.
    Element diff_element;

    if (diff > 0)
    {
        diff_element = text(" +" + decimal_suffix_string(diff))
                       | color(scheme_light_teal);
    }
    else if (diff < 0)
    {
        diff_element = text(" +" + decimal_suffix_string(diff))
                       | color(scheme_red);
    }
    else
    {
        diff_element = text(" ");
    }

    auto numeric_element = hbox({
        text(title),
        text(decimal_suffix_string(value)),
        diff_element
    });

    return numeric_element;
}
