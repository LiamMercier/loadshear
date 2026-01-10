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
    metrics
    Boost::program_options
    wasmtime
    ftxui::screen
    ftxui::component
)

# wasmtime has an unused variable that we have errors on in release.
target_compile_options(cli PRIVATE -Wno-all)

add_executable(loadshear
    main.cpp
)

target_link_libraries(loadshear PRIVATE
    cli
)

# Install the binary
install(TARGETS loadshear
        RUNTIME DESTINATION bin)
