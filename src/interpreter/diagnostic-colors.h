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
