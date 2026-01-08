#include "udp-session.h"

// TODO <feature>: report UDP errors when the endpoint does not exist, etc.

UDPSession::UDPSession(asio::io_context & cntx,
                       const SessionConfig & config,
                       const MessageHandler & message_handler,
                       const PayloadManager & payload_manager,
                       ShardMetrics & shard_metrics,
                       DisconnectCallback & on_disconnect)
:config_(config),
strand_(cntx.get_executor()),
socket_(cntx),
message_handler_(message_handler),
payload_manager_(payload_manager),
metrics_sink_(shard_metrics),
write_sample_counter_(config_.packet_sample_rate),
read_sample_counter_(config_.packet_sample_rate),
on_disconnect_(on_disconnect)
{
    size_t expected_body = (config_.payload_size_limit < MAX_DATAGRAM_SIZE ?
                            config_.payload_size_limit : MAX_DATAGRAM_SIZE);

    packet_buffer_.resize(expected_body);
}

// Always the first function called on the Session if any are called.
//
// NOTE: Connecting over UDP does not work like with TCP, we are only
//       setting operating system primitives and filtering for this endpoint.
void UDPSession::start(const Endpoints & endpoints)
{
    asio::post(strand_, [self = shared_from_this(), endpoints]{
        self->live_ = true;

        self->metrics_sink_.record_connection_attempt();

        boost::system::error_code ec;
        self->socket_.open(endpoints.protocol(), ec);

        // If we failed to open, stop.
        if (ec)
        {
            self->metrics_sink_.record_connection_fail();
            self->close_session();
            return;
        }

        self->socket_.connect(endpoints, ec);

        // If we failed to connect, stop.
        if (ec)
        {
            self->metrics_sink_.record_connection_fail();
            self->close_session();
            return;
        }

        self->metrics_sink_.record_connection_success();
        self->on_connect();
    });
}

// Request enabling flood.
void UDPSession::flood()
{
    asio::post(strand_, [self = shared_from_this()]{

        // If we are already flooding, don't try to open two flood loops.
        //
        // See the discussion above do_write() for why.
        if (self->flood_ || self->draining_)
        {
            return;
        }

        self->flood_ = true;

        // If safe to start write, go ahead
        if (self->live_)
        {
            self->try_start_write();
        }

    });
}

// Enqueue N payloads to be sent, if they exist.
void UDPSession::send(size_t N)
{
    asio::post(strand_, [self = shared_from_this(), N]{
        self->writes_queued_ += N;

        if (self->live_)
        {
            self->try_start_write();
        }
    });
}

void UDPSession::drain()
{
    asio::post(strand_, [self = shared_from_this()]{
        self->draining_ = true;

        if (!self->writing_
            && self->writes_queued_ == 0
            && self->responses_.empty())
        {
            self->close_session();
        }
    });
}

// Stop the session and callback to the orchestrator
void UDPSession::stop()
{
    asio::post(strand_, [self = shared_from_this()]{
        self->close_session();
    });
}

