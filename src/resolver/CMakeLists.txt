add_library(resolver STATIC
    resolver.cpp
)

target_include_directories(resolver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
