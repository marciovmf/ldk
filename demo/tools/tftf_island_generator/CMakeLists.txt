cmake_minimum_required(VERSION 3.19)
project(tftf_island_generator LANGUAGES C)

add_executable(tftf_island_generator
    tftf_island_generator_cli.c
    ../../src/system/tftf_island_generator.c)
target_include_directories(tftf_island_generator PRIVATE ../../src/system)
set_target_properties(tftf_island_generator PROPERTIES C_STANDARD 11
    C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
if(NOT WIN32)
    target_link_libraries(tftf_island_generator PRIVATE m)
endif()
