# src/frontend/ — preprocessor, lexer, parser, Sema (no backend/linker/runtime deps).

add_library(krc_frontend
    source_manager.cpp
    diagnostics.cpp
    lexer.cpp
    ast.cpp
    parser.cpp
    preprocessor.cpp
    sema.cpp
    debug_info.cpp
)

target_include_directories(krc_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
