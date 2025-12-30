#pragma once

#include <ftxui/dom/elements.hpp>

inline ftxui::Element create_numeric_display(std::string title,
                                             uint64_t value,
                                             uint64_t diff)
{
    using namespace ftxui;

    // Show the change in bytes written.
    Element diff_element;

    if (diff > 0)
    {
        diff_element = text(" +" + std::to_string(diff))
                    | color(Color::Green);
    }
    else
    {
        diff_element = text(" ");
    }

    auto numeric_element = hbox({
        text(title),
        text(std::to_string(value)),
        diff_element
    });

    return numeric_element;
}
