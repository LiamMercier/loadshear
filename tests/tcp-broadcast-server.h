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
                       uint64_t broadcast_interval_ms)
    :cntx_(cntx),
    acceptor_(cntx, endpoint),
    timer_(cntx),
    broadcast_interval_ms_(broadcast_interval_ms)
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

        explicit BasicSession(asio::io_context & cntx)
        :socket(cntx)
        {

        }
    };

    void start_accept()
    {
        auto session = std::make_shared<BasicSession>(cntx_);

        acceptor_.async_accept(session->socket,
            [this, session](std::error_code ec){
            if (!ec)
            {
                clients_.insert(session);
                lifetime_connections_ += 1;
            }

            start_accept();
        });
    }

    void start_broadcast_timer()
    {
        timer_.expires_after(std::chrono::milliseconds(broadcast_interval_ms_));

        timer_.async_wait([this](std::error_code ec){
            if (!ec)
            {
                broadcast_heartbeat();
                start_broadcast_timer();
            }
        });
    }

    void broadcast_heartbeat()
    {
        static const uint8_t packet[8] = { 0x1, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0 };

        for (auto it = clients_.begin(); it != clients_.end(); it++)
        {
            auto session = *it;

            session->write_buffer.assign(packet, packet + 8);

            asio::async_write(
                session->socket,
                asio::buffer(session->write_buffer),
                [this, session](std::error_code ec, std::size_t){
                    if (ec)
                    {
                        clients_.erase(session);
                        return;
                    }

                    lifetime_sends_ += 1;
            });
        }

    }
public:
    size_t lifetime_connections_{0};
    size_t lifetime_sends_{0};
    size_t lifetime_received_{0};

private:
    asio::io_context & cntx_;
    tcp::acceptor acceptor_;
    asio::steady_timer timer_;

    uint64_t broadcast_interval_ms_;

    std::unordered_set<std::shared_ptr<BasicSession>> clients_;
};
