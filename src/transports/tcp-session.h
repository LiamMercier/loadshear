#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <deque>

#include "session-config.h"
#include "message-handler-interface.h"
#include "payload-manager.h"
#include "response-packet.h"
#include "shard-metrics.h"

namespace asio = boost::asio;

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

// Performance-aware TCP session class to be stored in a SessionPool.
//
// Assumptions:
//
// - SessionPool MUST NOT expect writes to occur after calling stop()
// - SessionPool MUST NOT destroy itself or any references passed until every session has closed
//      - Sessions are considered closed as soon as they call on_disconnect_
//      - SessionPool MAY decide to delay destruction after all sessions are closed
// - Payloads that are shared across session instances are read only
// - Server packets are handled by an interface passed to the session.
//
class TCPSession : public std::enable_shared_from_this<TCPSession>
{
public:
    using tcp = asio::ip::tcp;
    using Endpoint = tcp::endpoint;
    using Endpoints = std::vector<tcp::endpoint>;

    using DisconnectCallback = std::function<void()>;

public:
    TCPSession(asio::io_context & cntx,
               const SessionConfig & config,
               const MessageHandler & message_handler,
               const PayloadManager & payload_manager,
               ShardMetrics & shard_metrics,
               DisconnectCallback & on_disconnect);

    // This class should not be moved or copied.
    TCPSession(const TCPSession &) = delete;
    TCPSession & operator=(const TCPSession &) = delete;
    TCPSession(TCPSession &&) = delete;
    TCPSession & operator=(TCPSession &&) = delete;

    void start(const Endpoints & endpoints);

    void flood();

    void send(size_t N);

    // Graceful exit.
    void drain();

    void stop();

private:
    void on_connect();

    void do_read_header();

    void do_read_body();

    void handle_message();

    void try_start_write();

    void do_write();

    void close_session();

    void handle_stream_error(boost::system::error_code ec);

public:

private:
    const SessionConfig & config_;

    //
    // Concurrency handling.
    //
    asio::strand<asio::io_context::executor_type> strand_;
    bool live_{false};
    bool connecting_{false};
    bool flood_{false};

    // For graceful exits.
    bool draining_{false};

    // Ensures we don't have two writers for non-flooding scenario
    bool writing_{false};

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

    std::deque<ResponsePacket> responses_;

    // Increasing index into the payloads that need to be sent by this session
    size_t next_payload_index_{0};

    PreparedPayload current_payload_;

    // Keep track of how many payload writes are requested if not flooding.
    size_t writes_queued_{0};

    //
    // Handlers & metrics
    //

    // Reference to the thread's message handler interface.
    const MessageHandler & message_handler_;

    // Reference to the Controller's payload manager.
    const PayloadManager & payload_manager_;

    // Write metrics, keep track of connection times.
    ShardMetrics & metrics_sink_;

    //
    const DisconnectCallback on_disconnect_;
};
