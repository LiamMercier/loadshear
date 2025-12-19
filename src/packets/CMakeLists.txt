add_library(packets STATIC
wasm-message-handler.cpp
nop-message-handler.cpp
payload-manager.cpp)

target_include_directories(packets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(packets PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(packets PUBLIC wasmtime)

target_link_libraries(packets PRIVATE
    Boost::system
    wasmtime
)

# wasmtime has an unused variable that we have errors on in release.
target_compile_options(packets PRIVATE -Wno-all)