// on_connect runs inside of a strand after we open the UDP socket.
void UDPSession::on_connect()
{
    // Start the header read loop if setting is enabled.
    if (config_.read_messages)
    {
        do_read();
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
void UDPSession::do_read()
{
    // Every packet_sample_rate packets, record write latency.
    if (++read_sample_counter_ >= config_.packet_sample_rate)
    {
        read_start_time_ = std::chrono::steady_clock::now();
    }

    socket_.async_receive(
        asio::buffer(packet_buffer_),
        asio::bind_executor(strand_,
            [self = shared_from_this()](boost::system::error_code ec, size_t count){
                if (ec)
                {
                    self->close_session();
                    return;
                }

                self->packet_size_ = count;
                self->metrics_sink_.record_bytes_read(count);

                // Handle the server sending messages that are too big.
                if (self->packet_size_ > self->config_.payload_size_limit)
                {
                    self->close_session();
                    return;
                }

                // If we sampled, compute the latency.
                if (self->read_sample_counter_ > self->config_.packet_sample_rate)
                {
                    auto end = std::chrono::steady_clock::now();

                    uint64_t latency_us = static_cast<uint64_t>(
                            std::chrono::duration_cast
                                <std::chrono::microseconds>
                                    (
                                        end - self->read_start_time_
                                    ).count()
                                );

                    self->metrics_sink_.record_read_latency(latency_us);

                    self->read_sample_counter_ = 0;
                }

                // Handle the packet.
                self->handle_message();
        }));
}

// TODO <feature>: allow the user to split the header and body.
// Handles a server packet based on user set rules.
void UDPSession::handle_message()
{
    // Give the message handler the packet, there is no header to pass.
    message_handler_.parse_message(
        std::span<const uint8_t>(packet_buffer_.data(), 0),
        std::span<const uint8_t>(packet_buffer_.data(), packet_size_),
        [self = shared_from_this()](ResponsePacket response_packet) {

            asio::post(self->strand_, [self, response_packet]() {
                // Add to our responses and try to write.
                if (response_packet.packet->size() > 0)
                {
                    self->responses_.push_back(response_packet);

                    self->try_start_write();
                }

                self->do_read();
            });

    });

}

// Called from a strand on send or flood or connect.
void UDPSession::try_start_write()
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
    // Handle draining but nothing queued.
    else if (draining_)
    {
        close_session();
    }
}

void UDPSession::do_write()
{
    // Send responses first, then payloads.
    if (responses_.size() > 0)
    {
        ResponsePacket packet = responses_.front();
        responses_.pop_front();

        writing_ = true;

        // Every packet_sample_rate packets, record write latency.
        if (++write_sample_counter_ >= config_.packet_sample_rate)
        {
            write_start_time_ = std::chrono::steady_clock::now();
        }

        socket_.async_send(asio::buffer(packet.data(), packet.size()),
            asio::bind_executor(strand_,
                [self = shared_from_this(), packet](boost::system::error_code ec,
                                                    size_t count){
                    if (ec)
                    {
                        self->close_session();;
                        return;
                    }

                    self->metrics_sink_.record_bytes_sent(count);

                    // If we sampled, compute the latency.
                    if (self->write_sample_counter_ > self->config_.packet_sample_rate)
                    {
                        auto end = std::chrono::steady_clock::now();

                        uint64_t latency_us = static_cast<uint64_t>(
                                std::chrono::duration_cast
                                    <std::chrono::microseconds>
                                        (
                                            end - self->write_start_time_
                                        ).count()
                                    );

                        self->metrics_sink_.record_send_latency(latency_us);

                        self->write_sample_counter_ = 0;
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
            // Grab the payload from the payload manger.
            bool valid_payload = payload_manager_.fill_payload(next_payload_index_,
                                                               current_payload_);

            if (!valid_payload)
            {
                if (config_.loop_payloads && !draining_)
                {
                    next_payload_index_ = 0;
                    do_write();
                    return;
                }

                // If not looping, stop writing, we are done.
                writing_ = false;

                if (draining_)
                {
                    close_session();
                }

                return;
            }

            // Decrement after payload is valid.
            if (!flood_)
            {
                writes_queued_--;
            }

            // If we get here, we have a valid payload to write.
            next_payload_index_++;

            writing_ = true;

            // Every packet_sample_rate packets, record write latency.
            if (++write_sample_counter_ >= config_.packet_sample_rate)
            {
                write_start_time_ = std::chrono::steady_clock::now();
            }

            socket_.async_send(current_payload_.packet_slices,
            asio::bind_executor(strand_,
                [self = shared_from_this()](boost::system::error_code ec,
                                            size_t count){
                    if (ec)
                    {
                        self->close_session();
                        return;
                    }

                    self->metrics_sink_.record_bytes_sent(count);

                    // If we sampled, compute the latency.
                    if (self->write_sample_counter_ > self->config_.packet_sample_rate)
                    {
                        auto end = std::chrono::steady_clock::now();

                        uint64_t latency_us = static_cast<uint64_t>(
                                std::chrono::duration_cast
                                    <std::chrono::microseconds>
                                        (
                                            end - self->write_start_time_
                                        ).count()
                                    );

                        self->metrics_sink_.record_send_latency(latency_us);

                        self->write_sample_counter_ = 0;
                    }

                    // Call this function again to post another async_write call.
                    self->do_write();

                }));
        }
        else
        {
            // We stopped writing, set to false.
            writing_ = false;

            if (draining_)
            {
                close_session();
            }
        }

        return;
    }
}

// close_session runs in a strand
void UDPSession::close_session()
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
