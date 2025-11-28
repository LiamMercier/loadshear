#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <memory>

#include "wasm-message-handler.h"
#include "tcp-session.h"

#include "tcp-broadcast-server.h"

TEST(TCPSessionTests, MessageHandling)
{
    // Setup basic server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    uint64_t server_interval_ms = 50;

    TCPBroadcastServer server(server_cntx, server_ep, server_interval_ms);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

    // Create mock header parsing function.
    WASMMessageHandler handler;

    std::array<bool, 4> bytes_to_read{0,0,0,1};
    handler.set_header_parser([bytes_to_read](std::span<const uint8_t> buffer) -> HeaderResult
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

    // Empty payload manager.
    PayloadManager payload_manager;

    // Empty callback that just prints and stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        GTEST_LOG_(INFO) << "Session finished!";
        session_cntx.stop();
    };

    // Create the TCPSession instance.
    auto session = make_shared<TCPSession>(session_cntx,
                                           config,
                                           handler,
                                           payload_manager,
                                           cb);

    // Connection endpoint.
    const TCPSession::Endpoints endpoints{
            TCPSession::tcp::endpoint(
                asio::ip::make_address("127.0.0.1"),
                12345
            )};

    // Call to connect.
    session->start(endpoints);

    // Turn this test off after 500ms of responding.
    asio::steady_timer stop_timer(session_cntx, std::chrono::milliseconds(530));
    stop_timer.async_wait([&](const boost::system::error_code &)
    {
        // This should stop the context.
        session->stop();
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
    // of which we should send back 4 bytes.
    EXPECT_EQ(server.lifetime_sent_ / 2,
              server.lifetime_received_) << "Server was not responded to properly!";

    SUCCEED();
}
