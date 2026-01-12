// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include <gtest/gtest.h>

#include <thread>
#include <chrono>
#include <condition_variable>

#include "wasm-message-handler.h"
#include "tcp-session.h"
#include "session-pool.h"

#include "tcp-broadcast-server.h"

namespace asio = boost::asio;

// Disable tests for now while changing WASMMessageHandler api
/*
TEST(SessionPoolTests, TCPPool)
{
    size_t N_Sessions = 100;
    uint64_t server_interval_ms = 50;

    // Startup basic TCP server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    TCPBroadcastServer server(server_cntx, server_ep, server_interval_ms);

    std::thread server_thread([&]
    {
        server.start();
        server_cntx.run();
    });

    SessionConfig config(4, 12288, true, false);

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

    SessionPool<TCPSession> pool(cntx, config, [](){});

    pool.create_sessions(N_Sessions, cntx, config, handler, payload_manager);

    const TCPSession::Endpoints endpoints{
            TCPSession::tcp::endpoint(
                asio::ip::make_address("127.0.0.1"),
                12345
            )};

    pool.start_all_sessions(endpoints);

    asio::steady_timer stop_timer(cntx, std::chrono::milliseconds(500));
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
    // Mock Controller
    SessionConfig config(4, 12288, true, false);

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

    uint64_t server_interval_ms = 50;

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

        SessionPool<TCPSession> pool(cntx, config, [](){});

        pool.create_sessions(1, cntx, config, handler, payload_manager);

        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        pool.start_all_sessions(endpoints);

        asio::steady_timer stop_timer(cntx, std::chrono::milliseconds(200));
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

        SessionPool<TCPSession> pool(cntx, config, [](){});

        pool.create_sessions(1, cntx, config, handler, payload_manager);

        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        pool.start_all_sessions(endpoints);

        asio::steady_timer stop_timer(cntx, std::chrono::milliseconds(200));
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

// TEST(SessionPoolTests, MultiplePoolStartup)
// {
//     uint64_t server_interval_ms = 500;
//
//     // Startup basic TCP server.
//     asio::io_context server_cntx;
//     asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);
//
//     TCPBroadcastServer server(server_cntx, server_ep, server_interval_ms);
//
//     std::thread server_thread([&]
//     {
//         server.start();
//         server_cntx.run();
//     });
//
//     // One thread pool shared across all pools
//     auto thread_count = std::thread::hardware_concurrency();
//
//     if (thread_count == 0)
//     {
//         thread_count = 2;
//     }
//
//     asio::io_context cntx;
//     auto work_guard = asio::make_work_guard(cntx);
//
//     std::vector<std::thread> controller_threads;
//     controller_threads.reserve(thread_count);
//
//     // Create custom header parsing function.
//     WASMMessageHandler handler;
//     std::array<bool, 4> bytes_to_read{0,0,0,1};
//
//     handler.set_header_parser([bytes_to_read](std::span<const uint8_t> buffer) -> HeaderResult
//     {
//         size_t size = 0;
//
//         for (size_t i = 0; i < bytes_to_read.size(); i++)
//         {
//             if (bytes_to_read[i])
//             {
//                 size <<= 8;
//                 size |= buffer[i];
//             }
//         }
//
//         return {size, HeaderResult::Status::OK};
//     });
//
//     // Empty payload manager.
//     PayloadManager payload_manager;
//
//     // Make it so we can simulate the controller deciding to delete the pool.
//     //
//     // Otherwise in our test it will go out of scope too soon.
//     SessionPool<TCPSession> *pool1_ptr = nullptr;
//
//     auto on_closed_1 = [&cntx, &pool1_ptr](){
//         cntx.post([&pool1_ptr]{
//             EXPECT_EQ(pool1_ptr->active_sessions(), 0) << "Pool still has "
//                                                        << pool1_ptr->active_sessions()
//                                                        << " active!";
//             delete pool1_ptr;
//             pool1_ptr = nullptr;
//         });
//     };
//
//     asio::steady_timer stop_pool1_timer(cntx, std::chrono::seconds(1));
//
//     SessionConfig config(4, 12288, true, false);
//
//     // First SessionPool using TCPSession objects.
//     {
//         size_t N_sessions = 500;
//
//         pool1_ptr = new SessionPool<TCPSession>(cntx, config, on_closed_1);
//
//         pool1_ptr->create_sessions(N_sessions, cntx, config, handler, payload_manager);
//
//         const TCPSession::Endpoints endpoints{
//                 TCPSession::tcp::endpoint(
//                     asio::ip::make_address("127.0.0.1"),
//                     12345
//                 )};
//
//         pool1_ptr->start_all_sessions(endpoints);
//
//         stop_pool1_timer.async_wait([&](const boost::system::error_code &)
//         {
//             if (!pool1_ptr)
//             {
//                 return;
//             }
//
//             pool1_ptr->stop_all_sessions();
//         });
//     }
//
//     SessionPool<TCPSession> *pool2_ptr = nullptr;
//
//     auto on_closed_2 = [&cntx, &pool2_ptr](){
//         cntx.post([&pool2_ptr]{
//             EXPECT_EQ(pool2_ptr->active_sessions(), 0) << "Pool still has "
//                                                        << pool2_ptr->active_sessions()
//                                                        << " active!";
//             delete pool2_ptr;
//             pool2_ptr = nullptr;
//         });
//     };
//
//     asio::steady_timer stop_pool2_timer(cntx, std::chrono::seconds(1));
//
//     // Second SessionPool using TCPSession objects.
//     {
//         size_t N_sessions = 300;
//
//         pool2_ptr = new SessionPool<TCPSession>(cntx, config, on_closed_2);
//
//         pool2_ptr->create_sessions(N_sessions, cntx, config, handler, payload_manager);
//
//         const TCPSession::Endpoints endpoints{
//                 TCPSession::tcp::endpoint(
//                     asio::ip::make_address("127.0.0.1"),
//                     12345
//                 )};
//
//         pool2_ptr->start_all_sessions(endpoints);
//
//         stop_pool2_timer.async_wait([&](const boost::system::error_code &)
//         {
//             if (!pool2_ptr)
//             {
//                 return;
//             }
//
//             pool2_ptr->stop_all_sessions();
//         });
//     }
//
//     // Start timer on threads to eventually exit.
//     asio::steady_timer stop_timer(cntx, std::chrono::seconds(5));
//     stop_timer.async_wait([&](const boost::system::error_code &)
//     {
//         cntx.stop();
//     });
//
//     // Start the pool.
//     for (unsigned int i = 0; i < thread_count; i++)
//     {
//         controller_threads.emplace_back([&]{
//             cntx.run();
//         });
//     }
//
//     // Join threads.
//     for (auto & thread : controller_threads)
//     {
//         if (thread.joinable())
//         {
//             thread.join();
//         }
//     }
//
//     server_cntx.stop();
//
//     if (server_thread.joinable())
//     {
//         server_thread.join();
//     }
//
//     std::cout << "Server received "
//             << server.lifetime_received_
//             << " bytes\n";
// }
*/
