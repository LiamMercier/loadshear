#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <unordered_set>

class TCPBroadcastServer
{
public:
    using tcp = asio::ip::tcp;

    TCPBroadcastServer(asio::io_context & cntx,
                       tcp::endpoint endpoint,
                       uint64_t broadcast_interval_ms,
                       std::vector<uint8_t> bytes,
                       uint64_t num_packets)
    :cntx_(cntx),
    acceptor_(cntx, endpoint),
    timer_(cntx),
    broadcast_interval_ms_(broadcast_interval_ms),
    total_heartbeats_to_send_(num_packets),
    send_bytes_(bytes)
    {
    }

    void start()
    {
        start_accept();
        start_broadcast_timer();
    }

private:

    // Just hold client data.
    struct BasicSession : std::enable_shared_from_this<BasicSession>
    {
        tcp::socket socket;
        std::vector<uint8_t> write_buffer;
        std::vector<uint8_t> read_buffer;

        explicit BasicSession(asio::io_context & cntx)
        :socket(cntx),
        read_buffer(1024*4)
        {

        }
    };

    void start_accept()
    {
        auto session = std::make_shared<BasicSession>(cntx_);

        acceptor_.async_accept(session->socket,
            [this, session](boost::system::error_code ec){
            if (!ec)
            {
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    clients_.insert(session);
                }
                lifetime_connections_ += 1;

                start_read(session);
            }

            start_accept();
        });
    }

    // Loop reads that just record the total data received.
    void start_read(std::shared_ptr<BasicSession> session)
    {
        session->socket.async_read_some(
            asio::buffer(session->read_buffer),
                [this, session](boost::system::error_code ec, size_t s){
                   if (ec)
                   {
                       std::lock_guard<std::mutex> lock(clients_mutex_);
                       clients_.erase(session);
                       return;
                    }

                    lifetime_received_.fetch_add(s, std::memory_order_relaxed);

                    start_read(session);
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
        std::lock_guard<std::mutex> lock(clients_mutex_);

        lifetime_heartbeats_ += 1;

        for (auto it = clients_.begin(); it != clients_.end(); it++)
        {
            auto session = *it;

            session->write_buffer.assign(send_bytes_.begin(), send_bytes_.end());

            lifetime_broadcasts_.fetch_add(1);

            asio::async_write(
                session->socket,
                asio::buffer(session->write_buffer),
                [this, session](boost::system::error_code ec, std::size_t){
                    if (ec)
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex_);
                        clients_.erase(session);
                        return;
                    }

                    lifetime_sent_.fetch_add(session->write_buffer.size(), std::memory_order_relaxed);
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
    tcp::acceptor acceptor_;
    asio::steady_timer timer_;

    uint64_t broadcast_interval_ms_;
    uint64_t total_heartbeats_to_send_;

    std::mutex clients_mutex_;
    std::unordered_set<std::shared_ptr<BasicSession>> clients_;

    // Buffer to send.
    std::vector<uint8_t> send_bytes_;
};
