#include <gtest/gtest.h>

#include "test-helpers.h"
#include "tcp-sink-server.h"

#include "shard.h"
#include "payload-manager.h"
#include "wasm-message-handler.h"
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
    auto payload_manager = std::make_shared<PayloadManager>(payloads, steps);

    //
    // Create the message handler.
    //

    // Our orchestrator will hold a shared pointer to a WASM engine and copy of the module.
    //
    // This can be passed to each Shard.
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

    Shard<TCPSession>::MessageHandlerFactory factory =
        [engine, module]() -> std::unique_ptr<MessageHandler>
        {
            return std::make_unique<WASMMessageHandler>(engine, module);
        };

    // Create one shard to do work.
    Shard s1(payload_manager,
             factory,
             config,
             host_info,
             [&](){ server_cntx.stop(); });

    // Start the shard.
    s1.start();

    std::vector<ActionDescriptor> actions;

    uint32_t NUM_SESSIONS = 50;

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
        std::chrono::milliseconds(0)
    });

    // Enable flood on each session.
    actions.push_back({
        ActionType::FLOOD,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(0)
    });

    actions.push_back({
        ActionType::DRAIN,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(0)
    });

    actions.push_back({
        ActionType::DISCONNECT,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(0)
    });

    // Mimic a 50ms timer loop, orchestrator will have a real asio timer.
    for (size_t action_index = 0; action_index < actions.size(); action_index++)
    {
        const auto & action = actions[action_index];
        s1.submit_work(action);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Mimic orchestrator stop at end of command loop.
    s1.stop();

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    // Join the shard, this would be done in the Orchestrator after our shard calls back and
    // says it can be joined. We could just wait on a condition variable in the Orchestrator
    // and we will not end up eating resources because our thread will be marked as blocked.
    s1.join();

    EXPECT_EQ(server.lifetime_received_,
              packet_size * payloads.size() * NUM_SESSIONS) << "Server only got "
                                                            << server.lifetime_received_
                                                            << " of "
                                                            << packet_size
                                                               * payloads.size()
                                                               * NUM_SESSIONS
                                                            << " bytes!";
}

