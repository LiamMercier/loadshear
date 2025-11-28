#pragma once

struct SessionConfig {
    size_t header_size;
    size_t payload_size_limit;
    bool read_messages;
    bool loop_payloads;

    SessionConfig(size_t h_size,
                  size_t p_size,
                  bool read,
                  bool loop)
    :header_size(h_size),
    payload_size_limit(p_size),
    read_messages(read),
    loop_payloads(loop)
    {}
};
