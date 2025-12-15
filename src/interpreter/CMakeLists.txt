add_library(interpreter STATIC
    interpreter.cpp
    lexer.cpp
    parser.cpp
    diagnostic-colors.cpp
)

target_include_directories(interpreter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(interpreter PRIVATE
    orchestrator
    resolver
)
