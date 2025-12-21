#include <gtest/gtest.h>

#include "test-helpers.h"
#include "tcp-sink-server.h"

#include "wasm-message-handler.h"
#include "tcp-session.h"
#include "orchestrator.h"

TEST(TCPOrchestratorTests, LightMultishardWASM)
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

    int BASE_NUM_PAYLOADS = 8;

    HostInfo<TCPSession> host_info;
    host_info.endpoints = {server_ep};

    // For this test we are using WASM, create the necessary components.                                                  

    //
    // Create the message handler factory.
    //

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
        server_cntx.stop();

        if (server_thread.joinable())
        {
            server_thread.join();
        }

        FAIL();
    }

    auto module = std::make_shared<wasmtime::Module>(module_tmp.unwrap());

    Orchestrator<TCPSession>::MessageHandlerFactory handler_factory = 
        [engine, module]() -> std::unique_ptr<MessageHandler>
            {
                return std::make_unique<WASMMessageHandler>(engine, module);
            };

    // Configure the orchestrator, set to use 2 shards.
    OrchestratorConfig<TCPSession> orchestrator_config({4, 12288, true, false},
                                                       host_info,
                                                       handler_factory,
                                                       2);

    //
    // Create the payloads.
    //
    std::vector<PayloadDescriptor> payloads;

    for (int i = 0; i < BASE_NUM_PAYLOADS; i++)
    {
        // Alternative between little endian and big endian
        bool little_endian = (i % 2);
        uint32_t len = i;

        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - len);

        PacketOperation counter_op;
        counter_op.make_counter(len, little_endian);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes, counter_op} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);

    // Create the list of actions
    std::vector<ActionDescriptor> actions;

    uint32_t NUM_SESSIONS = 170;

    // Create NUM_SESSION session's. Only thing that matters is count.
    actions.push_back({
        ActionType::CREATE,
        0,
        NUM_SESSIONS,
        NUM_SESSIONS,
        std::chrono::milliseconds(0)
    });

    // Connect each session.
    actions.push_back({
        ActionType::CONNECT,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(100)
    });

    // Enable flood on each session.
    actions.push_back({
        ActionType::FLOOD,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(150)
    });

    actions.push_back({
        ActionType::DRAIN,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(200)
    });

    actions.push_back({
        ActionType::DISCONNECT,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(500)
    });

    // Create the orchestrator.

    Orchestrator<TCPSession> orchestrator(actions,
                                          payloads,
                                          steps,
                                          orchestrator_config);

    // Start the orchestrator.
    orchestrator.start();

    // After the orchestrator stops, we stop the server and exit.
    server_cntx.stop();

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    EXPECT_EQ(server.lifetime_received_,
              packet_size
              * payloads.size()
              * NUM_SESSIONS) << "Server only got "
                              << server.lifetime_received_
                              << " of "
                              << packet_size
                                  * payloads.size()
                                  * NUM_SESSIONS
                              << " bytes!";
}

TEST(TCPOrchestratorTests, HeavyMultishardWASM)
{
    if (std::getenv("RUN_HEAVY_GTEST") == nullptr)
    {
        GTEST_SKIP() << "RUN_HEAVY_GTEST was disabled. Set RUN_HEAVY_GTEST=1 to run.";
    }

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

    int BASE_NUM_PAYLOADS = 8;

    HostInfo<TCPSession> host_info;
    host_info.endpoints = {server_ep};

    // For this test we are using WASM, create the necessary components.

    //
    // Create the message handler factory.
    //

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
        server_cntx.stop();

        if (server_thread.joinable())
        {
            server_thread.join();
        }

        FAIL();
    }

    auto module = std::make_shared<wasmtime::Module>(module_tmp.unwrap());

    Orchestrator<TCPSession>::MessageHandlerFactory handler_factory =
        [engine, module]() -> std::unique_ptr<MessageHandler>
            {
                return std::make_unique<WASMMessageHandler>(engine, module);
            };

    // Configure the orchestrator, set to use 2 shards.
    OrchestratorConfig<TCPSession> orchestrator_config({4, 12288, true, false},
                                                       host_info,
                                                       handler_factory,
                                                       4);

    //
    // Create the payloads.
    //
    std::vector<PayloadDescriptor> payloads;

    for (int i = 0; i < BASE_NUM_PAYLOADS; i++)
    {
        // Alternative between little endian and big endian
        bool little_endian = (i % 2);
        uint32_t len = i;

        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - len);

        PacketOperation counter_op;
        counter_op.make_counter(len, little_endian);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes, counter_op} });
    }

    std::vector<uint16_t> steps(payloads.size(), 1);

    // Create the list of actions
    std::vector<ActionDescriptor> actions;

    uint32_t NUM_SESSIONS = 4321;

    // Create NUM_SESSION session's. Only thing that matters is count.
    actions.push_back({
        ActionType::CREATE,
        0,
        NUM_SESSIONS,
        NUM_SESSIONS,
        std::chrono::milliseconds(0)
    });

    // Connect each session.
    actions.push_back({
        ActionType::CONNECT,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(100)
    });

    // Enable flood on each session.
    actions.push_back({
        ActionType::SEND,
        0,
        NUM_SESSIONS,
        static_cast<uint32_t>(BASE_NUM_PAYLOADS),
        std::chrono::milliseconds(200)
    });

    actions.push_back({
        ActionType::DRAIN,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(300)
    });

    actions.push_back({
        ActionType::DISCONNECT,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(1000)
    });

    // Create the orchestrator.

    Orchestrator<TCPSession> orchestrator(actions,
                                          payloads,
                                          steps,
                                          orchestrator_config);

    // Start the orchestrator.
    orchestrator.start();

    // After the orchestrator stops, we stop the server and exit.
    server_cntx.stop();

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    EXPECT_EQ(server.lifetime_connections_,
              NUM_SESSIONS) << "Server did not accept all connections. "
                            << "You may be hitting OS limits, or the server "
                            << "may not have enough connector backlog for this burst, "
                            << "or your CPU may not be processing connection callbacks before "
                            << "the drain call. You can try setting ulimit to"
                            << "something large, i.e ulimit -n 16000";

    EXPECT_EQ(server.lifetime_received_,
              packet_size
              * payloads.size()
              * NUM_SESSIONS) << "Server only got "
                              << server.lifetime_received_
                              << " of "
                              << packet_size
                                  * payloads.size()
                                  * NUM_SESSIONS
                              << " bytes!";
}
