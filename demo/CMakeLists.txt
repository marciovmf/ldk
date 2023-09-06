cmake_minimum_required(VERSION 3.9)
project("demo")

add_executable(demo main.c)
target_link_libraries(demo PRIVATE ldk)

