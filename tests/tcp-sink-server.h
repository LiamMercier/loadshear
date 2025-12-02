#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <unordered_set>

class TCPSinkServer
{
public:
    using tcp = asio::ip::tcp;

    TCPSinkServer(asio::io_context & cntx,
                  tcp::endpoint endpoint,
                  size_t expected_read)
    :cntx_(cntx),
    acceptor_(cntx, endpoint),
    expected_read_(expected_read)
    {
    }

    void start()
    {
        start_accept();
    }

private:

    // Just hold client data.
    struct BasicSession : std::enable_shared_from_this<BasicSession>
    {
        tcp::socket socket;
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
        async_read(session->socket,
                    asio::buffer(session->read_buffer, expected_read_),
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

public:
    size_t lifetime_connections_{0};
    std::atomic<size_t> lifetime_received_{0};

private:
    asio::io_context & cntx_;
    tcp::acceptor acceptor_;

    std::mutex clients_mutex_;
    std::unordered_set<std::shared_ptr<BasicSession>> clients_;

    size_t expected_read_{0};

    // Buffer to send.
    std::vector<uint8_t> send_bytes_;
};
