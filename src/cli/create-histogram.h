#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include <ftxui/dom/elements.hpp>

const std::vector<std::string> latency_labels = {
    "64 ", "128", "256", "512", "1  ", "2  ",
    "4  ", "8  ", "16 ", "32 ", "64 ", "128",
    "256", "512", "1  ", "2  "
};

const std::vector<std::string> unit_labels = {
    "us ", "us ", "us ", "us ", "ms ", "ms ",
    "ms ", "ms ", "ms ", "ms ", "ms ", "ms ",
    "ms ", "ms ", "s  ", "s  "
};

// Fill the entire character space.
const std::string block_char = std::string(reinterpret_cast<const char*>(u8"\u2588"));

// TODO: fine tuned styling that looks better, maybe cyan color scheme instead?
template<typename BucketType>
inline ftxui::Element generate_histogram(const BucketType & buckets,
                                         std::string title,
                                         int height = 8,
                                         int bin_width = 3)
{
    using namespace ftxui;

    uint64_t max_v = *std::max_element(buckets.begin(), buckets.end());
    int width = buckets.size();

    // Fetch label string
    // TODO: handle larger values for stuff like packet latency.
    auto y_axis_text = [](uint64_t value) -> std::string {
        std::string res;
        res.reserve(6);

        res += " ";

        // transform [10000, 99999] -> [" 10k", " 99k"]
        if (value >= 10 * 1000
            && value < 100 * 1000)
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

    // Lambda to quickly decide on LightGreen, Green, Yellow, Red
    auto rgb_interp = [&](int column) -> Color {

        double t = double(column) / double(std::max(1, width - 1));

        if (t < 0.5)
        {
            return (t < 0.25) ? Color(Color::Green) : Color(Color::LightGreen);
        }
        else
        {
            return (t < 0.75) ? Color(Color::Yellow) : Color(Color::Red);
        }

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

    // Place all the columns beside one another.
    std::vector<Element> spaced_columns;
    for (size_t i = 0; i < columns.size(); i++)
    {
        spaced_columns.push_back(columns[i]);
        if (i + 1 < columns.size())
        {
            spaced_columns.push_back(text(" "));
        }
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
        hbox(std::move(spaced_columns))
    });

    auto hist = vbox({
        text(title) | bold | center,
        separator(),
        body
    }) | border;

    return hist;
}
