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

// TODO: change writes to atomics to be relaxed inside the strand.

// Always the first function called on the Session if any are called.
void TCPSession::start(const Endpoints & endpoints)
{
    asio::post(strand_, [self = shared_from_this(), endpoints]{
        self->live_ = true;
        self->connecting_ = true;

        asio::async_connect(self->socket_, endpoints,
            asio::bind_executor(self->strand_,
                [self](boost::system::error_code ec,
                    tcp::endpoint ep){
                self->connecting_ = false;

                // If we failed to connect, stop.
                if (ec)
                {
                    self->close_session();
                    return;
                }

                self->on_connect();
            }));
    });
}

// Request enabling flood.
void TCPSession::flood()
{
    asio::post(strand_, [self = shared_from_this()]{

        // If we are already flooding, don't try to open two flood loops.
        //
        // See the discussion above do_flood() for why.
        if (self->flood_)
        {
            return;
        }

        self->flood_ = true;

        // If safe to start write, go ahead
        if (self->live_ && !self->connecting_)
        {
            self->try_start_write();
        }

    });
}

// Enqueue N payloads to be sent, if they exist.
void TCPSession::send(size_t N)
{
    asio::post(strand_, [self = shared_from_this(), N]{
        self->writes_queued_ += N;

        if (self->live_ && !self->connecting_)
        {
            self->try_start_write();
        }
    });
}

// Stop the session and callback to the orchestrator
void TCPSession::stop()
{
    asio::post(strand_, [self = shared_from_this()]{
        self->close_session();
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

    if (writing_)
    {
        return;
    }

    if (flood_ || writes_queued_ > 0)
    {
        do_write();
    }
}

// do_read_header runs inside of a strand
void TCPSession::do_read_header()
{
    asio::async_read(socket_,
        asio::buffer(incoming_header_, config_.header_size),
        asio::bind_executor(strand_,
            [self = shared_from_this()](boost::system::error_code ec, std::size_t){
                if (ec)
                {
                    self->handle_stream_error(ec);
                    return;
                }

                // User defined message parsing to get message size
                std::span<const uint8_t> header_bytes(self->incoming_header_);
                HeaderResult result = self->message_handler_.parse_header(header_bytes);

                // Handle errors, should only occur if we have a WASM call.
                if (result.status != HeaderResult::Status::OK)
                {
                    self->next_payload_size_ = 0;
                    self->do_read_header();
                    return;
                }

                self->next_payload_size_ = result.length;

                // Handle the server sending messages that are too big.
                if (self->next_payload_size_ > self->config_.payload_size_limit)
                {
                    self->handle_stream_error(ec);
                    return;
                }

                self->do_read_body();
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

    asio::async_read(socket_,
        asio::buffer(body_buffer_ptr_, next_payload_size_),
        asio::bind_executor(strand_,
            [self = shared_from_this()](boost::system::error_code ec, size_t){
                if (ec)
                {
                    self->handle_stream_error(ec);
                    return;
                }

                self->handle_message();
            }));
}

// Handles a server packet based on user set rules.
void TCPSession::handle_message()
{
    message_handler_.parse_body_async(
        std::span<const uint8_t>(body_buffer_ptr_, body_buffer_ptr_ + next_payload_size_),
        [self = shared_from_this()](ResponsePacket response_packet) {

            asio::post(self->strand_, [self, response_packet]() {
                // Add to our responses and try to write.
                self->responses_.push_back(response_packet);

                self->try_start_write();
            });

    });

}

// Called from a strand.
void TCPSession::try_start_write()
{
    // Clearly we can't start another loop.
    if (writing_)
    {
        return;
    }

    // If we have a response to deliver or payloads.
    if (flood_ || writes_queued_ > 0 || responses_.size() > 0)
    {
        do_write();
    }
}

// There should only be one outstanding write per socket to maximize throughput. Why?
// - Having N async_write operations at once just increases backpressure on the socket
// - Filling the socket with data too fast will eventually consume userspace memory
// - We would not be writing to N different sockets, just N times to a single socket
// - Posting (N * num_sessions) different async_writes will often overwhelm the thread pool
// - We may interleave writes to the socket creating garbage data
//
// Why not coalese the entire queue of payloads to write everything once then?
// - The payloads might exceed SO_SNDBUF and so we again consume extra userspace memory
// - If we wanted to simulate sending a maximally coalesced payload, we can supply a custom packet.
void TCPSession::do_write()
{
    // Send responses first, then payloads.
    if (responses_.size() > 0)
    {
        ResponsePacket packet = responses_.front();
        responses_.pop_front();

        writing_ = true;

        asio::async_write(socket_, asio::buffer(packet.data(), packet.size()),
            asio::bind_executor(strand_,
                [self = shared_from_this(), packet](boost::system::error_code ec, size_t){
                    if (ec)
                    {
                        self->handle_stream_error(ec);
                        return;
                    }

                    // Call this function again to post another async_write call.
                    self->do_write();

                }));

        return;
    }
    else
    {
        // If flooding or writes are queued, write payloads.
        if (flood_ || writes_queued_ > 0)
        {
            // TODO: grab the payload from the payload manger.
            //
            // TODO: if we find that there are no more payloads, set writing to false and return.

            writing_ = true;

            // asio::async_write(socket_, <BUFFER  HERE>,
            // asio::bind_executor(strand_,
            //     [this, <PAYLOAD>](boost::system::error_code ec, size_t){
            //         if (ec)
            //         {
            //             handle_stream_error(ec);
            //             return;
            //         }
            //
            //         // Call this function again to post another async_write call.
            //         if (live_)
            //         {
            //             do_flood();
            //         }
            //
            //     }));
        }
        else
        {
            // We stopped writing, set to false.
            writing_ = false;
        }

        return;
    }
}

// close_session runs in a strand
void TCPSession::close_session()
{
    // Prevent calling twice.
    if (!live_)
    {
        return;
    }

    boost::system::error_code ignored;
    socket_.cancel(ignored);
    socket_.close(ignored);

    live_ = false;

    on_disconnect_();
}

void TCPSession::handle_stream_error(boost::system::error_code ec)
{
    close_session();
    return;
}
