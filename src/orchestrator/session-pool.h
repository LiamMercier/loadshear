#pragma once

#include <cstddef>
#include <vector>

#include "session-config.h"

template<typename Session>
class SessionPool
{
public:
    SessionPool(const SessionConfig & config);

    void set_config(const SessionConfig & config);

    void create_sessions(size_t session_count);

private:

private:
    SessionConfig & config_;
    std::vector<Session> sessions_;
};
