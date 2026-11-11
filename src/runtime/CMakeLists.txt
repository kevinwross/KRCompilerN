# src/runtime/CMakeLists.txt — build runtime objects (crt0, libc) for KR targets.
#
# The runtime is compiled by krc for each target. A C-only bootstrap path is
# available for the first bring-up (compiled by the host compiler).
#
# crt0 is intentionally not built as part of the default build: compiling it
# requires the C front end to support goto/loops, which is an M1 front-end
# feature not yet implemented. It is compiled manually when needed.

add_library(krc_runtime_common
    common/hal.h
    common/allocator.c
    common/ctype.c
    common/errno.c
    common/fileio.c
    common/math.c
    common/stdio.c
    common/string.c
    common/stdlib.c
    common/time.c
    common/cxa_runtime.cpp
    common/unwinder.c
)

target_include_directories(krc_runtime_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

# Per-target runtime libraries (built when the corresponding target is
# supported by the front end).
add_library(krc_runtime_kr32
    kr32/crt0.c
    kr32/libc.c
    kr32/kr32os/kernel/console.c
    kr32/kr32os/kernel/main.c
    kr32/kr32os/kernel/memory.c
    kr32/kr32os/kernel/process.c
    kr32/kr32os/kernel/shell.c
    kr32/kr32os/kernel/syscall.c
    kr32/kr32os/kernel/timer.c
    kr32/kr32os/kernel/traps.c
    kr32/kr32os/kernel/user.c
    kr32/kr32os/user/fault.c
    kr32/kr32os/user/init.c
    kr32/kr32os/user/wx.c
)

target_include_directories(krc_runtime_kr32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_include_directories(krc_runtime_kr32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kr32/kr32os/kernel)
target_include_directories(krc_runtime_kr32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kr32/kr32os/user)
target_compile_options(krc_runtime_kr32 PRIVATE -fno-builtin -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)

add_library(krc_runtime_kr16
    kr16/crt0.c
)

target_include_directories(krc_runtime_kr16 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
