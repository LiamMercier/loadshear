add_library(transports STATIC tcp-session.cpp)

target_include_directories(transports PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(transports PRIVATE
    Boost::system
    packets
)
