add_executable(unit-tests
    main-tests.cpp
    tcp-session-tests.cpp
    payload-manager-tests.cpp
    shard-tests.cpp
    orchestrator-tests.cpp
    interpreter-tests.cpp
    resolver-tests.cpp
)

target_include_directories(unit-tests PUBLIC wasmtime)

target_link_libraries(unit-tests PRIVATE
    GTest::gtest_main
    wasmtime
    transports
    packets
    orchestrator
    interpreter
    resolver
)

include(GoogleTest)

gtest_discover_tests(unit-tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
