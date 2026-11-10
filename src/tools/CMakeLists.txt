# src/tools/ — krc, krld, fixture extractor.

add_executable(extract_fixtures
    extract_fixtures/main.cpp
)

target_link_libraries(extract_fixtures PRIVATE krc_frontend)

add_executable(krc
    krc/main.cpp
)

target_link_libraries(krc PRIVATE krc_frontend krc_ir krc_backend krc_asm krc_debug)
target_include_directories(krc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../../backend ${CMAKE_CURRENT_SOURCE_DIR}/../../backend/targets/kr32 ${CMAKE_CURRENT_SOURCE_DIR}/../../debug)

add_subdirectory(krld)
