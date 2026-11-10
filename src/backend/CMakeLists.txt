# src/backend/ — lowering, instruction selection, regalloc, frames, peepholes, target descriptors.

add_library(krc_backend
    machine.cpp
    debug_info.cpp
    instruction_selection.cpp
    register_allocation.cpp
    frame_layout.cpp
    target_descriptor.cpp
    targets/kr32/kr32.cpp
)

target_include_directories(krc_backend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ir ${CMAKE_CURRENT_SOURCE_DIR}/../frontend ${CMAKE_CURRENT_SOURCE_DIR}/targets/kr32)
