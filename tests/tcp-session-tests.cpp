#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <memory>

#include "wasm-message-handler.h"
#include "tcp-session.h"

#include "tcp-broadcast-server.h"
#include "tcp-sink-server.h"
#include "test-helpers.h"

TEST(TCPSessionTests, SingleSessionParsing)
{
    // Setup basic server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    uint64_t server_interval_ms = 5;
    uint64_t total_packets = 10;

    std::vector<uint8_t> packet{ 0x1, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0 };

    TCPBroadcastServer server(server_cntx,
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

    SessionConfig config(4, 12288, true, false);

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

    // Empty payload manager.
    std::vector<PayloadDescriptor> payloads;
    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    // WASMMessageHandler handler(engine, module);
    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // Create the TCPSession instance.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

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
              server.lifetime_received_) << "Server was not responded to properly!";

    SUCCEED();
}

//
// Example fixed size header
//

enum class HeaderType : uint8_t
{
    Login,
    Register,
    Ping,
    PingResponse,
    SendDM
};

struct Header
{
    HeaderType type_;
    uint32_t payload_len;

    static constexpr size_t HEADER_SIZE = sizeof(payload_len) + sizeof(type_);
};

TEST(TCPSessionTests, SingleSessionHeartbeat)
{
    // Setup basic server.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    uint64_t server_interval_ms = 5;

    // Send ping packets with body length 2, little endian.
    //
    // We would expect a ping response of 0x3, 0x0, 0x0, 0x0, 0x0 for example.
    std::vector<uint8_t> packet{ 0x2,
                                 0x2, 0x0, 0x0, 0x0,
                                 0xa, 0xb };

    uint64_t total_packets = 10;

    TCPBroadcastServer server(server_cntx,
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

    SessionConfig config(Header::HEADER_SIZE, 12288, true, false);

    // Create mock header parsing function and WASM instance.
    wasmtime::Config WASM_config;
    auto engine = std::make_shared<wasmtime::Engine>(std::move(WASM_config));

    std::vector<uint8_t> wasm_bytes;

    try {
        wasm_bytes = read_binary_file("tests/modules/tcp-single-session-heartbeat.wasm");
    }
    catch (const std::exception & error)
    {
        std::cerr << error.what() << "\n";

        server_cntx.stop();

        if (server_thread.joinable())
        {
            server_thread.join();
        }

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

    // Empty payload manager.
    std::vector<PayloadDescriptor> payloads;
    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    // WASMMessageHandler handler(engine, module);
    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // Create the TCPSession instance.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

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
    EXPECT_EQ(server.lifetime_sent_ * 5,
              server.lifetime_received_ * 7) << "Server was not responded to properly!";

    SUCCEED();
}

TEST(TCPSessionTests, SingleSessionWriteOne)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup basic server.
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

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

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
    std::vector<PayloadDescriptor> payloads;

    PacketOperation identity_op;
    identity_op.make_identity(packet_size);

    payloads.push_back({{packet_1.data(), packet_1.size()},
                       std::vector<PacketOperation>{identity_op} });

    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // We could also consider reading them as <index 1> <index 2> ... instead of bools.
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

        // Create the TCPSession instance.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

        // Send our single payload.
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

    // Check equal to size of binary file, we sent this once.
    EXPECT_EQ(server.lifetime_received_, packet_size) << "Server only got "
                                                      << server.lifetime_received_
                                                      << " of "
                                                      << packet_size
                                                      << " bytes!";

    SUCCEED();
}

TEST(TCPSessionTests, SingleSessionCounterFlood)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup basic server.
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

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

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

    // Setup payload manager
    std::vector<PayloadDescriptor> payloads;

    // Create 9 payloads with decreasing sizes
    for (int i = 0; i < 9; i++)
    {
        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - i);

        PacketOperation counter_op;
        counter_op.make_counter(i, (i % 2));

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes, counter_op} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // We could also consider reading them as <index 1> <index 2> ... instead of bools.
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

        // Create the TCPSession instance.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

        // Send out all payloads as fast as possible.
        session_ptr->flood();
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

    // Check equal to size of binary file, we sent this once.
    EXPECT_EQ(server.lifetime_received_,
              packet_size * payloads.size()) << "Server only got "
                                             << server.lifetime_received_
                                             << " of "
                                             << packet_size * payloads.size()
                                             << " bytes!";

    SUCCEED();
}

TEST(TCPSessionTests, SingleSessionTimestampFlood)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup basic server.
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

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

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

    // Setup payload manager.
    std::vector<PayloadDescriptor> payloads;

    // Create 8 payloads with decreasing sizes
    for (int i = 0; i < 8; i++)
    {
        // Alternative between 4 byte and 8 byte timestamp
        bool endian = (i % 2);
        uint32_t len = endian ? 4 : 8;
        TimestampFormat this_format = endian ? TimestampFormat::Seconds
                                             : TimestampFormat::Milliseconds;

        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - len);

        PacketOperation timestamp_op;
        timestamp_op.make_timestamp(len, endian, this_format);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes, timestamp_op} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // We could also consider reading them as <index 1> <index 2> ... instead of bools.
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

        // Create the TCPSession instance.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);

        // Send out all payloads as fast as possible.
        session_ptr->flood();
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

    // Check equal to size of binary file, we sent this once.
    EXPECT_EQ(server.lifetime_received_,
              packet_size * payloads.size()) << "Server only got "
                                             << server.lifetime_received_
                                             << " of "
                                             << packet_size * payloads.size()
                                             << " bytes!";

    SUCCEED();
}

TEST(TCPSessionTests, MultiSessionCounterFlood)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup basic server.
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

    // Setup a TCPSession by itself.
    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

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

    // Setup payload manager
    std::vector<PayloadDescriptor> payloads;

    // Create 9 payloads with decreasing sizes
    for (int i = 0; i < 9; i++)
    {
        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - i);

        PacketOperation counter_op;
        counter_op.make_counter(i, (i % 2));

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes, counter_op} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;
    std::shared_ptr<TCPSession> session_ptr_2;

    // Empty callback that just stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        session_cntx.stop();
    };

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

        // We could also consider reading them as <index 1> <index 2> ... instead of bools.
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

        // Create the TCPSession instances.
        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              cb);

        session_ptr_2 = make_shared<TCPSession>(session_cntx,
                                                config,
                                                *handler_ptr,
                                                payload_manager,
                                                cb);

        // Connection endpoint.
        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        // Call to connect.
        session_ptr->start(endpoints);
        session_ptr_2->start(endpoints);

        // Send out all payloads as fast as possible.
        session_ptr->flood();
        session_ptr_2->flood();
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
    EXPECT_EQ(server.lifetime_connections_, 2) << "Server only accepted "
                                               << server.lifetime_connections_
                                               << " of "
                                               << 2
                                               << "requests!";

    // Check equal to size of binary file, we sent this once.
    EXPECT_EQ(server.lifetime_received_,
              packet_size * payloads.size() * 2) << "Server only got "
                                                 << server.lifetime_received_
                                                 << " of "
                                                 << packet_size * payloads.size() * 2
                                                 << " bytes!";

    SUCCEED();
}
