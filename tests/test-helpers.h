#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <boost/asio.hpp>

#include "script-structs.h"

namespace asio = boost::asio;

inline std::vector<uint8_t> read_binary_file(const std::string & path)
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

inline std::vector<uint8_t> slices_to_vector(const std::vector<boost::asio::const_buffer> & slices)
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

inline std::string hexdump(const std::vector<uint8_t> & vec)
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

inline void EXPECT_VECTOR_EQ(const std::vector<uint8_t> & expected,
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

inline std::string dump_DSL_data(const DSLData & data)
{
    std::ostringstream data_str;

    std::string endpoints_list;
    std::string packet_ids;

    endpoints_list += "[";

    for (const auto & ep : data.settings.endpoints)
    {
        endpoints_list += ep;
        endpoints_list += " ";
    }

    endpoints_list += "]";

    packet_ids += "[";

    for (const auto & mapping : data.settings.packet_identifiers)
    {
        packet_ids += "{ ";
        packet_ids += mapping.first;
        packet_ids += "->";
        packet_ids += mapping.second;
        packet_ids += " }";
    }

    packet_ids += "]";

    // TODO: add orchestrator block printing.

    data_str << "{"
             << "id: " << data.settings.identifier << " "
             << "SESSION: " << data.settings.session_protocol << " "
             << "HEADERSIZE: " << data.settings.header_size << " "
             << "BODYMAX: " << data.settings.body_max << " "
             << "READ: " << data.settings.read << " "
             << "REPEAT: " << data.settings.repeat << " "
             << "SHARDS: " << data.settings.shards << " "
             << "HANDLER: " << data.settings.handler_value << " "
             << "ENDPOINTS: " << endpoints_list << " "
             << "PACKETS: " << packet_ids
             << "}";

    return data_str.str();
}

inline void EXPECT_DSL_EQ(const DSLData & expected,
                          const DSLData & actual)
{
    bool fail = false;

    if (expected.settings.identifier
        != actual.settings.identifier)
    {
        fail = true;
    }
    else if (expected.settings.session_protocol
             != actual.settings.session_protocol)
    {
        fail = true;
    }
    else if (expected.settings.header_size
             != actual.settings.header_size)
    {
        fail = true;
    }
    else if (expected.settings.body_max
             != actual.settings.body_max)
    {
        fail = true;
    }
    else if (expected.settings.read
             != actual.settings.read)
    {
        fail = true;
    }
    else if (expected.settings.repeat
             != actual.settings.repeat)
    {
        fail = true;
    }
    else if (expected.settings.shards
             != actual.settings.shards)
    {
        fail = true;
    }
    else if (expected.settings.handler_value
             != actual.settings.handler_value)
    {
        fail = true;
    }
    else if (expected.settings.packet_identifiers != actual.settings.packet_identifiers)
    {
        fail = true;
    }

    // Terrible runtime, but at this scale it's essentially meaningless.
    for (const auto & inner_ep : expected.settings.endpoints)
    {
        bool found = false;

        for (const auto & outer_ep : actual.settings.endpoints)
        {
            if (outer_ep == inner_ep)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            fail = true;
            break;
        }
    }

    // TODO: compare the orchestrator fields.

    if (fail)
    {
        // Control failure message for bytes.
        ADD_FAILURE() << "DSL data differs!\n\nExpected: "
                      << dump_DSL_data(expected)
                      << "\n\nActual: "
                      << dump_DSL_data(actual);
    }
}
