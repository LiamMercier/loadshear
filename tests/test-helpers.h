#pragma once

#include <vector>
#include <filesystem>
#include <fstream>

#include <boost/asio.hpp>

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
