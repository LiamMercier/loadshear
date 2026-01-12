# Copyright (c) 2026 Liam Mercier
#
# This file is part of Loadshear.
#
# Loadshear is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License Version 3.0
# as published by the Free Software Foundation.
#
# Loadshear is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
# for more details.
#
# You should have received a copy of the GNU General Public License v3.0
# along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

add_executable(unit-tests
    main-tests.cpp
    tcp-session-tests.cpp
    payload-manager-tests.cpp
    shard-tests.cpp
    orchestrator-tests.cpp
    interpreter-tests.cpp
    resolver-tests.cpp
    cli-tests.cpp
    test-fixtures.cpp
    metric-tests.cpp
    udp-session-tests.cpp
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
    cli
    metrics
)

# wasmtime has an unused variable that we have errors on in release.
target_compile_options(unit-tests PRIVATE -Wno-all)

include(GoogleTest)

gtest_discover_tests(unit-tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
