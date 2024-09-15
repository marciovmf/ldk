cmake_minimum_required(VERSION 3.9)
project("demo")


add_executable(demo_pure_os_mode demo_pure_os_mode.c)
target_link_libraries(demo_pure_os_mode PRIVATE ldk)

add_executable(demo_mixed_mode demo_mixed_mode.c)
target_link_libraries(demo_mixed_mode PRIVATE ldk)

add_executable(demo main.c)
target_link_libraries(demo PRIVATE ldk)


# --- MAKE RUTREE SYMLINK ------------------------
add_custom_command(
  TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove "${OUTPUT_DIR}/assets/" 
  COMMAND ${CMAKE_COMMAND} -E remove "${OUTPUT_DIR}/ldk.cfg"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${RUNTREE_DIR}/assets/" "${OUTPUT_DIR}/assets/"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${RUNTREE_DIR}/ldk.cfg" "${OUTPUT_DIR}/ldk.cfg"
  )

if (OPTION_BUILD_EDITOR)
  target_compile_definitions(demo PRIVATE LDK_EDITOR)
  target_compile_definitions(demo_mixed_mode PRIVATE LDK_EDITOR)
  target_compile_definitions(demo_mixed_mode PRIVATE LDK_EDITOR)
endif()

