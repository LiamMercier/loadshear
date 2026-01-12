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

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include <unordered_set>

#include <iostream>

namespace asio = boost::asio;

class UDPBroadcastServer
{
public:
    using udp = asio::ip::udp;

    UDPBroadcastServer(asio::io_context & cntx,
                       udp::endpoint endpoint,
                       uint64_t broadcast_interval_ms,
                       std::vector<uint8_t> bytes,
                       uint64_t num_packets)
    :cntx_(cntx),
    socket_(cntx, endpoint),
    timer_(cntx),
    broadcast_interval_ms_(broadcast_interval_ms),
    total_heartbeats_to_send_(num_packets),
    send_bytes_(bytes)
    {
        read_buffer_.resize(1024 * 4);
    }

    void start()
    {
        start_receive();
        start_broadcast_timer();
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

    // Allow clients to register themselves.
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

    void start_broadcast_timer()
    {
        if (lifetime_heartbeats_ >= total_heartbeats_to_send_)
        {
            return;
        }

        timer_.expires_after(std::chrono::milliseconds(broadcast_interval_ms_));

        timer_.async_wait([this](boost::system::error_code ec){
            if (!ec)
            {
                broadcast_heartbeat();
                start_broadcast_timer();
            }
        });
    }

    void broadcast_heartbeat()
    {
        std::vector<udp::endpoint> clients_snapshot_;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_snapshot_.reserve(clients_.size());
            for (const auto & ep : clients_)
            {
                clients_snapshot_.push_back(ep);
            }
        }

        lifetime_heartbeats_ += 1;

        for (const auto & ep : clients_snapshot_)
        {
            auto buffer_ptr = std::make_shared<std::vector<uint8_t>>(send_bytes_);

            lifetime_broadcasts_.fetch_add(1);

            socket_.async_send_to(
                asio::buffer(*buffer_ptr),
                ep,
                [this, buffer_ptr, ep](boost::system::error_code ec,
                                       std::size_t count){
                    if (ec)
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        clients_.erase(ep);
                        return;
                    }

                    lifetime_sent_.fetch_add(count,
                                             std::memory_order_relaxed);
            });
        }
    }

public:
    size_t lifetime_connections_{0};
    std::atomic<size_t> lifetime_broadcasts_{0};
    std::atomic<size_t> lifetime_sent_{0};
    std::atomic<size_t> lifetime_received_{0};
    std::atomic<size_t> lifetime_heartbeats_{0};

private:
    asio::io_context & cntx_;
    udp::socket socket_;
    asio::steady_timer timer_;

    udp::endpoint remote_endpoint_;
    std::vector<uint8_t> read_buffer_;

    uint64_t broadcast_interval_ms_;
    uint64_t total_heartbeats_to_send_;

    std::mutex clients_mutex_;
    std::unordered_set<udp::endpoint, EndpointHash, EndpointEq> clients_;

    // Buffer to send.
    std::vector<uint8_t> send_bytes_;
};
