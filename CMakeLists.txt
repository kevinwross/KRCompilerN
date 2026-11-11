cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(krc VERSION 0.1.0 LANGUAGES C CXX)

#-----------------------------------------------------------------------------
# Options
#-----------------------------------------------------------------------------
set(KRC_CXX_STANDARD 20 CACHE STRING "C++ language standard")
set(KRC_ENABLE_SANITIZERS OFF CACHE BOOL "Enable ASan/UBSan (Linux clang only)")
set(KRC_BUILD_TESTS ON CACHE BOOL "Build the doctest test suite")
set(KRC_VENDORED_DOCTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/third_party/doctest" CACHE PATH "Vendored doctest source directory")

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${KRC_CXX_STANDARD})
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#-----------------------------------------------------------------------------
# Warning flags: warnings-as-errors on both toolchains (AGENTS.md §15.4)
#   MSVC  : /W4 /WX /permissive-
#   GCC/Clang: -Wall -Wextra -Wpedantic -Werror
#-----------------------------------------------------------------------------
if(MSVC)
  foreach(flag IN ITEMS /W4 /WX /permissive-)
    add_compile_options($<$<CONFIG:Debug,Release,RelWithDebInfo,MinSizeRel>:${flag}>)
  endforeach()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(flag IN ITEMS -Wall -Wextra -Wpedantic -Werror)
    add_compile_options($<$<CONFIG:Debug,Release,RelWithDebInfo,MinSizeRel>:${flag}>)
  endforeach()
  if(KRC_ENABLE_SANITIZERS)
    add_compile_options("-fsanitize=address,undefined")
    add_link_options("-fsanitize=address,undefined")
  endif()
endif()

#-----------------------------------------------------------------------------
# doctest is the only vendored test dependency; vendored source tree, no fetch.
#-----------------------------------------------------------------------------
if(KRC_BUILD_TESTS AND EXISTS "${KRC_VENDORED_DOCTEST_DIR}/doctest.h")
  set(KRC_DOCTEST_FOUND TRUE)
  add_library(doctest INTERFACE IMPORTED)
  target_include_directories(doctest INTERFACE "${KRC_VENDORED_DOCTEST_DIR}")
else()
  set(KRC_DOCTEST_FOUND FALSE)
  message(STATUS "doctest not vendored at ${KRC_VENDORED_DOCTEST_DIR}; tests disabled")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(src)
add_subdirectory(krsrc)
if(KRC_BUILD_TESTS AND KRC_DOCTEST_FOUND)
  enable_testing()
  add_subdirectory(tests)
endif()
