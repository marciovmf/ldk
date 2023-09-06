cmake_minimum_required(VERSION 3.9)
project("ldk")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- GLOBAL OUTPUT DIRECTORIES ------------------
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/ldk")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/bin" CACHE PATH "" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/lib" CACHE PATH "" FORCE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/lib" CACHE PATH "" FORCE)

# --- GLOBAL DEFINES/OPTIONS----------------------
set(LDK_AS_SHARED_LIB FALSE CACHE BOOL "Set if LDK should be built as a shared library." )

if (LDK_AS_SHARED_LIB)
  message(STATUS "LDK_AS_SHARED_LIB = ${LDK_AS_SHARED_LIB}")
  add_compile_definitions(LDK_AS_SHARED_LIB)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include") 

# --- TARGETS ------------------------------------
add_subdirectory(src)
add_subdirectory(demo)
