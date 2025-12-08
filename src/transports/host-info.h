#pragma once

#include <boost/asio/ip/tcp.hpp>

template <typename Session>
struct HostInfo
{
    typename Session::Endpoints endpoints;

    // TODO: config, i.e for TLS we have to store state
    //
    // Consider a shared_ptr since we keep a copy of this object per shard, and a
    // ssl context is thread safe. If the underlying config option is not thread safe,
    // force the session implementation to provide an atomic wrapper.
};
