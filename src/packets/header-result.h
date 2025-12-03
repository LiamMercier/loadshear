#pragma once

#include <cstddef>

struct HeaderResult
{
    enum class Status
    {
        OK = 0,
        ERROR,
        TIMEOUT
    };

    std::size_t length;
    Status status = Status::OK;
};