TEST(TCPShardTests, MultiShardTest)
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

    // Mock orchestrator, make NUM_SHARDS shards for testing.
    int BASE_NUM_PAYLOADS = 8;
    int NUM_SHARDS = 4;

    SessionConfig config(4, 12288, true, false);
    HostInfo<TCPSession> host_info;
    host_info.endpoints = {server_ep};

    //
    // Create the payload manager.
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
    auto payload_manager = std::make_shared<PayloadManager>(payloads, steps);

    //
    // Create the message handler.
    //

    // Our orchestrator will hold a shared pointer to a WASM engine and copy of the module.
    //
    // This can be passed to each Shard.
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

    Shard<TCPSession>::MessageHandlerFactory factory =
        [engine, module]() -> std::unique_ptr<MessageHandler>
        {
            return std::make_unique<WASMMessageHandler>(engine, module);
        };

    // Create NUM_SHARDS shards to do work.
    std::vector<std::unique_ptr<Shard<TCPSession>>> shards;

    for (int i = 0; i < NUM_SHARDS; i++)
    {
        shards.emplace_back(std::make_unique<Shard<TCPSession>>
                                (payload_manager,
                                 factory,
                                 config,
                                 host_info,
                                 [&](){ server_cntx.stop(); }));
    }

    // Start the shards.
    for (auto & shard : shards)
    {
        shard->start();
    }

    std::vector<ActionDescriptor> actions;

    uint32_t NUM_SESSIONS = 50;

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
        std::chrono::milliseconds(0)
    });

    // Enable flood on each session.
    actions.push_back({
        ActionType::FLOOD,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(0)
    });

    actions.push_back({
        ActionType::DRAIN,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(0)
    });

    actions.push_back({
        ActionType::DISCONNECT,
        0,
        NUM_SESSIONS,
        10*1000,
        std::chrono::milliseconds(0)
    });

    // Mimic a 50ms timer loop, orchestrator will have a real asio timer.
    for (size_t action_index = 0; action_index < actions.size(); action_index++)
    {
        const auto & action = actions[action_index];

        for (auto & shard : shards)
        {
             shard->submit_work(action);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Mimic orchestrator stop at end of command loop.
    for (auto & shard : shards)
    {
        shard->stop();
    }

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    // Join the shards, this would be done in the Orchestrator after our shards call back and
    // say they can be joined. We could just wait on a condition variable in the Orchestrator
    // and we will not end up eating resources because our thread will be marked as blocked.
    for (auto & shard : shards)
    {
        shard->join();
    }

    EXPECT_EQ(server.lifetime_received_,
              packet_size
              * payloads.size()
              * NUM_SESSIONS
              * NUM_SHARDS) << "Server only got "
                            << server.lifetime_received_
                            << " of "
                            << packet_size
                                * payloads.size()
                                * NUM_SESSIONS
                                * NUM_SHARDS
                            << " bytes!";
}

TEST(TCPShardTests, MultiShardHeavy)
{
    if (std::getenv("RUN_HEAVY_GTEST") == nullptr)
    {
        GTEST_SKIP() << "RUN_HEAVY_GTEST was disabled. Set RUN_HEAVY_GTEST=1 to run.";
    }

    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-heavy.bin");
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

    // Mock orchestrator, make NUM_SHARDS shards for testing.
    int BASE_NUM_PAYLOADS = 8;
    int NUM_SHARDS = 4;

    SessionConfig config(4, 12288, true, false);
    HostInfo<TCPSession> host_info;
    host_info.endpoints = {server_ep};

    //
    // Create the payload manager.
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
    auto payload_manager = std::make_shared<PayloadManager>(payloads, steps);

    //
    // Create the message handler.
    //

    // Our orchestrator will hold a shared pointer to a WASM engine and copy of the module.
    //
    // This can be passed to each Shard.
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

    Shard<TCPSession>::MessageHandlerFactory factory =
        [engine, module]() -> std::unique_ptr<MessageHandler>
        {
            return std::make_unique<WASMMessageHandler>(engine, module);
        };

    // Create NUM_SHARDS shards to do work.
    std::vector<std::unique_ptr<Shard<TCPSession>>> shards;

    for (int i = 0; i < NUM_SHARDS; i++)
    {
        shards.emplace_back(std::make_unique<Shard<TCPSession>>
                                (payload_manager,
                                 factory,
                                 config,
                                 host_info,
                                 [&](){ server_cntx.stop(); }));
    }

    // Start the shards.
    for (auto & shard : shards)
    {
        shard->start();
    }

    std::vector<ActionDescriptor> actions;

    uint32_t NUM_SESSIONS = 500;

    // Create NUM_SESSION session's. Only thing that matters is count.
    actions.push_back({
        ActionType::CREATE,
        0,
        NUM_SESSIONS,
        NUM_SESSIONS,
        std::chrono::milliseconds(0)
    });

    // Connect each session.
    for (int i = 0; i < 10; i++)
    {
        actions.push_back({
            ActionType::CONNECT,
            i * NUM_SESSIONS / 10,
            (i+1) * NUM_SESSIONS / 10,
            0,
            std::chrono::milliseconds(0)
        });
    }

    // Enable flood on each session.
    actions.push_back({
        ActionType::FLOOD,
        0,
        NUM_SESSIONS,
        0,
        std::chrono::milliseconds(0)
    });

    actions.push_back({
        ActionType::DRAIN,
        0,
        NUM_SESSIONS,
        10*3000,
        std::chrono::milliseconds(0)
    });

    // Mimic a 50ms timer loop, orchestrator will have a real asio timer.
    for (size_t action_index = 0; action_index < actions.size(); action_index++)
    {
        const auto & action = actions[action_index];

        // We need to actually give some delay to DRAIN for this test, since we will
        // have many Session objects that connect but never start writing (and thus close).
        //
        // This is because we are spinning up a bunch of TCP sockets and writes all at once,
        // so we are going to cause the io_context to be flooded, thus we see callbacks
        // very late, thus we see DRAIN before on_connect for the starved session's and
        // thus we do not get the expected number of bytes written.
        //
        // So, give them around 300ms to start writing and then drain.
        //
        // If you were using this tool to flood, you would not care about a deterministic
        // number of bytes being written anyways, so it is a non-issue in my opinion.
        if (action.type == ActionType::DRAIN)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }

        for (auto & shard : shards)
        {
             shard->submit_work(action);
        }
    }

    // Mimic orchestrator stop at end of command loop.
    for (auto & shard : shards)
    {
        shard->stop();
    }

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    // Join the shards, this would be done in the Orchestrator after our shards call back and
    // say they can be joined. We could just wait on a condition variable in the Orchestrator
    // and we will not end up eating resources because our thread will be marked as blocked.
    for (auto & shard : shards)
    {
        shard->join();
    }

    EXPECT_EQ(server.lifetime_connections_,
              NUM_SESSIONS * NUM_SHARDS) << "Server did not accept all connections. You may be hitting OS limits!";

    EXPECT_EQ(server.lifetime_received_,
              packet_size
              * payloads.size()
              * NUM_SESSIONS
              * NUM_SHARDS) << "Server only got "
                            << server.lifetime_received_
                            << " of "
                            << packet_size
                                * payloads.size()
                                * NUM_SESSIONS
                                * NUM_SHARDS
                            << " bytes!";
}
