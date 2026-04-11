cmake_minimum_required(VERSION 3.9)
project("demo")

# --- PURE_OS_MODE DEMO --------------------------
add_executable(demo_pure_os_mode demo_pure_os_mode.c)
target_link_libraries(demo_pure_os_mode PRIVATE ldk)
target_compile_definitions(demo_pure_os_mode PRIVATE 
  $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
  $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
)

