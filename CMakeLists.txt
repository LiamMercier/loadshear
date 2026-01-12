cmake_minimum_required(VERSION 3.25)

project(Loadshear
LANGUAGES CXX
VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer"
    )
    add_compile_definitions(DEV_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    add_link_options($<$<CONFIG:Release>:-s>)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-interference-size")

# Find Threads just in case we haven't already gotten the library.
find_package(Threads REQUIRED)

# We need to use the wasmtime c-api since it is a rust project.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/wasmtime/crates/c-api
                 ${CMAKE_CURRENT_BINARY_DIR}/wasmtime
                 EXCLUDE_FROM_ALL)

include(FetchContent)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/arthursonzogni/ftxui.git
    GIT_TAG v6.1.9
    GIT_SHALLOW TRUE
)

set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "Disable install")

FetchContent_MakeAvailable(ftxui)

# Try modern version first
find_package(Boost CONFIG COMPONENTS
    system
    program_options
)

# Fallback to our old way of finding boost.
if (NOT Boost_FOUND)
    if (POLICY CMP0167)
        cmake_policy(SET CMP0167 OLD)
    endif()
    find_package(Boost REQUIRED COMPONENTS
        system
        program_options
    )
endif()

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found!")
endif()

add_subdirectory(src/logger)
add_subdirectory(src/metrics)
add_subdirectory(src/packets)
add_subdirectory(src/transports)
add_subdirectory(src/orchestrator)
add_subdirectory(src/interpreter)
add_subdirectory(src/resolver)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_subdirectory(tests)
endif()

add_subdirectory(src/cli)

# Packaging settings.
set(CPACK_PACKAGE_NAME "loadshear")
set(CPACK_PACKAGE_VENDOR "Liam Mercier")
set(CPACK_PACKAGE_CONTACT "LiamMercier@proton.me")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a customizable load generator")

# The fact that we're targeteting Linux and making .deb/.rpm files really
# makes adding Linux to the file redundant.
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION share/doc/loadshear)

install(FILES "${CMAKE_SOURCE_DIR}/README.md"
        DESTINATION share/doc/loadshear)

# Bring over script examples
install(DIRECTORY sdk/scripts/
        DESTINATION share/doc/loadshear/examples/scripts
        FILES_MATCHING
        PATTERN "*.loadshear")

install(DIRECTORY sdk/scripts/
        DESTINATION share/doc/loadshear/examples/scripts
        FILES_MATCHING
        PATTERN "*.ldsh")

# Keep a copy of the contract for users to see.
install(FILES sdk/wasm/wasm-contract.h
        DESTINATION share/doc/loadshear/examples/wasm)

# Also bring over the documentation files.
install(DIRECTORY docs
        DESTINATION share/doc/loadshear)

set(CPACK_GENERATOR "DEB;RPM")

# Debian specific settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Liam Mercier <LiamMercier@proton.me>")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
"Loadshear is a customizable load generator designed for
high-concurrency testing of TCP and UDP endpoints.")

# RPM specific settings
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RPM_PACKAGE_AUTOPROV ON)

# Prevent installing read only header library installs
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_CURRENT_BINARY_DIR};${CMAKE_PROJECT_NAME};ALL;/")

include(CPack)
