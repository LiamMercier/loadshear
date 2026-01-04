#include "udp-session.h"

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

    body_buffer_.reserve();
}
