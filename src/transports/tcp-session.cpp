#include "tcp-session.h"

TCPSession::TCPSession(asio::io_context & cntx,
                       const SessionConfig & config)
:config_(config),
strand_(cntx.get_executor()),
socket_(cntx),
incoming_header_(config_.header_size)
{
}

// TODO: check parser is not null before starting
void TCPSession::start(const std::vector<tcp::endpoint> & endpoints)
{
    asio::async_connect(socket_, endpoints,
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec,
                   tcp::endpoint ep){

            // If we failed to connect, stop.
            if (ec)
            {
                return;
            }

            this->on_connect();
        }));
}

void TCPSession::do_read_header()
{
    asio::async_read(socket_,
        asio::buffer(incoming_header_, config_.header_size),
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec, std::size_t){

                if (ec)
                {
                    this->handle_stream_error(ec);
                    return;
                }

                // TODO: user defined message parsing to get message size

                // Handle the server sending messages that are too big.
                if (next_payload_size_ > config_.payload_size_limit)
                {
                    this->handle_stream_error(ec);
                    return;
                }

                this->do_read_body();
        }));
}

void TCPSession::do_read_body()
{
    if (next_payload_size_ > MESSAGE_BUFFER_SIZE)
    {
        large_body_buffer_.resize(next_payload_size_);
        body_buffer_ptr_ = large_body_buffer_.data();
    }

    asio::async_read(socket_,
        asio::buffer(body_buffer_ptr_, next_payload_size_),
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t){

                if (ec)
                {
                    this->handle_stream_error(ec);
                    return;
                }

                // TODO: user message handling

            }));
}

void TCPSession::on_connect()
{
    // TODO: start header read
}

void TCPSession::stop()
{
    // TODO: stop the session and callback to the orchestrator
}

void TCPSession::close_session()
{

}

void TCPSession::handle_stream_error(boost::system::error_code ec)
{
    close_session();
    return;
}
