#pragma once

struct MessageHandler
{
public:
    virtual void parse_body_async(std::span<const uint8_t> buffer,
                                  std::function<void()> callback) = 0;

    virtual void parse_header(std::span<const uint8_t> buffer) = 0;

    virtual ~MessageHandler() = default;
private:

};
