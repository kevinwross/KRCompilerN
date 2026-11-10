# src/runtime/CMakeLists.txt — build runtime objects (crt0, libc) for KR targets.
#
# crt0 is intentionally not built as part of the default build: compiling it
# requires the C front end to support goto/loops, which is an M1 front-end
# feature not yet implemented. It is compiled manually when needed.
