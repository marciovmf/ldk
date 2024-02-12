cmake_minimum_required(VERSION 3.9)
project("demo")

add_executable(demo_pure_os_mode demo_pure_os_mode.c)
target_link_libraries(demo_pure_os_mode PRIVATE ldk)

add_executable(demo_mixed_mode demo_mixed_mode.c)
target_link_libraries(demo_mixed_mode PRIVATE ldk)


# We override the output foder so the demo gets placed in the runtree folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../runtree" CACHE PATH "" FORCE)
add_executable(demo main.c)
target_link_libraries(demo PRIVATE ldk)

