#include "tcp-session.h"

// TODO: remove
#include <iostream>

TCPSession::TCPSession(asio::io_context & cntx,
                       const SessionConfig & config,
                       const MessageHandler & message_handler,
                       const PayloadManager & payload_manager,
                       DisconnectCallback & on_disconnect)
:config_(config),
strand_(cntx.get_executor()),
socket_(cntx),
incoming_header_(config_.header_size),
message_handler_(message_handler),
payload_manager_(payload_manager),
on_disconnect_(on_disconnect)
{
}

void TCPSession::start(const Endpoints & endpoints)
{
    asio::post(strand_, [this, endpoints]{
        connecting_ = true;
        live_ = true;
        pending_ops_++;

        asio::async_connect(socket_, endpoints,
            asio::bind_executor(strand_,
                [this](boost::system::error_code ec,
                    tcp::endpoint ep){

                pending_ops_--;
                connecting_ = false;

                // Check if we need to exit now.
                if(should_disconnect())
                {
                    on_disconnect_();
                    return;
                }

                // If we failed to connect, stop.
                if (ec)
                {
                    this->close_session();
                    return;
                }

                this->on_connect();
            }));
    });
}

// Request enabling flood.
void TCPSession::flood()
{
    asio::post(strand_, [this]{
        // TODO: do_flood();
    });
}

// Stop the session and callback to the orchestrator
void TCPSession::stop()
{
    asio::post(strand_, [this]{
        this->close_session();
    });
}

// Forcefully stop, ignoring all data to be sent.
void TCPSession::halt()
{
    asio::dispatch(strand_, [this]{
        this->close_session();
    });
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

// do_read_header runs inside of a strand
void TCPSession::do_read_header()
{
    pending_ops_++;

    asio::async_read(socket_,
        asio::buffer(incoming_header_, config_.header_size),
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec, std::size_t){
                pending_ops_--;
                if(should_disconnect())
                {
                    on_disconnect_();
                    return;
                }

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

// do_read_body runs inside a strand
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

    pending_ops_++;

    asio::async_read(socket_,
        asio::buffer(body_buffer_ptr_, next_payload_size_),
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t){
                pending_ops_--;
                if(should_disconnect())
                {
                    on_disconnect_();
                    return;
                }

                if (ec)
                {
                    this->handle_stream_error(ec);
                    return;
                }

                this->handle_message();
            }));
}

// Write a response back to the server.
//
// This function runs in a strand.
void TCPSession::do_write_response()
{
    pending_ops_++;

    // Grab the next packet to write.
    ResponsePacket packet = responses_.front();
    responses_.pop_front();

    asio::async_write(socket_, asio::buffer(packet.data(), packet.size()),
        asio::bind_executor(strand_,
            [this](boost::system::error_code ec, size_t s)
            {
                pending_ops_--;
                if (should_disconnect())
                {
                    on_disconnect_();
                    return;
                }

                if (ec)
                {
                    handle_stream_error(ec);
                    return;
                }

                // Post another write if more responses exist.
                //
                // Our message adding callback doesn't need to check
                // since it is on the strand and adds a message.
                if (!responses_.empty())
                {
                    pending_ops_++;
                    asio::post(strand_,
                        [this]() {
                            pending_ops_--;

                            if (should_disconnect())
                            {
                                on_disconnect_();
                                return;
                            }
                            do_write_response();
                    });
                }

            }));
}

// Handles a server packet based on user set rules.
void TCPSession::handle_message()
{
    pending_ops_++;

    message_handler_.parse_body_async(
        std::span<const uint8_t>(body_buffer_ptr_, body_buffer_ptr_ + next_payload_size_),
        [this](ResponsePacket response_packet) {
            asio::post(strand_, [this, response_packet]() {
                pending_ops_--;

                if (should_disconnect())
                {
                    on_disconnect_();
                    return;
                }

                // Add to our responses and try to write.
                responses_.push_back(response_packet);

                if (!flood_)
                {
                    do_write_response();
                }
            });
    });

}

// close_session runs in a strand
void TCPSession::close_session()
{
    if (!live_ && !connecting_)
    {
        return;
    }

    boost::system::error_code ignored;
    socket_.cancel(ignored);
    socket_.close(ignored);

    live_ = false;

    // Call on_disconnect_ once every other handler has run.
    //
    // This assumes that the pool SHALL NOT call any other functions after.
    if (pending_ops_ == 0)
    {
        on_disconnect_();
    }
}

void TCPSession::handle_stream_error(boost::system::error_code ec)
{
    close_session();
    return;
}
