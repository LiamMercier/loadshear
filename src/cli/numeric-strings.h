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

inline std::string bytes_display_string(int64_t value)
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

inline std::string decimal_suffix_string(int64_t value)
{
    std::string res;
    res.reserve(4);

    // transform [1000000, 9999999] -> ["1.0M", "9.9M"]
    if (value >= 1000 * 1000
        && value < 10 * 1000 * 1000)
    {
        int millions = value / (1000 * 1000);
        int hundred_thousands = (value % (1000 * 1000))
                                / (100 * 1000);

        res += std::to_string(millions);
        res += ".";
        res += std::to_string(hundred_thousands);
        res += "M";
    }
    // transform [10000, 999999] -> ["10k", "999k"]
    else if (value >= 10 * 1000
                && value < 1000 * 1000)
    {
        int thousands = value / 1000;

        res += std::to_string(thousands);
        res += "k";
    }
    // transform [1000, 9999] -> ["1.0k", "9.9k"]
    else if (value >= 1000
                && value < 10000)
    {
        int thousands = value / 1000;
        int hundreds = (value % 1000) / 100;

        res += std::to_string(thousands);
        res += ".";
        res += std::to_string(hundreds);
        res += "k";
    }
    // Display [0, 999] as is.
    else
    {
        res += std::to_string(value);
    }

    return res;
}

// Fetch label string, slightly different because of spacing.
inline std::string y_axis_text(uint64_t value)
{
    std::string res;
    res.reserve(6);

    res += " ";

    // transform [1000000, 9999999] -> ["1.0M", "9.9M"]
    if (value >= 1000 * 1000
        && value < 10 * 1000 * 1000)
    {
        int millions = value / (1000 * 1000);
        int hundred_thousands = (value % (1000 * 1000))
                                / (100 * 1000);

        res += std::to_string(millions);
        res += ".";
        res += std::to_string(hundred_thousands);
        res += "M";
    }
    // transform [10000, 999999] -> [" 10k", "999k"]
    else if (value >= 10 * 1000
                && value < 1000 * 1000)
    {
        int thousands = value / 1000;

        res += std::to_string(thousands);
        res += "k";
    }
    // transform [1000, 9999] -> ["1.0k", "9.9k"]
    else if (value >= 1000
                && value < 10000)
    {
        int thousands = value / 1000;
        int hundreds = (value % 1000) / 100;

        res += std::to_string(thousands);
        res += ".";
        res += std::to_string(hundreds);
        res += "k";
    }
    // Display [0, 999] as is.
    else
    {
        res += std::to_string(value);

        size_t padding = 5 - res.size();

        res += std::string(padding, ' ');
    }

    res += " ";
    return res;
};
