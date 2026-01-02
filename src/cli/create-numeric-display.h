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
