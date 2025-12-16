add_executable(loadshear
    main.cpp
    cli-parsing.cpp
    cli.cpp
    execution-plan.cpp
)

target_include_directories(loadshear PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(transports PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(loadshear PRIVATE
    interpreter
    resolver
    orchestrator
    transports
    packets
    Boost::program_options
)
