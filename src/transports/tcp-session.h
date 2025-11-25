#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "session-config.h"
#include "message-handler-interface.h"

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

namespace asio = boost::asio;

// Performance-aware TCP session class to be stored in a SessionPool.
//
// Assumptions:
// - The underlying SessionPool will not destroy the session until session is safely closed
// - Messages that are shared across session instances are read only
// - Server packets will use an abstract interface to parse using user-defined WASM or defaults
// - MessageInterface is alive until all Sessions in the SessionPool are destroyed
class TCPSession
{
public:
    using tcp = asio::ip::tcp;

public:
    TCPSession(asio::io_context & cntx,
               const SessionConfig & config,
               const MessageHandler & message_handler);

    // This class should not be moved or copied.
    TCPSession(const TCPSession &) = delete;
    TCPSession & operator=(const TCPSession &) = delete;
    TCPSession(const TCPSession &&) = delete;
    TCPSession & operator=(const TCPSession &&) = delete;

    void start(const std::vector<tcp::endpoint> & endpoints);

    void stop();

private:
    void on_connect();

    void do_read_header();

    void do_read_body();

    void handle_message();

    void close_session();

    void handle_stream_error(boost::system::error_code ec);

private:
    const SessionConfig & config_;

    //
    // Concurrency handling.
    //
    asio::strand<asio::io_context::executor_type> strand_;

    //
    // Packet management.
    //
    tcp::socket socket_;

    // Header + body size
    std::vector<uint8_t> incoming_header_;
    size_t next_payload_size_{0};

    // Ring buffer to hold small messages
    std::array<uint8_t, MESSAGE_BUFFER_SIZE> body_buffer_;

    // Vector for large messages
    //
    // TODO <optimization>: a memory pool like Boost.pool would
    // be better so we can avoid allocations on large messages.
    std::vector<uint8_t> large_body_buffer_;

    // Pointer to last server packet (fixed array or vector).
    uint8_t *body_buffer_ptr_{nullptr};

    // Reference to the thread's message handler interface.
    const MessageHandler & message_handler_;

};
