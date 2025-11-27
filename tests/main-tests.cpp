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
    uint64_t server_interval_ms = 500;

    // Startup basic TCP server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    TCPBroadcastServer server(server_cntx, server_ep, server_interval_ms);

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

    PayloadManager payload_manager;

    SessionPool<TCPSession> pool(cntx, config);

    pool.create_sessions(N_Sessions, cntx, config, handler, payload_manager);

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

    pool.stop_all_sessions();

    cntx.run();

    // Asserts after we finish testing.
    EXPECT_EQ(server.lifetime_connections_, N_Sessions) << "Server only accepted "
                                                        << server.lifetime_connections_
                                                        << " of "
                                                        << N_Sessions
                                                        << "requests!";

    EXPECT_EQ(pool.active_sessions(), 0) << "Pool still has "
                                         << pool.active_sessions()
                                         << " active!";

    server_cntx.stop();
    if (server_thread.joinable())
    {
        server_thread.join();
    }

    SUCCEED();
}

// Test what happens when we create and destroy pools repeatedly.
TEST(SessionPoolTests, TCPPoolDestruction)
{
    // Mock Controller, since we don't have one implemented right now.
    SessionConfig config(4, 12288, true);

    // Create custom header parsing function.
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

    // Cycle this 50 times.
    int NUM_CYCLES = 3;

    uint64_t server_interval_ms = 500;

    // Startup basic TCP server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    TCPBroadcastServer server(server_cntx, server_ep, server_interval_ms);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    bool pool_ne = false;

    for (int i = 0; i < NUM_CYCLES; i++)
    {
        asio::io_context cntx;

        SessionPool<TCPSession> pool(cntx, config);

        pool.create_sessions(1, cntx, config, handler, payload_manager);

        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        pool.start_all_sessions(endpoints);

        asio::steady_timer stop_timer(cntx, std::chrono::seconds(1));
        stop_timer.async_wait([&](const boost::system::error_code &)
        {
            cntx.stop();
        });

        pool.stop_all_sessions();

        cntx.run();

        if (pool.active_sessions() != 0)
        {
            pool_ne = true;
        }
    }

    EXPECT_EQ(pool_ne, false) << "Pool had active sessions in one or more cycles (test 1)!";

    pool_ne = false;

    for (int i = 0; i < NUM_CYCLES; i++)
    {
        asio::io_context cntx;

        SessionPool<TCPSession> pool(cntx, config);

        pool.create_sessions(1, cntx, config, handler, payload_manager);

        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        pool.start_all_sessions(endpoints);

        asio::steady_timer stop_timer(cntx, std::chrono::seconds(1));
        stop_timer.async_wait([&](const boost::system::error_code &)
        {
            cntx.stop();
        });

        pool.stop_all_sessions();

        cntx.run();

        if (pool.active_sessions() != 0)
        {
            pool_ne = true;
        }
    }

    EXPECT_EQ(pool_ne, false) << "Pool had active sessions in one or more cycles (test 2)!";

    server_cntx.stop();
    if (server_thread.joinable())
    {
        server_thread.join();
    }
}
