# src/tools/krld/ — krld linker tool.

add_executable(krld main.cpp)

target_include_directories(krld PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../asm ${CMAKE_CURRENT_SOURCE_DIR}/../../linker ${CMAKE_CURRENT_SOURCE_DIR}/../../debug)
target_link_libraries(krld PRIVATE krc_linker krc_debug)
