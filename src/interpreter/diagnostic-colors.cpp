#include "diagnostic-colors.h"

constexpr std::string_view ANSI_RESET = "\033[0m";

// Color choices:
//
// Linux users typically have their own command line color schemes, which we
// will respect by only omitting ansi 16-color values.
//
// - Errors should be red
// - Expected values should be green
// - Keywords should be bolded versions of standard text
// - Bad fields should be darker red and underlined
// - Bad values should be bright red
// - Limits should be orange
// - References should be a cyan or magenta style color
// - Context should be a darker version of text

constexpr std::string_view PALETTE_ERROR = "\033[31m";
constexpr std::string_view PALETTE_EXPECTED = "\033[32m";
constexpr std::string_view PALETTE_KEYWORD = "\033[1m";
constexpr std::string_view PALETTE_CONTEXT = "\033[95m";
constexpr std::string_view PALETTE_BAD_FIELD = "\033[4;31m";
constexpr std::string_view PALETTE_BAD_VALUE = "\033[91m";
constexpr std::string_view PALETTE_REFERENCE = "\033[36m";
constexpr std::string_view PALETTE_LIMITS = "\033[33m";

std::string apply_palette(std::string input, std::string_view palette)
{
    std::string out;

    out.reserve(palette.size()
                + input.size()
                + ANSI_RESET.size());

    out.append(palette);
    out.append(input);
    out.append(ANSI_RESET);

    return out;
}

std::string styled_string(std::string input, PrintStyle style)
{
    switch (style)
    {
        case PrintStyle::Error:
        {
            return apply_palette(input, PALETTE_ERROR);
        }
        case PrintStyle::Expected:
        {
            return apply_palette(input, PALETTE_EXPECTED);
        }
        // Bold but not colored for context clues
        case PrintStyle::Keyword:
        {
            return apply_palette(input, PALETTE_KEYWORD);
        }
        // Used sparsely
        case PrintStyle::Context:
        {
            return apply_palette(input, PALETTE_CONTEXT);
        }
        // Red and underlined for bad field
        case PrintStyle::BadField:
        {
            return apply_palette(input, PALETTE_BAD_FIELD);
        }
        // Bright red to show error
        case PrintStyle::BadValue:
        {
            return apply_palette(input, PALETTE_BAD_VALUE);
        }
        // Reference to identifiers or other script parts for context.
        case PrintStyle::Reference:
        {
            return apply_palette(input, PALETTE_REFERENCE);
        }
        // Maybe orange? Already seems to be pretty orange though.
        case PrintStyle::Limits:
        {
            return apply_palette(input, PALETTE_LIMITS);
        }
    }

    return input;
}
