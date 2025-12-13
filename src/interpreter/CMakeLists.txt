add_library(interpreter STATIC
    interpreter.cpp
    lexer.cpp
    parser.cpp
)

# TODO: consider what we actually need
target_include_directories(interpreter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(interpreter PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(interpreter PRIVATE
    Boost::system
    orchestrator
    resolver
)
