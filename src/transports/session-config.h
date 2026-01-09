#pragma once

struct SessionConfig {
    size_t header_size;
    size_t payload_size_limit;
    bool read_messages;
    bool loop_payloads;
    // How often we should sample packet latencies.
    uint32_t packet_sample_rate;

    SessionConfig(size_t h_size,
                  size_t p_size,
                  bool read,
                  bool loop,
                  uint64_t sample_rate)
    :header_size(h_size),
    payload_size_limit(p_size),
    read_messages(read),
    loop_payloads(loop),
    packet_sample_rate(sample_rate)
    {}
};

// We set the default ring buffer to 4 KiB for reading small messages.
//
// Expected memory usage:
//
// Sessions |   Memory
// ---------------------
//      100 |   0.39 MiB
//     1000 |   3.91 MiB
//     5000 |  19.15 MiB
//    10000 |  39.06 MiB
//    20000 |  78.13 MiB
//    30000 | 117.19 MiB
//    50000 | 195.31 MiB
//   100000 | 390.63 MiB
//
constexpr size_t MESSAGE_BUFFER_SIZE = 4 * 1024;
