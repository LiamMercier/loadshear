#include <gtest/gtest.h>

#include "test-helpers.h"
#include "tcp-sink-server.h"

#include "shard.h"
#include "payload-manager.h"
#include "tcp-session.h"

TEST(TCPShardTests, SingleShardTest)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Startup basic server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    TCPSinkServer server(server_cntx,
                         server_ep,
                         packet_size);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    // Mock orchestrator, make one shard for testing.
    int BASE_NUM_PAYLOADS = 8;
    SessionConfig config(4, 12288, true, false);
    HostInfo<TCPSession> host_info;
    host_info.endpoints = {server_ep};

    //
    // Create the payload manager.
    //
    std::vector<PayloadDescriptor> payloads;

    // Create 8 payloads from the base "hello world" packet for different lengths.
    for (int i = 0; i < BASE_NUM_PAYLOADS; i++)
    {
        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - i);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    //
    // Create the message handler.
    //

    // TODO: figure out factory logic.


    // Turn this test off.
    asio::steady_timer stop_timer(server_cntx, std::chrono::milliseconds(530));
    stop_timer.async_wait([&](const boost::system::error_code &)
    {
        // This should stop the context.
        server_cntx.stop();
    });

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    FAIL();
}
