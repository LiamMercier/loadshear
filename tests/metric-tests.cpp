#include <gtest/gtest.h>

#include "test-helpers.h"

#include "shard-metrics.h"
#include "tcp-broadcast-server.h"
#include "wasm-message-handler.h"
#include "tcp-session.h"

TEST(ShardMetrics, RecordLatencies)
{
    ShardMetrics metrics;

    // Record random latencies.

    // Two records in bucket 0
    metrics.record_connection_latency(0);
    metrics.record_connection_latency(63);

    // Six records in bucket 1
    metrics.record_connection_latency(64);
    metrics.record_connection_latency(65);
    metrics.record_connection_latency(72);
    metrics.record_connection_latency(84);
    metrics.record_connection_latency(99);
    metrics.record_connection_latency(127);

    // Ten records in bucket 2
    metrics.record_connection_latency(128);
    metrics.record_connection_latency(129);
    metrics.record_connection_latency(170);
    metrics.record_connection_latency(186);
    metrics.record_connection_latency(199);
    metrics.record_connection_latency(210);
    metrics.record_connection_latency(211);
    metrics.record_connection_latency(212);
    metrics.record_connection_latency(250);
    metrics.record_connection_latency(255);

    // Six records in bucket 3
    metrics.record_connection_latency(256);
    metrics.record_connection_latency(280);
    metrics.record_connection_latency(333);
    metrics.record_connection_latency(444);
    metrics.record_connection_latency(510);
    metrics.record_connection_latency(511);

    // Five records in bucket 4
    metrics.record_connection_latency(512);
    metrics.record_connection_latency(555);
    metrics.record_connection_latency(666);
    metrics.record_connection_latency(1000);
    metrics.record_connection_latency(1023);

    // Four records in bucket 5
    metrics.record_connection_latency(1024);
    metrics.record_connection_latency(1500);
    metrics.record_connection_latency(2020);
    metrics.record_connection_latency(2047);

    // Four records in bucket 6
    metrics.record_connection_latency(2048);
    metrics.record_connection_latency(3000);
    metrics.record_connection_latency(4000);
    metrics.record_connection_latency(4095);

    // Four records in bucket 7
    metrics.record_connection_latency(4096);
    metrics.record_connection_latency(6000);
    metrics.record_connection_latency(7777);
    metrics.record_connection_latency(8191);

    // Four records in bucket 8
    metrics.record_connection_latency(8192);
    metrics.record_connection_latency(10000);
    metrics.record_connection_latency(12000);
    metrics.record_connection_latency(16383);

    // Seven records in bucket 9
    metrics.record_connection_latency(16384);
    metrics.record_connection_latency(18000);
    metrics.record_connection_latency(20000);
    metrics.record_connection_latency(21777);
    metrics.record_connection_latency(22777);
    metrics.record_connection_latency(30000);
    metrics.record_connection_latency(32767);

    // Three records in bucket 10
    metrics.record_connection_latency(32768);
    metrics.record_connection_latency(35353);
    metrics.record_connection_latency(65535);

    // Three records in bucket 11
    metrics.record_connection_latency(65536);
    metrics.record_connection_latency(100000);
    metrics.record_connection_latency(131071);

    // Four records in bucket 12
    metrics.record_connection_latency(131072);
    metrics.record_connection_latency(160000);
    metrics.record_connection_latency(200000);
    metrics.record_connection_latency(262143);

    // Five records in bucket 13
    metrics.record_connection_latency(262144);
    metrics.record_connection_latency(300000);
    metrics.record_connection_latency(373737);
    metrics.record_connection_latency(400000);
    metrics.record_connection_latency(524287);

    // Three records in bucket 14
    metrics.record_connection_latency(524288);
    metrics.record_connection_latency(600000);
    metrics.record_connection_latency(1048575);

    // Eight records in bucket 15
    metrics.record_connection_latency(1048576);
    metrics.record_connection_latency(10000000);
    metrics.record_connection_latency(20000000);
    metrics.record_connection_latency(526236372);
    metrics.record_connection_latency(63734747);
    metrics.record_connection_latency(222222222);
    metrics.record_connection_latency(777777777);
    metrics.record_connection_latency(848484454);

    std::array<std::atomic<uint64_t>, 16> bucket_values = {2, 6, 10, 6, 5, 4, 4, 4,
                                                           4, 7, 3, 3, 4, 5, 3, 8};

    auto snapshot = metrics.fetch_snapshot();

    // Ensure the counts are equal.
    for (size_t i = 0; i < snapshot.connection_latency_buckets.size(); i++)
    {
        EXPECT_EQ(snapshot.connection_latency_buckets[i],
                  bucket_values[i]) << "Bucket values for bucket "
                                    << i
                                    << " not equal! Expected: "
                                    << bucket_values[i]
                                    << " Actual: "
                                    << snapshot.connection_latency_buckets[i];
    }
}

TEST(ShardMetrics, RecordBytesTransmitted)
{
    // Create a simple server that sends packets.
    asio::io_context server_cntx;
    asio::ip::tcp::endpoint server_ep(asio::ip::make_address("127.0.0.1"), 12345);

    uint64_t server_interval_ms = 1;
    uint64_t total_packets = 50;

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

    asio::io_context session_cntx;

    SessionConfig config(4, 12288, true, false);

    wasmtime::Config WASM_config;
    auto engine = std::make_shared<wasmtime::Engine>(std::move(WASM_config));

    std::vector<uint8_t> wasm_bytes;

    // re-use this module.
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

    // Add some payloads.
    std::vector<PayloadDescriptor> payloads;
    std::vector<uint16_t> steps(payloads.size(), 1);
    PayloadManager payload_manager(payloads, steps);

    std::shared_ptr<WASMMessageHandler> handler_ptr;
    std::shared_ptr<TCPSession> session_ptr;

    TCPSession::DisconnectCallback cb = [&](){
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

        session_ptr = make_shared<TCPSession>(session_cntx,
                                              config,
                                              *handler_ptr,
                                              payload_manager,
                                              metrics,
                                              cb);

        const TCPSession::Endpoints endpoints{
                TCPSession::tcp::endpoint(
                    asio::ip::make_address("127.0.0.1"),
                    12345
                )};

        session_ptr->start(endpoints);

    });

    asio::steady_timer stop_timer(session_cntx, std::chrono::milliseconds(100));
    stop_timer.async_wait([&](const boost::system::error_code &)
    {
        session_ptr->stop();
    });

    session_cntx.run();

    server_cntx.stop();

    if (server_thread.joinable())
    {
        server_thread.join();
    }

    auto snapshot = metrics.fetch_snapshot();

    // Check the shard metrics to see that we wrote and read payloads.
    EXPECT_EQ(server.lifetime_sent_,
              snapshot.bytes_read) << "Snapshot reads not equal to server writes! "
                                   << "Expected: "
                                   << server.lifetime_sent_
                                   << " Actual: "
                                   << snapshot.bytes_read;

    EXPECT_EQ(server.lifetime_received_,
              snapshot.bytes_sent) << "Snapshot writes not equal to server reads! "
                                   << "Expected: "
                                   << server.lifetime_received_
                                   << " Actual: "
                                   << snapshot.bytes_sent;

    EXPECT_EQ(server.lifetime_connections_,
              snapshot.finished_connections) << "Snapshot finished connections "
                                             << "not equal to server connection count! "
                                             << "Expected: "
                                             << server.lifetime_connections_
                                             << " Actual: "
                                             << snapshot.finished_connections;

    SUCCEED();
}
