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

cmake_minimum_required(VERSION 3.25)

project(Loadshear
LANGUAGES CXX
VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_DEB "Build .deb packages" OFF)
option(BUILD_RPM "Build .rpm packages" OFF)
option(BUILD_FREEBSD  "Build FreeBSD packages" OFF)

# Port file should set this to off.
option(SET_OPTIMIZATIONS "Set compiler to strip, optimize" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer"
    )
    add_compile_definitions(DEV_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    # If building from port, we skip (set BUILD_FREEBSD off)
    if(SET_OPTIMIZATIONS)
        add_compile_options($<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-O2>)
        add_link_options($<$<CONFIG:Release>:-s>)
    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-interference-size")
endif()

# We need to give FreeBSD the experimental library flag for certain features
# in this program. This might not be necessary in the future when C++23
# is given better support
if(BUILD_FREEBSD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library")
endif()

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

if(BUILD_TESTING)
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
set(CPACK_PACKAGE_FILE_NAME
"${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")

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

# Bring over third party licenses.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/licenses"
        DESTINATION share/doc/loadshear/third_party)

set(CPACK_GENERATOR "TGZ")

if(BUILD_FREEBSD)
    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(CPACK_GENERATOR "${CPACK_GENERATOR};FREEBSD")
    endif()
endif()

if(BUILD_DEB)
    set(CPACK_GENERATOR "${CPACK_GENERATOR};DEB")
endif()

if(BUILD_RPM)
    set(CPACK_GENERATOR "${CPACK_GENERATOR};RPM")
endif()

# Debian specific settings
if(BUILD_DEB)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Liam Mercier <LiamMercier@proton.me>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Loadshear is a customizable load generator designed for
    high-concurrency testing of TCP and UDP endpoints.")
endif()

# RPM specific settings
if(BUILD_RPM)
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_PACKAGE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RPM_PACKAGE_AUTOPROV ON)
endif()

# FreeBSD package settings
if(BUILD_FREEBSD)
    set(CPACK_FREEBSD_PACKAGE_LICENSE GPL-3.0-only)
endif()

# Prevent installing read only header library installs
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_CURRENT_BINARY_DIR};${CMAKE_PROJECT_NAME};ALL;/")

include(CPack)
