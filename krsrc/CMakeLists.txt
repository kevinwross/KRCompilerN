# krsrc/CMakeLists.txt — target runtime source, compiled by krc (the KRCompiler).
#
# Build order: src/ (compiler) first, then krsrc/ (runtime using krc).
# Output goes to build/krsrc/<target>/ for each CPU target.
#
# Architecture note: krc is a cross-compiler for KR targets. It produces
# KR target object files, not host object files. The krsrc runtime source
# is compiled by krc after krc itself has been built from src/.

cmake_minimum_required(VERSION 3.25)
project(KRsrc NONE)

#-----------------------------------------------------------------------------
# Target CPU architectures. Each gets its own build/krsrc/<target>/ output.
# Common sources are compiled once per target.
#-----------------------------------------------------------------------------
set(KRSRC_TARGETS kr32 kr16)

# Common runtime sources — compiled for every CPU target.
# Headers are NOT listed here; they are found via include directories.
set(KRSRC_COMMON_SOURCES
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

foreach(target ${KRSRC_TARGETS})
    # Output directory: build/krsrc/<target>/
    set(krsrc_target_out "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    file(MAKE_DIRECTORY "${krsrc_target_out}")

    # Collect source files for this target.
    set(krsrc_sources "")
    set(krsrc_objects "")

    # Common sources (built for each target, output under build/krsrc/<target>/common/)
    foreach(src ${KRSRC_COMMON_SOURCES})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
            list(APPEND krsrc_sources "${src}")
        endif()
    endforeach()

    # Target-specific sources.
    if(target STREQUAL "kr32")
        list(APPEND krsrc_sources
            kr32/crt0.c
            kr32/libc.c)
    elseif(target STREQUAL "kr16")
        list(APPEND krsrc_sources
            kr16/crt0.c
            kr16/fileio.c)
    endif()

    # Filter to files that actually exist and compute object paths.
    set(krsrc_existing_sources "")
    foreach(src ${krsrc_sources})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
            list(APPEND krsrc_existing_sources "${src}")

            get_filename_component(src_name "${src}" NAME_WE)
            get_filename_component(src_dir "${src}" DIRECTORY)
            set(obj_out_dir "${krsrc_target_out}/${src_dir}")
            file(MAKE_DIRECTORY "${obj_out_dir}")
            set(obj_file "${obj_out_dir}/${src_name}.o")
            list(APPEND krsrc_objects "${obj_file}")

            add_custom_command(
                OUTPUT "${obj_file}"
                COMMAND $<TARGET_FILE:krc> -target ${target} -c -I "${CMAKE_CURRENT_SOURCE_DIR}/common" -I "${CMAKE_CURRENT_SOURCE_DIR}/${target}" -o "${obj_file}" "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
                DEPENDS krc "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
                COMMENT "Compiling ${src} for ${target}"
                VERBATIM)
        endif()
    endforeach()

    if(krsrc_existing_sources)
        # Create a custom target for this target's runtime.
        # Depends on krc being built first (from src/) and on the object files.
        add_custom_target(krsrc_${target}
            COMMAND ${CMAKE_COMMAND} -E echo "krsrc ${target} runtime compiled"
            DEPENDS krc ${krsrc_objects}
            COMMENT "Compiling krsrc runtime for ${target} using krc"
            VERBATIM)
    endif()
endforeach()

#-----------------------------------------------------------------------------
# Aggregate target: build all krsrc runtime targets.
# Opt-in: build with --target krsrc after krc is available.
#-----------------------------------------------------------------------------
add_custom_target(krsrc
    DEPENDS krsrc_kr32 krsrc_kr16
    COMMENT "Building all krsrc runtime targets using krc"
    VERBATIM)
