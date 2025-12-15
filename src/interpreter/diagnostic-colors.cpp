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
constexpr std::string_view PALETTE_CONTEXT = "\033[90m";
constexpr std::string_view PALETTE_BAD_FIELD = "\033[4;31m";
constexpr std::string_view PALETTE_BAD_VALUE = "\033[91m";
constexpr std::string_view PALETTE_REFERENCE = "\033[36m";
constexpr std::string_view PALETTE_LIMITS = "\033[33m";

std::string styled_string(std::string input, PrintStyle style)
{
    switch (style)
    {
        case PrintStyle::Error:
        {
            std::string out;

            out.reserve(PALETTE_ERROR.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_ERROR);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        case PrintStyle::Expected:
        {
            std::string out;

            out.reserve(PALETTE_EXPECTED.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_EXPECTED);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Bold but not colored for context clues
        case PrintStyle::Keyword:
        {
            std::string out;

            out.reserve(PALETTE_KEYWORD.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_KEYWORD);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Used sparsely
        case PrintStyle::Context:
        {
            std::string out;

            out.reserve(PALETTE_CONTEXT.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_CONTEXT);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Red and underlined for bad field
        case PrintStyle::BadField:
        {
            std::string out;

            out.reserve(PALETTE_BAD_FIELD.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_BAD_FIELD);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Bright red to show error
        case PrintStyle::BadValue:
        {
            std::string out;

            out.reserve(PALETTE_BAD_VALUE.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_BAD_VALUE);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Reference to identifiers or other script parts for context.
        case PrintStyle::Reference:
        {
            std::string out;

            out.reserve(PALETTE_REFERENCE.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_REFERENCE);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
        // Maybe orange? Already seems to be pretty orange though.
        case PrintStyle::Limits:
        {
            std::string out;

            out.reserve(PALETTE_LIMITS.size()
                        + input.size()
                        + ANSI_RESET.size());

            out.append(PALETTE_LIMITS);
            out.append(input);
            out.append(ANSI_RESET);

            return out;
        }
    }

    return input;
}
