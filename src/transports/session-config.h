#pragma once

struct SessionConfig {
    size_t header_size;
    size_t payload_size_limit;
    bool read_messages;

    SessionConfig(size_t h_size,
                  size_t p_size,
                  bool read)
    :header_size(h_size),
    payload_size_limit(p_size),
    read_messages(read)
    {}
};
