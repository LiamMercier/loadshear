#include "tcp-session.h"

// TODO: remove
#include <iostream>

TCPSession::TCPSession(asio::io_context & cntx,
                       const SessionConfig & config,
                       const MessageHandler & message_handler)
:config_(config),
strand_(cntx.get_executor()),
socket_(cntx),
incoming_header_(config_.header_size),
message_handler_(message_handler)
{
}

void TCPSession::start(const Endpoints & endpoints)
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

void TCPSession::stop()
{
    // TODO: stop the session and callback to the orchestrator
}

// on_connect runs inside of a strand.
void TCPSession::on_connect()
{
    // Start the header read loop if setting is enabled.
    if (config_.read_messages)
    {
        do_read_header();
    }
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

                // User defined message parsing to get message size
                std::span<const uint8_t> header_bytes(incoming_header_);
                HeaderResult result = message_handler_.parse_header(header_bytes);

                // Handle errors, should only occur if we have a WASM call.
                if (result.status != HeaderResult::Status::OK)
                {
                    next_payload_size_ = 0;
                    this->do_read_header();
                    return;
                }

                next_payload_size_ = result.length;

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
    else
    {
        body_buffer_ptr_ = body_buffer_.data();
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

                this->handle_message();
            }));
}

// Handles a server packet based on user set rules.
void TCPSession::handle_message()
{
    // TODO: packet parsing interface

    // TODO: start header read while WASM work is posted to thread pool
}

void TCPSession::close_session()
{

}

void TCPSession::handle_stream_error(boost::system::error_code ec)
{
    close_session();
    return;
}
