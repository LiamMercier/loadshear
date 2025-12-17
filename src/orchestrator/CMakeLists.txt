add_library(orchestrator STATIC session-pool.h shard.h orchestrator.h)

target_include_directories(orchestrator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(orchestrator PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(orchestrator PRIVATE
    Boost::system
    transports
    packets
    logger
)
