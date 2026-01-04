#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "payload-manager.h"
#include "test-helpers.h"

TEST(PayloadManagerTests, CheckPayloadBytes)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup payload manager.
    std::vector<PayloadDescriptor> payloads;

    int BASE_NUM_PAYLOADS = 8;

    // Create 8 payloads from the base "hello world" packet for different lengths.
    for (int i = 0; i < BASE_NUM_PAYLOADS; i++)
    {
        PacketOperation identity_op_missing_bytes;
        identity_op_missing_bytes.make_identity(packet_size - i);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{identity_op_missing_bytes} });
    }

    // Create 8 payloads from the base "hello world" packet for counters.
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

    std::vector<std::vector<uint16_t>> steps(payloads.size(), {5});
    PayloadManager payload_manager(payloads, steps);

    int NUM_PAYLOADS = BASE_NUM_PAYLOADS * 2;

    // Test the end result for each payload.
    std::vector<std::vector<uint8_t>> expected_first_pass = {
        // Expected bytes for unedited inserts.
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w'},
        {'H', 'e', 'l', 'l', 'o', ' '},
        {'H', 'e', 'l', 'l', 'o'},
        {'H', 'e', 'l', 'l'},

        // Expected bytes for inserts with counters (started at zero).
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 0x00},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 0x00, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 0x00, 0x00, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', ' ', 0x00, 0x00, 0x00, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    };

    for (int i = 0; i < NUM_PAYLOADS; i++)
    {
        PreparedPayload prepared;
        payload_manager.fill_payload(i, prepared);

        auto bytestring = slices_to_vector(prepared.packet_slices);
        EXPECT_VECTOR_EQ(expected_first_pass[i], bytestring);
    }

    // Now test that counters are being properly handled.
    std::vector<std::vector<uint8_t>> expected_second_pass = {
        // Expected bytes for unedited inserts.
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w'},
        {'H', 'e', 'l', 'l', 'o', ' '},
        {'H', 'e', 'l', 'l', 'o'},
        {'H', 'e', 'l', 'l'},

        // Expected bytes for inserts with counters (started at zero).
        //
        // Alternates between big endian (even) and little endian (odd) insertions.
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 0x05},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 0x00, 0x05},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 0x05, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', ' ', 'w', 0x00, 0x00, 0x00, 0x05},
        {'H', 'e', 'l', 'l', 'o', ' ', 0x05, 0x00, 0x00, 0x00, 0x00},
        {'H', 'e', 'l', 'l', 'o', 0x00, 0x00, 0x00, 0x00, 0x00, 0x05},
        {'H', 'e', 'l', 'l', 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    };

    for (int i = 0; i < NUM_PAYLOADS; i++)
    {
        PreparedPayload prepared;
        payload_manager.fill_payload(i, prepared);

        auto bytestring = slices_to_vector(prepared.packet_slices);
        EXPECT_VECTOR_EQ(expected_second_pass[i], bytestring);
    }

}

TEST(PayloadManagerTests, MultiCounterPayloads)
{
    std::vector<uint8_t> packet_1 = read_binary_file("tests/packets/test-packet-1.bin");
    size_t packet_size = packet_1.size();

    // Setup payload manager.
    std::vector<PayloadDescriptor> payloads;

    // Create payloads from the base "hello world" packet for counters.
    {
        PacketOperation identity_first;
        identity_first.make_identity(1);

        PacketOperation counter_op;
        counter_op.make_counter(4, false);

        PacketOperation identity_second;
        identity_second.make_identity(2);

        PacketOperation counter_op_two;
        counter_op_two.make_counter(4, true);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{
                               identity_first,
                               counter_op,
                               identity_second,
                               counter_op_two}});
    }

    {
        PacketOperation identity_first;
        identity_first.make_identity(1);

        PacketOperation counter_op;
        counter_op.make_counter(2, true);

        PacketOperation identity_second;
        identity_second.make_identity(2);

        PacketOperation counter_op_two;
        counter_op_two.make_counter(2, true);

        PacketOperation identity_third;
        identity_third.make_identity(1);

        PacketOperation counter_op_three;
        counter_op_three.make_counter(3, true);

        payloads.push_back({{packet_1.data(), packet_1.size()},
                           std::vector<PacketOperation>{
                               identity_first,
                               counter_op,
                               identity_second,
                               counter_op_two,
                               identity_third,
                               counter_op_three}});
    }

    std::vector<std::vector<uint16_t>> steps;

    steps.push_back({5, 5});
    steps.push_back({5, 5, 5});

    PayloadManager payload_manager(payloads, steps);

    int NUM_PAYLOADS = 2;

    // Test the end result for each payload.
    std::vector<std::vector<uint8_t>> expected_first_pass = {
        // Expected bytes for inserts with counters (started at zero).
        {'H', 0x00, 0x00, 0x00, 0x00, ' ', 'w', 0x00, 0x00, 0x00, 0x00},
        {'H', 0x00, 0x00, 'l', 'o', 0x00, 0x00, 'o', 0x00, 0x00, 0x00}
    };

    for (int i = 0; i < NUM_PAYLOADS; i++)
    {
        PreparedPayload prepared;
        payload_manager.fill_payload(i, prepared);

        auto bytestring = slices_to_vector(prepared.packet_slices);
        EXPECT_VECTOR_EQ(expected_first_pass[i], bytestring);
    }

    // Now test that counters are being properly handled.
    std::vector<std::vector<uint8_t>> expected_second_pass = {
        // Expected bytes for inserts with counters (started at zero).
        {'H', 0x00, 0x00, 0x00, 0x05, ' ', 'w', 0x05, 0x00, 0x00, 0x00},
        {'H', 0x05, 0x00, 'l', 'o', 0x05, 0x00, 'o', 0x05, 0x00, 0x00}
    };

    for (int i = 0; i < NUM_PAYLOADS; i++)
    {
        PreparedPayload prepared;
        payload_manager.fill_payload(i, prepared);

        auto bytestring = slices_to_vector(prepared.packet_slices);
        EXPECT_VECTOR_EQ(expected_second_pass[i], bytestring);
    }

}
