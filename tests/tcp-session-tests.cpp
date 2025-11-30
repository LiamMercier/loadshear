#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <memory>
#include <filesystem>
#include <fstream>

#include "wasm-message-handler.h"
#include "tcp-session.h"

#include "tcp-broadcast-server.h"

static std::vector<uint8_t> read_wasm_file(const std::string & path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("File " + path + " does not exist\n");
    }

    std::error_code ec;
    size_t filesize = static_cast<size_t>(std::filesystem::file_size(path));

    if (ec)
    {
        throw std::runtime_error("Call file_size for " + path + " failed\n");
    }

    std::vector<uint8_t> buffer(filesize);

    if (!file.read(reinterpret_cast<char *>(buffer.data()), filesize))
    {
        throw std::runtime_error("File " + path + " read failed\n");
    }

    return buffer;
}

TEST(TCPSessionTests, SingleSessionParsing)
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

    // Create mock header parsing function and WASM instance.
    wasmtime::Config WASM_config;
    auto engine = std::make_shared<wasmtime::Engine>(std::move(WASM_config));

    std::vector<uint8_t> wasm_bytes;

    try {
        wasm_bytes = read_wasm_file("tests/modules/tcp-single-session-parsing.wasm");
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
    PayloadManager payload_manager;

    // WASMMessageHandler handler(engine, module);
    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    // Empty callback that just prints and stops the context.
    TCPSession::DisconnectCallback cb = [&](){
        GTEST_LOG_(INFO) << "Session finished!";
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

    // Turn this test off after 500ms of responding.
    asio::steady_timer stop_timer(session_cntx, std::chrono::milliseconds(530));
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
    // of which we should send back 4 bytes.
    EXPECT_EQ(server.lifetime_sent_ / 2,
              server.lifetime_received_) << "Server was not responded to properly!";

    SUCCEED();
}


