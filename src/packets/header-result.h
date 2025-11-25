#pragma once

#include <cstddef>

struct HeaderResult
{
    enum class Status
    {
        OK,
        ERROR,
        TIMEOUT
    };

    std::size_t length;
    Status status = Status::OK;
};
