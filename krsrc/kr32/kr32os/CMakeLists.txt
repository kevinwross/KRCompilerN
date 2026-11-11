cmake_minimum_required(VERSION 3.25)
project(KR32OS NONE)

# KR32OS is built by the host kr32 toolchain, not by the C++ compiler used to
# build that toolchain. Point KR32 at the host executable if it is not in the
# default build location.
set(KR32 "" CACHE FILEPATH "Path to the kr32 host executable")

if(WIN32)
    add_custom_target(kr32os ALL
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/build.ps1" -Kr32 "${KR32}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
else()
    add_custom_target(kr32os ALL
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/build.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
endif()
