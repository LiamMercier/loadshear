#include "session-pool.h"

template<typename Session>
SessionPool<Session>::SessionPool(const SessionConfig & config)
:config_(config)
{

}

template<typename Session>
void SessionPool<Session>::set_config(const SessionConfig & config)
{
    config_ = config;
}

template<typename Session>
void SessionPool<Session>::create_sessions(size_t session_count)
{

}
