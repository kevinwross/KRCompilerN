# src/linker/ — object reader/writer, linker core, layout scripts, image writers.

add_library(krc_linker
    linker.cpp
)

target_include_directories(krc_linker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../asm ${CMAKE_CURRENT_SOURCE_DIR}/../debug)
target_link_libraries(krc_linker PUBLIC krc_asm)
