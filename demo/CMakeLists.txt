cmake_minimum_required(VERSION 3.9)
project("demo")

# --- PURE_OS_MODE DEMO --------------------------
add_executable(demo_pure_os_mode demo_pure_os_mode.c)
target_link_libraries(demo_pure_os_mode PRIVATE ldk)
target_compile_definitions(demo_pure_os_mode PRIVATE 
  $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
  $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
)

# --- MIXED_MODE DEMO ----------------------------
add_executable(demo_mixed_mode demo_mixed_mode.c)
target_link_libraries(demo_mixed_mode PRIVATE ldk)
target_compile_definitions(demo_mixed_mode PRIVATE
  $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
  $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
)

# --- PURE_ENGINE DEMO ---------------------------
add_executable(demo main.c)
target_link_libraries(demo PRIVATE ldk)
target_compile_definitions(demo PRIVATE 
  $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
  $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
)

add_custom_command(
  TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove "${OUTPUT_DIR}/assets/" 
  COMMAND ${CMAKE_COMMAND} -E remove "${OUTPUT_DIR}/ldk.cfg"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${RUNTREE_DIR}/assets/" "${OUTPUT_DIR}/assets/"
  COMMAND ${CMAKE_COMMAND} -E create_symlink "${RUNTREE_DIR}/ldk.cfg" "${OUTPUT_DIR}/ldk.cfg"
)

