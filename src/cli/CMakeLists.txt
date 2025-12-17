add_library(cli STATIC
    cli-parsing.cpp
    cli.cpp
    execution-plan.cpp
)

target_include_directories(cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(cli PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(cli PUBLIC wasmtime)

target_link_libraries(cli PUBLIC
    interpreter
    resolver
    orchestrator
    transports
    packets
    logger
    Boost::program_options
    wasmtime
)

add_executable(loadshear
    main.cpp
)

target_link_libraries(loadshear PRIVATE
    cli
)
