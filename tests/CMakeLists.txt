add_executable(unit-tests
    main-tests.cpp
)

target_link_libraries(unit-tests PRIVATE
    GTest::gtest_main
    transports
    packets
    orchestrator
)

include(GoogleTest)

gtest_discover_tests(unit-tests)
