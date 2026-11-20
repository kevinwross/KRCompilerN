# tests/ — doctest test suite (doctest is the only vendored test dependency).

# doctest is header-only; provide an INTERFACE target so consumers can link it.
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
)

add_executable(krc_tests
    test_sema.cpp
    test_mangle.cpp
    test_determinism.cpp
    test_target_descriptor.cpp
    test_kr16_encoding.cpp
    test_machine_ir.cpp
    test_register_allocation.cpp
    test_krdap.cpp
    test_frame_layout.cpp
    test_debug_info.cpp
    test_irgen.cpp
)

target_link_libraries(krc_tests PRIVATE krc_frontend krc_asm krc_backend krc_linker krc_debug krc_ir doctest)

target_include_directories(krc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backend
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/backend/targets/kr16
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/asm
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/linker
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/debug
)

target_compile_definitions(krc_tests PRIVATE
    PROJECT_BINARY_DIR="${CMAKE_BINARY_DIR}"
)

if(MSVC)
    target_compile_options(krc_tests PRIVATE /W4 /WX /permissive- /Wv:18)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(krc_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(krc_tests PRIVATE -Wno-cpp)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(krc_tests PRIVATE "-Wno-error=\#warnings")
    endif()
endif()

# Register with ctest.
add_test(NAME krc_tests COMMAND krc_tests)
