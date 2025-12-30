#pragma once

#include <string>
#include <sstream>

#include <ftxui/dom/elements.hpp>

static inline std::string numeric_display_string(int64_t value)
{
    constexpr int max_index = 5;
    constexpr std::array<std::string_view, max_index> units
        = { "B", "KiB", "MiB", "GiB", "TiB"};

    // Convert the value into B, KiB, MiB, GiB, TiB to one decimal place.

    double v = static_cast<double>(value);
    int indx = 0;
    while (v >= 1024.0 && indx < max_index)
    {
        v /= 1024.0;
        indx++;
    }

    std::ostringstream oss;
    if (indx == 0)
    {
        oss << static_cast<int64_t>(v) << " " << std::string(units[indx]);
        return oss.str();
    }

    oss << std::fixed << std::setprecision(1) << v;

    std::string res = oss.str() + " " + std::string(units[indx]);
    return res;
}

inline ftxui::Element create_bytes_display(std::string title,
                                           uint64_t value,
                                           int64_t diff)
{
    using namespace ftxui;

    // Show the change in bytes written.
    Element diff_element;

    if (diff > 0)
    {
        diff_element = text(" +" + numeric_display_string(diff))
                    | color(Color::Green);
    }
    else
    {
        diff_element = text(" ");
    }

    auto numeric_element = hbox({
        text(title),
        text(numeric_display_string(value)),
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
        diff_element = text(" +" + std::to_string(diff))
                       | color(Color::Green);
    }
    else if (diff < 0)
    {
        diff_element = text(" +" + std::to_string(diff))
                       | color(Color::Red);
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
