# tests/ — doctest test suite (doctest is the only vendored test dependency).

add_executable(krc_tests
    test_sema.cpp
)

target_link_libraries(krc_tests PRIVATE krc_frontend doctest)

target_include_directories(krc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/frontend
)

if(MSVC)
    target_compile_options(krc_tests PRIVATE /W4 /WX /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(krc_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Register with ctest.
add_test(NAME krc_tests COMMAND krc_tests)
