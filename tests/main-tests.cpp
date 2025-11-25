#include <gtest/gtest.h>

#include "wasm-message-handler.h"
#include "tcp-session.h"
#include "session-pool.h"

TEST(SessionPoolTests, TCPPool)
{
    SessionConfig config(8, 12288, true);
    WASMMessageHandler handler;
    asio::io_context cntx;

    SessionPool<TCPSession> pool(cntx, config);
    pool.create_sessions(100, cntx, config, handler);
}
