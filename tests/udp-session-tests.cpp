#include <gtest/gtest.h>

#include "wasm-message-handler.h"
#include "udp-session.h"

#include "udp-broadcast-server.h"
#include "test-helpers.h"

TEST(UDPSessionTests, SingleSessionParsing)
{
    // Setup basic server.
    asio::io_context server_cntx;
    asio::ip::udp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    uint64_t server_interval_ms = 5;
    uint64_t total_packets = 10;

    std::vector<uint8_t> packet{ 0x1, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0 };

    UDPBroadcastServer server(server_cntx,
                              server_ep,
                              server_interval_ms,
                              packet,
                              total_packets);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false, 100);

    // Create mock header parsing function and WASM instance.
    wasmtime::Config WASM_config;
    auto engine = std::make_shared<wasmtime::Engine>(std::move(WASM_config));

    std::vector<uint8_t> wasm_bytes;

    try {
        wasm_bytes = read_binary_file("tests/modules/tcp-single-session-parsing.wasm");
    }
    catch (const std::exception & error)
    {
        std::cerr << error.what() << "\n";
        FAIL();
    }

    auto module_tmp = wasmtime::Module::compile(*engine, wasm_bytes);

    if (!module_tmp)
    {
        session_cntx.stop();

        server_cntx.stop();

        if (server_thread.joinable())
        {
            server_thread.join();
        }

        FAIL();
    }

    auto module = std::make_shared<wasmtime::Module>(module_tmp.unwrap());

    try {
        WASMMessageHandler test_handler_construct(engine, module);
    }
    catch (const std::exception & error)
    {
        std::cerr << error.what() << "\n";
        FAIL();
    }

    // Setup payload manager with one payload, no operations.
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    std::vector<PayloadDescriptor> payloads;

    PacketOperation identity_op;
    identity_op.make_identity(packet_size);

    payloads.push_back({{packet_1.data(), packet_1.size()},
                       std::vector<PacketOperation>{identity_op} });

    std::vector<std::vector<uint16_t>> steps(payloads.size(), {10});
    PayloadManager payload_manager(payloads, steps);

    // WASMMessageHandler handler(engine, module);
    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<UDPSession> session_ptr;

    // Empty callback that just stops the context.
    UDPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

    ShardMetrics metrics;

    asio::post(session_cntx, [&](){
        try {
            handler_ptr = std::make_shared<WASMMessageHandler>(engine, module);
        }
        catch (const std::exception & error)
        {
            std::cerr << error.what() << "\n";
            session_cntx.stop();
            return;
        }

        std::array<bool, 4> bytes_to_read{0,0,0,1};
        handler_ptr->set_header_parser([bytes_to_read](std::span<const uint8_t> buffer) -> HeaderResult
        {
            size_t size = 0;

            for (size_t i = 0; i < bytes_to_read.size(); i++)
            {
                if (bytes_to_read[i])
                {
                    size <<= 8;
                    size |= buffer[i];
                }
            }

            return {size, HeaderResult::Status::OK};
        });

        // Create the UDPSession instance.
        session_ptr = make_shared<UDPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              metrics,
                                              cb);

        // Connection endpoint.
        const UDPSession::Endpoints endpoints{
                UDPSession::udp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

        session_ptr->send(1);

    });

    // Turn this test off after 100ms of responding.
    asio::steady_timer stop_timer(session_cntx, std::chrono::milliseconds(100));
    stop_timer.async_wait([&](const boost::system::error_code &)
    {
        // This should stop the context.
        session_ptr->stop();
    });

    session_cntx.run();

    server_cntx.stop();

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    // Asserts after we finish testing.
    EXPECT_EQ(server.lifetime_connections_, 1) << "Server only accepted "
                                               << server.lifetime_connections_
                                               << " of "
                                               << 1
                                               << "requests!";

    // The server sends 8 bytes at a time, we read 4 as the header and 4 as the payload
    // of which we send an equal amount of bytes 0x55 back.
    EXPECT_EQ(server.lifetime_sent_,
              server.lifetime_received_
              - packet_size) << "Server was not responded to properly!";

    SUCCEED();
}
