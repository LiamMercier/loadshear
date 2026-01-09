#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include <deque>

#include "session-config.h"
#include "message-handler-interface.h"
#include "payload-manager.h"
#include "response-packet.h"
#include "shard-metrics.h"

namespace asio = boost::asio;

// Performance-aware UDP session class to be stored in a SessionPool.
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
class UDPSession : public std::enable_shared_from_this<UDPSession>
{
public:
    using udp = asio::ip::udp;
    using Endpoint = udp::endpoint;

    // Endpoint lists are not meaningful in UDP.
    using Endpoints = Endpoint;

    using DisconnectCallback = std::function<void()>;

    // Everything read by asio's receive will be less than this
    // anyways due to headers.
    static constexpr size_t MAX_DATAGRAM_SIZE = 65535 - 8;

    // Typical max network fragment size, without ipv4 and udp header.
    static constexpr size_t SUGGESTED_PAYLOAD_SIZE = 1500 - 20 - 8;

public:
    UDPSession(asio::io_context & cntx,
               const SessionConfig & config,
               const MessageHandler & message_handler,
               const PayloadManager & payload_manager,
               ShardMetrics & shard_metrics,
               DisconnectCallback & on_disconnect);

    // We do not want to move this class.
    UDPSession(const UDPSession &) = delete;
    UDPSession & operator=(const UDPSession &) = delete;
    UDPSession(UDPSession &&) = delete;
    UDPSession & operator=(UDPSession &&) = delete;

    void start(const Endpoints & endpoints);

    void flood();

    void send(size_t N);

    // Try to get all payloads out.
    void drain();

    void stop();

private:
    void on_connect();

    void do_read();

    void handle_message();

    void try_start_write();

    void do_write();

    void close_session();

public:

private:
    const SessionConfig & config_;

    //
    // Concurrency handling.
    //
    asio::strand<asio::io_context::executor_type> strand_;
    bool live_{false};
    bool flood_{false};

    // For graceful exits.
    bool draining_{false};

    // Ensures we don't have two writers for non-flooding scenario
    bool writing_{false};

    //
    // Packet management.
    //
    udp::socket socket_;

    // We hold the maximum expected packet size in this buffer.
    std::vector<uint8_t> packet_buffer_;
    size_t packet_size_{0};

    std::deque<ResponsePacket> responses_;

    // Increasing index into the payloads that need to be sent by this session
    size_t next_payload_index_{0};

    //
    // TODO <optimization>: we would prefer to store these in a pool.
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
    uint32_t write_sample_counter_{0};
    uint32_t read_sample_counter_{0};
    std::chrono::steady_clock::time_point write_start_time_;
    std::chrono::steady_clock::time_point read_start_time_;

    //
    const DisconnectCallback on_disconnect_;
};
