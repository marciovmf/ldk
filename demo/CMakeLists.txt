cmake_minimum_required(VERSION 3.9)
project("demo")

# --- PURE_OS_MODE DEMO --------------------------
add_executable(demo demo.c)
target_link_libraries(demo PRIVATE ldk)
target_compile_definitions(demo PRIVATE 
  $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
  $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
)
