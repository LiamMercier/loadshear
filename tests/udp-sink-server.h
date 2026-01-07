#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include <unordered_set>

#include <iostream>

namespace asio = boost::asio;

class UDPSinkServer
{
public:
    using udp = asio::ip::udp;

    UDPSinkServer(asio::io_context & cntx,
                  udp::endpoint endpoint)
    :cntx_(cntx),
    socket_(cntx, endpoint)
    {
        read_buffer_.resize(1024 * 4);
    }

    void start()
    {
        start_receive();
    }

private:

    struct EndpointHash
    {
        // Untested hash function.
        size_t operator()(const udp::endpoint & ep) const
        {
            std::string hash_str = ep.address().to_string()
                                   + std::to_string(ep.port());

            return std::hash<std::string>()(hash_str);
        }
    };

    struct EndpointEq
    {
        bool operator()(const udp::endpoint & lhs, const udp::endpoint & rhs) const
        {
            return (lhs.port() == rhs.port())
                    && (lhs.address() == rhs.address());
        }
    };

    // Allow clients to register themselves, and count bytes read.
    void start_receive()
    {
        socket_.async_receive_from(
            asio::buffer(read_buffer_),
            remote_endpoint_,
            [this](boost::system::error_code ec, size_t count)
            {
                // Keep registering clients.
                start_receive();

                if (!ec && count > 0)
                {
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        if (clients_.count(remote_endpoint_) == 0)
                        {
                            lifetime_connections_ += 1;
                            clients_.insert(remote_endpoint_);
                        }
                    }

                    lifetime_received_ += count;
                }
            });
    }

public:
    size_t lifetime_connections_{0};
    std::atomic<size_t> lifetime_received_{0};

private:
    asio::io_context & cntx_;
    udp::socket socket_;

    udp::endpoint remote_endpoint_;
    std::vector<uint8_t> read_buffer_;

    std::mutex clients_mutex_;
    std::unordered_set<udp::endpoint, EndpointHash, EndpointEq> clients_;
};
