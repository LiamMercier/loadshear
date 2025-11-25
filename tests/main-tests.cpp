#include <gtest/gtest.h>

#include "wasm-message-handler.h"
#include "tcp-session.h"
#include "session-pool.h"

TEST(SessionPoolTests, TCPPool)
{
    SessionConfig config(8, 12288, true);

    WASMMessageHandler handler;
    handler.set_header_parser([](std::span<const uint8_t> buffer) -> HeaderResult
    {
        return {0, HeaderResult::Status::OK};
    });

    asio::io_context cntx;

    SessionPool<TCPSession> pool(cntx, config);
    pool.create_sessions(100, cntx, config, handler);
}
