add_library(packets STATIC wasm-message-handler.cpp)

target_include_directories(packets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(packets PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(packets PUBLIC wasmtime)

target_link_libraries(packets PRIVATE
    Boost::system
    wasmtime
)
