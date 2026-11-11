# src/ — compiler sources. Subdirectories add their own libraries as M0–M3 progress.
# Front end must not depend on backend/linker/runtime; see AGENTS.md architecture boundaries.

add_subdirectory(frontend)
add_subdirectory(backend)
add_subdirectory(ir)
add_subdirectory(asm)
add_subdirectory(debug)
add_subdirectory(linker)
add_subdirectory(tools)
