cmake_minimum_required(VERSION 3.25)

project(Stressor CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer"
    )
    add_compile_definitions(DEV_BUILD)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-interference-size")

# We need to use the wasmtime c-api since it is a rust project.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/wasmtime/crates/c-api ${CMAKE_CURRENT_BINARY_DIR}/wasmtime)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(Boost REQUIRED COMPONENTS
    system
    program_options
)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif()

add_subdirectory(src/packets)
add_subdirectory(src/transports)
add_subdirectory(src/orchestrator)
add_subdirectory(src/interpreter)
add_subdirectory(src/resolver)
add_subdirectory(tests)

add_subdirectory(src/cli)
