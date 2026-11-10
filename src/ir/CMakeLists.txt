# src/ir/ — typed SSA intermediate representation (M2).
# Front end → IR → backend → assembler → linker; one-way dependencies.
# Diagnostics and source manager are shared services (live under src/frontend/
# for now; may be relocated to a shared location later).

add_library(krc_ir
    ir.h
    ir.cpp
    irgen.h
    irgen.cpp
)

target_include_directories(krc_ir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(krc_ir PRIVATE ${CMAKE_SOURCE_DIR}/src/frontend)
