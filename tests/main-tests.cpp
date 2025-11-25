#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <condition_variable>

#include "wasm-message-handler.h"
#include "tcp-session.h"
#include "session-pool.h"

#include "tcp-broadcast-server.h"

namespace asio = boost::asio;

TEST(SessionPoolTests, TCPPool)
{
    size_t N_Sessions = 100;

    // Startup basic TCP server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    TCPBroadcastServer server(server_cntx, server_ep);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    SessionConfig config(4, 12288, true);

    std::array<bool, 4> bytes_to_read{0,0,0,1};

    WASMMessageHandler handler;
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

    // Test out the header parsing.
    const uint8_t packet[8] = { 0x1, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0 };
    std::span<const uint8_t> buffer(packet, 8);

    HeaderResult result = handler.parse_header(buffer);

    EXPECT_EQ(result.length, 4);

    //
    // Start testing out the server
    //

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context cntx;

    SessionPool<TCPSession> pool(cntx, config);

    pool.create_sessions(N_Sessions, cntx, config, handler);

    const TCPSession::Endpoints endpoints{
            TCPSession::tcp::endpoint(
                asio::ip::make_address("127.0.0.1"),
                12345
            )};

    pool.start_all_sessions(endpoints);

    asio::steady_timer stop_timer(cntx, std::chrono::seconds(5));
    stop_timer.async_wait([&](const boost::system::error_code &)
    {
        cntx.stop();
    });

    cntx.run();

    // Asserts after we finish testing.
    EXPECT_EQ(N_Sessions, server.lifetime_connections_) << "Server only accepted "
                                                        << server.lifetime_connections_
                                                        << " of "
                                                        << N_Sessions
                                                        << "requests!";

    server_cntx.stop();
    if (server_thread.joinable())
    {
        server_thread.join();
    }

    SUCCEED();
}
