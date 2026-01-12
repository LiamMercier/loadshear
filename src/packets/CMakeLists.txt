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

add_library(packets STATIC
wasm-message-handler.cpp
nop-message-handler.cpp
payload-manager.cpp)

target_include_directories(packets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(packets PRIVATE
    logger
    Boost::system
    wasmtime
)

# wasmtime has an unused variable that we have errors on in release.
target_compile_options(packets PRIVATE -Wno-all)
