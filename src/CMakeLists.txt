cmake_minimum_required(VERSION 3.9)
project("ldk-core")
find_package(OpenGL REQUIRED)
set(INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include/ldk")

list(APPEND SOURCE
  ${INCLUDE_DIR}/ldk.h
  ${INCLUDE_DIR}/os.h
  ${INCLUDE_DIR}/gl.h gl.c
  ${INCLUDE_DIR}/hashmap.h hashmap.c
  ${INCLUDE_DIR}/maths.h maths.c
  ${INCLUDE_DIR}/arena.h arena.c
  ${INCLUDE_DIR}/hlist.h hlist.c
  ${INCLUDE_DIR}/engine.h engine.c
  ${INCLUDE_DIR}/common.h common.c
  ${INCLUDE_DIR}/eventqueue.h eventqueue.c
  ${INCLUDE_DIR}/module/graphics.h module/graphics.c
  ${INCLUDE_DIR}/module/asset.h module/asset.c
  ${INCLUDE_DIR}/module/renderer.h module/gl_renderer.c
  ${INCLUDE_DIR}/asset/shader.h asset/shader.c
  ${INCLUDE_DIR}/asset/material.h asset/material.c
  ${INCLUDE_DIR}/asset/image.h asset/image.c
  ${INCLUDE_DIR}/asset/mesh.h asset/mesh.c
  ${INCLUDE_DIR}/asset/texture.h asset/texture.c
  )

if(WIN32)
  # Windows specific sources
  list(APPEND SOURCE win32_os.c)
endif()

if (LDK_AS_SHARED_LIB)
  add_library(ldk SHARED ${SOURCE})
  target_compile_definitions(ldk PRIVATE LDK_EXPORT_API LDK_AS_SHARED_LIB)
else()
  add_library(ldk STATIC ${SOURCE})
endif()

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
      if(NOT config STREQUAL "Release")
            list(APPEND LIBS dbghelp.lib)
        endif()
    endforeach()
else()
    # Single-configuration generator (e.g., Makefile, Ninja)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND LIBS dbghelp.lib)
    endif()
endif()

target_include_directories(ldk PUBLIC ${INCLUDE_DIR})
target_link_libraries(ldk PRIVATE ${LIBS} ${OPENGL_LIBRARY})
