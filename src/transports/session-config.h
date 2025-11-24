#pragma once

struct SessionConfig {
    size_t header_size;
    size_t payload_size_limit;

    SessionConfig(size_t h_size,
                  size_t r_size,
                  size_t p_size)
    :header_size(h_size),
    payload_size_limit(p_size)
    {}
};
