#pragma once

#include "header-result.h"

struct MessageHandler
{
public:
    virtual void parse_body_async(std::span<const uint8_t> buffer,
                                  std::function<void()> callback) const = 0;

    virtual HeaderResult parse_header(std::span<const uint8_t> buffer) const = 0;

    virtual ~MessageHandler() = default;

private:

};
