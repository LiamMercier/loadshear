#include "tcp-session.h"

TCPSession::TCPSession(asio::io_context & cntx,
                       const SessionConfig & config)
:strand_(cntx.get_executor()),
socket_(cntx),
config_(config)
{
}

// TODO: check parser is not null before starting, report back if so.
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
    // TODO: user defined parser interface

    // TODO: header read logic
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
