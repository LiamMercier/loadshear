cmake_minimum_required(VERSION 3.25)

project(Stressor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer"
    )
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

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
)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif()

add_subdirectory(src/packets)
add_subdirectory(src/transports)
add_subdirectory(tests)
