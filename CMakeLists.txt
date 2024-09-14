cmake_minimum_required(VERSION 3.9)
project("ldk")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# --- GLOBAL DEFINES/OPTIONS----------------------
option(OPTION_ADDRESS_SANITIZER "When to enable address sanitizer" OFF)

# --- GLOBAL OUTPUT DIRECTORIES ------------------
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/ldk")
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/bin" CACHE PATH "" FORCE)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/lib" CACHE PATH "" FORCE)
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/lib" CACHE PATH "" FORCE)

if (OPTION_ADDRESS_SANITIZER)
  add_compile_options(-fsanitize=address)
endif()

include_directories("./include/") 

# --- TARGETS ------------------------------------
add_subdirectory(src)
add_subdirectory(demo)
