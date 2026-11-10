# src/debug/ — symbol/debug format reader/writer, dumper, shared debugger reader.

add_library(krc_debug
    kdbg.cpp
)

target_include_directories(krc_debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../asm ${CMAKE_CURRENT_SOURCE_DIR}/../backend)
