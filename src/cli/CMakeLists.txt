add_executable(loadshear
    loadshear-cli.cpp
)

target_include_directories(loadshear PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(transports PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(loadshear PRIVATE
    interpreter
    resolver
    orchestrator
    Boost::program_options
)
