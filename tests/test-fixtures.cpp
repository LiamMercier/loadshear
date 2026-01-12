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

#include "test-fixtures.h"
#include "parser.h"
#include "wasm-message-handler.h"
#include "nop-message-handler.h"

DSLData get_simple_valid_script_data()
{
    DSLData correct_data;

    {
        auto & settings = correct_data.settings;
        auto & orchestrator = correct_data.orchestrator;

        // Fill the settings.
        settings.identifier = "my_settings";
        settings.session_protocol = "TCP";

        settings.header_size = 8;
        settings.body_max = 12288;
        settings.read = true;
        settings.repeat = false;

        settings.port = 55555;
        settings.shards = 4;

        settings.handler_value = "tests/modules/tcp-single-session-heartbeat.wasm";
        settings.endpoints = {"localhost", "127.0.0.1"};

        settings.packet_identifiers["p1"] = "tests/packets/test-packet-1.bin";
        settings.packet_identifiers["p2"] = "tests/packets/test-packet-heavy.bin";

        // Fill the orchestrator
        orchestrator.settings_identifier = "my_settings";

        // We know what these actions should be from the script.

        // CREATE 100 OFFSET 0ms
        {
            Action action_create;

            action_create.type = ActionType::CREATE;
            action_create.range = {0, 100};
            action_create.count = 100;
            action_create.offset_ms = 0;

            orchestrator.actions.push_back(action_create);
        }

        // CONNECT 0:50 OFFSET 100ms
        {
            Action action_connect;

            action_connect.type = ActionType::CONNECT;
            action_connect.range = {0, 50};
            action_connect.count = 50;
            action_connect.offset_ms = 100;

            orchestrator.actions.push_back(action_connect);
        }

        // CONNECT 50:100
        {
            Action action_connect;

            action_connect.type = ActionType::CONNECT;
            action_connect.range = {50, 100};
            action_connect.count = 50;
            action_connect.offset_ms = Parser::DEFAULT_OFFSET_MS;

            orchestrator.actions.push_back(action_connect);
        }

        // SEND 0:100 p1 COPIES 5 TIMESTAMP 0:8 "little":"seconds" OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 5;

            TimestampModification time_mod;

            time_mod.timestamp_bytes = {0, 8};
            time_mod.little_endian = true;
            time_mod.format_name = "seconds";

            action_send.push_modifier(time_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p1 COPIES 5 COUNTER 0:8 "little":1 OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 5;

            CounterModification counter_mod;

            counter_mod.counter_bytes = {0, 8};
            counter_mod.little_endian = true;
            counter_mod.counter_step = 1;

            action_send.push_modifier(counter_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p1 COPIES 1
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 1;

            action_send.offset_ms = Parser::DEFAULT_OFFSET_MS;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p2 COPIES 1 COUNTER 0:8 "little":7
        //      TIMESTAMP 12:8 "big":"milliseconds" OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p2";
            action_send.count = 1;

            CounterModification counter_mod;

            counter_mod.counter_bytes = {0, 8};
            counter_mod.little_endian = true;
            counter_mod.counter_step = 7;

            action_send.push_modifier(counter_mod);

            TimestampModification time_mod;

            time_mod.timestamp_bytes = {12, 8};
            time_mod.little_endian = false;
            time_mod.format_name = "milliseconds";

            action_send.push_modifier(time_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // FLOOD 0:100 OFFSET 100ms
        {
            Action action_flood;

            action_flood.type = ActionType::FLOOD;
            action_flood.range = {0, 100};
            action_flood.offset_ms = 100;

            orchestrator.actions.push_back(action_flood);
        }

        // DRAIN 0:100 OFFSET 500ms
        {
            Action action_drain;

            action_drain.type = ActionType::DRAIN;
            action_drain.range = {0, 100};
            action_drain.offset_ms = 500;

            orchestrator.actions.push_back(action_drain);
        }

        // DISCONNECT 0:100 OFFSET 15s
        {
            Action action_disconnect;

            action_disconnect.type = ActionType::DISCONNECT;
            action_disconnect.range = {0, 100};
            action_disconnect.offset_ms = 15000;

            orchestrator.actions.push_back(action_disconnect);
        }
    }

    return correct_data;
}

ExecutionPlan<TCPSession>
get_simple_valid_script_plan(std::pmr::memory_resource* memory)
{
    SessionConfig session_config(8, 12288, true, false, 100);
    HostInfo<TCPSession> h_info;

    using tcp = asio::ip::tcp;

    asio::io_context temp_cntx;

    tcp::resolver ip_resolver(temp_cntx);

    boost::system::error_code ec;

    {
        auto res = ip_resolver.resolve("localhost", "55555", ec);

        if (ec)
        {
            ADD_FAILURE() << "Resolver for localhost:55555 had ec";
        }

        for (const auto & entry : res)
        {
            h_info.endpoints.push_back(entry.endpoint());
        }
    }

    {
        auto res = ip_resolver.resolve("127.0.0.1", "55555", ec);

        if (ec)
        {
             ADD_FAILURE() << "Resolver for 127.0.0.1:55555 had ec";
        }

        for (const auto & entry : res)
        {
            h_info.endpoints.push_back(entry.endpoint());
        }
    }

    // Comparison of functions here is irrelevant anyways.
    Shard<TCPSession>::MessageHandlerFactory factory =
        []() -> std::unique_ptr<MessageHandler>{
            return std::make_unique<NOPMessageHandler>();
        };



    ExecutionPlan<TCPSession> plan
                        (
                            OrchestratorConfig<TCPSession>
                                (session_config,
                                 h_info,
                                 factory,
                                 4),
                            std::pmr::vector<std::pmr::vector<uint8_t>>(memory)
                        );

    uint64_t offset = 0;

    // CREATE 100 OFFSET 0ms
    {
        ActionDescriptor create;
        create.make_create(0, 100, 0);
        plan.actions.push_back(std::move(create));
    }

    // CONNECT 0:50 OFFSET 100ms
    {
        offset += 100;
        ActionDescriptor connect_action;
        connect_action.make_connect(0, 50, offset);
        plan.actions.push_back(std::move(connect_action));
    }

    // CONNECT 50:100
    {
        offset += Parser::DEFAULT_OFFSET_MS;
        ActionDescriptor connect_action;
        connect_action.make_connect(50, 100, offset);
        plan.actions.push_back(std::move(connect_action));
    }

    // We will not access this anyways.
    std::vector<uint8_t> fake_vector;

    // SEND 0:100 p1 COPIES 5 TIMESTAMP 0:8 "little":"seconds" OFFSET 200ms
    {
        offset += 200;
        ActionDescriptor send_action;
        send_action.make_send(0,
                              100,
                              5,
                              offset);
        plan.actions.push_back(std::move(send_action));

        PayloadDescriptor payload_desc;
        payload_desc.packet_data = {fake_vector.data(), size_t(11)};

        {
            PacketOperation packet_op;
            packet_op.make_timestamp(8, true, TimestampFormat::Seconds);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        {
            PacketOperation packet_op;
            packet_op.make_identity(3);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        for (int i = 0; i < 5; i++)
        {
            std::vector<uint16_t> empty;
            plan.counter_steps.push_back(empty);
            plan.payloads.push_back(payload_desc);
        }
    }

    // SEND 0:100 p1 COPIES 5 COUNTER 0:8 "little":1 OFFSET 200ms
    {
        offset += 200;
        ActionDescriptor send_action;
        send_action.make_send(0,
                              100,
                              5,
                              offset);
        plan.actions.push_back(std::move(send_action));

        PayloadDescriptor payload_desc;
        payload_desc.packet_data = {fake_vector.data(), size_t(11)};

        {
            PacketOperation packet_op;
            packet_op.make_counter(8, true);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        {
            PacketOperation packet_op;
            packet_op.make_identity(3);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        for (int i = 0; i < 5; i++)
        {
            plan.counter_steps.push_back({1});
            plan.payloads.push_back(payload_desc);
        }
    }

    // SEND 0:100 p1 COPIES 1
    {
        offset += Parser::DEFAULT_OFFSET_MS;
        ActionDescriptor send_action;
        send_action.make_send(0,
                              100,
                              1,
                              offset);
        plan.actions.push_back(std::move(send_action));

        PayloadDescriptor payload_desc;
        payload_desc.packet_data = {fake_vector.data(), size_t(11)};

        {
            PacketOperation packet_op;
            packet_op.make_identity(11);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        std::vector<uint16_t> empty;
        plan.counter_steps.push_back({empty});
        plan.payloads.push_back(std::move(payload_desc));
    }

    // SEND 0:100 p2 COPIES 1 COUNTER 0:8 "little":7 TIMESTAMP 12:8
    // "big":"milliseconds" OFFSET 200ms
    {
        offset += 200;
        ActionDescriptor send_action;
        send_action.make_send(0,
                              100,
                              1,
                              offset);
        plan.actions.push_back(std::move(send_action));

        PayloadDescriptor payload_desc;
        payload_desc.packet_data = {fake_vector.data(), size_t(5500)};

        {
            PacketOperation packet_op;
            packet_op.make_counter(8, true);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        {
            PacketOperation packet_op;
            packet_op.make_identity(4);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        {
            PacketOperation packet_op;
            packet_op.make_timestamp(8, false, TimestampFormat::Milliseconds);

            payload_desc.ops.push_back(std::move(packet_op));
        }

        {
            PacketOperation packet_op;
            packet_op.make_identity(5500 - (8 + 8 + 4));

            payload_desc.ops.push_back(std::move(packet_op));
        }

        plan.counter_steps.push_back({7});
        plan.payloads.push_back(std::move(payload_desc));
    }

    // FLOOD 0:100 OFFSET 100ms
    {
        offset += 100;
        ActionDescriptor flood;
        flood.make_flood(0, 100, offset);
        plan.actions.push_back(std::move(flood));
    }

    // DRAIN 0:100 TIMEOUT 10000ms OFFSET 500ms
    {
        offset += 500;
        ActionDescriptor drain;
        drain.make_drain(0, 100, 10000, offset);
        plan.actions.push_back(std::move(drain));
    }

    // DISCONNECT 0:100 OFFSET 15s
    {
        offset += 15 * 1000;
        ActionDescriptor disc;
        disc.make_disconnect(0, 100, offset);
        plan.actions.push_back(std::move(disc));
    }

    return plan;
}
