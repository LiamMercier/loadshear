// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <boost/asio/ip/tcp.hpp>

template <typename Session>
struct HostInfo
{
    typename Session::Endpoints endpoints;

    // TODO <feature>: other session config, state if we support TLS later.
    //
    // Consider a shared_ptr since we keep a copy of this object per shard, and a
    // ssl context is thread safe. If the underlying config option is not thread safe,
    // force the session implementation to provide an atomic wrapper.
};
