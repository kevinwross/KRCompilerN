# src/asm/ — assembler framework, syntax modules, encoders, relocations.

add_library(krc_asm
    object.cpp
    assembler.cpp
    kr16_assembler.cpp
)

target_include_directories(krc_asm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(krc_asm PUBLIC krc_backend)
