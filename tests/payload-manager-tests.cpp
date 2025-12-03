#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "payload-manager.h"

static std::vector<uint8_t> slices_to_vector(const std::vector<boost::asio::const_buffer> & slices)
{
    std::vector<uint8_t> out;

    size_t total = 0;

    for (auto const & buffer : slices)
    {
        total += buffer.size();
    }

    out.reserve(total);

    for (const auto & buffer : slices)
    {
        const uint8_t *ptr = static_cast<const uint8_t*>(buffer.data());
        out.insert(out.end(), ptr, ptr + buffer.size());
    }

    return out;
}

static std::string hexdump(const std::vector<uint8_t> & vec)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < vec.size(); i++)
    {
        oss << std::setw(2) << static_cast<int>(vec[i]);
        if (i + 1 < vec.size())
        {
            oss << ' ';
        }
    }

    return oss.str();
}

static void EXPECT_VECTOR_EQ(const std::vector<uint8_t> & expected,
                             const std::vector<uint8_t> & actual)
{
    if (expected == actual)
    {
        return;
    }

    // Control failure message for bytes.
    ADD_FAILURE() << "Vectors differ! Expected: "
                  << hexdump(expected)
                  << "\n Actual: "
                  << hexdump(actual);
}

static std::vector<uint8_t> read_binary_file(const std::string & path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("File " + path + " does not exist\n");
    }

    std::error_code ec;
    size_t filesize = static_cast<size_t>(std::filesystem::file_size(path, ec));

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

TEST(PayloadManagerTests, CheckPayloadBytes)
{
    std::cout << "Starting payload checks \n\n\n";

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

    std::vector<uint16_t> steps(payloads.size(), 5);
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
