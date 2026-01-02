#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include <ftxui/dom/elements.hpp>

#include "color-scheme.h"
#include "numeric-strings.h"

const std::array<std::string, 16> latency_labels = {
    "64 ", "128", "256", "512", "1  ", "2  ",
    "4  ", "8  ", "16 ", "32 ", "64 ", "128",
    "256", "512", "1  ", "2  "
};

const std::array<std::string, 16> unit_labels = {
    "us ", "us ", "us ", "us ", "ms ", "ms ",
    "ms ", "ms ", "ms ", "ms ", "ms ", "ms ",
    "ms ", "ms ", "s  ", "s  "
};

static const std::array<ftxui::Color, 5> hist_colors =
                {
                    scheme_light_teal, scheme_teal, scheme_teal,
                    scheme_purple, scheme_red
                };

static const std::array<double, 5> hist_stops =
                {
                    0.0f, 0.35f, 0.55f,
                    0.80f, 1.0f
                };

// Fill the entire character space.
const std::string block_char = std::string(reinterpret_cast<const char*>(u8"\u2588"));

template<typename BucketType>
inline ftxui::Element generate_histogram(const BucketType & buckets,
                                         std::string title,
                                         int height = 8,
                                         int bin_width = 4)
{
    using namespace ftxui;

    uint64_t max_v = *std::max_element(buckets.begin(), buckets.end());
    int width = buckets.size();

    auto rgb_interp = [&](int column) -> Color {

        double t = double(column) / double(std::max(1, width - 1));

        // Ensure t is in [0, 1]
        if (t > 1.0)
        {
            t = 1.0;
        }
        else if (t < 0.0)
        {
            t = 0.0;
        }

        size_t i = 0;
        while (i < hist_stops.size() - 1 && t > hist_stops[i + 1])
        {
            i++;
        }

        if (i >= hist_colors.size() - 1)
        {
            return hist_colors.back();
        }

        double t_segment = (t - hist_stops[i]) / (hist_stops[i+1] - hist_stops[i]);

        return Color::Interpolate(t_segment, hist_colors[i], hist_colors[i+1]);

    };

    std::vector<Element> columns;
    columns.reserve(buckets.size());

    for (size_t i = 0; i < buckets.size(); i++)
    {
        uint64_t val = buckets[i];

        // Calculate how many characters to fill.
        int fill = 0;
        if (max_v > 0)
        {
            fill = static_cast<int>
                        (
                            std::round((static_cast<double>(val)
                                        / static_cast<double>(max_v))
                                        * height)
                        );

            if (fill > height)
            {
                fill = height;
            }
        }

        std::vector<Element> rows;

        // Reserve data for each bar, one gap, and labels.
        rows.reserve(height + 3);

        // build rows top to bottom based on fill.
        for (int r = height - 1; r >= 0; r--)
        {
            if (r < fill)
            {
                std::string block;

                for (int n = 0; n < bin_width; n++)
                {
                    block += block_char;
                }

                rows.push_back(text(block) | color(rgb_interp(i)));
            }
            // Empty column.
            else
            {
                rows.push_back(text(std::string(bin_width, ' ')) | center);
            }
        }

        // Add one gap.
        rows.push_back(text(std::string(bin_width, ' ')) | center);

        // Add labels.
        rows.push_back(text(latency_labels[i]) | center);
        rows.push_back(text(unit_labels[i]) | center);

        // push back column.
        columns.push_back(vbox(std::move(rows)));
    }

    // Build the Y axis.
    std::vector<Element> y_rows;
    y_rows.reserve(height + 3);

    const int y_label_width = 6;
    int row_tick_counter = 0;

    // Build the y-axis rows top to bottom like with the data rows.
    for (int row = height - 1; row >= 0; row--)
    {
        // Only insert a tick every 4 rows.
        if (!(row_tick_counter == 0))
        {
            y_rows.push_back(text(std::string(y_label_width, ' ')) | center);
            row_tick_counter--;
            continue;
        }
        // On the 4th row, insert a tick.
        else
        {
            // Compute the value of this row.
            double frac = static_cast<double>(row)
                          / static_cast<double>(height - 1);

            uint64_t label_val = static_cast<uint64_t>
                                    (
                                        std::round(frac * static_cast<double>(max_v))
                                    );

            // Generate a readable version of the value.
            std::string lab = y_axis_text(label_val);

            y_rows.push_back(text(std::move(lab)) | center);

            row_tick_counter = 3;
            continue;
        }
    }

    // Insert three empty rows for the gap + 2 label rows.
    y_rows.push_back(text(std::string(y_label_width, ' ')) | center);
    y_rows.push_back(text(std::string(y_label_width, ' ')) | center);
    y_rows.push_back(text(std::string(y_label_width, ' ')) | center);

    auto y_axis = vbox(std::move(y_rows));

    // Return our histogram.
    auto body = hbox({
        y_axis,
        text(" "),
        hbox(std::move(columns))
    });

    auto hist = vbox({
        text(title) | bold | center,
        separator(),
        body
    });

    return hist;
}
