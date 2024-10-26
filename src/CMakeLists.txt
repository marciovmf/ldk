cmake_minimum_required(VERSION 3.9)
project("ldk-core")
find_package(OpenGL REQUIRED)

# --- GLOBAL DEFINES/OPTIONS----------------------
option(OPTION_LDKSHAREDLIB "When to build engine as a shared library" OFF)
option(OPTION_BUILD_EDITOR "When to build the editor" OFF)
option(OPTION_BUILD_TESTS "When to built and run tests" OFF)

add_custom_target(print_options ALL
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "CONFIG                    $<CONFIG>"
  COMMAND ${CMAKE_COMMAND} -E echo "OUTPUT_DIR                ${RUNTREE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_ADDRESS_SANITIZER  ${OPTION_ADDRESS_SANITIZER}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_LDKSHAREDLIB       ${OPTION_LDKSHAREDLIB}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_EDITOR       ${OPTION_BUILD_EDITOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_TESTS        ${OPTION_BUILD_TESTS}"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  )

# --- SOURCES ------------------------------------
set(INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include/ldk")
list(APPEND SOURCE
  # Core
  ${INCLUDE_DIR}/ldk.h
  ${INCLUDE_DIR}/os.h
  ${INCLUDE_DIR}/argparse.h argparse.c
  ${INCLUDE_DIR}/gl.h gl.c
  ${INCLUDE_DIR}/maths.h maths.c
  ${INCLUDE_DIR}/arena.h arena.c
  ${INCLUDE_DIR}/hlist.h hlist.c
  ${INCLUDE_DIR}/engine.h engine.c
  ${INCLUDE_DIR}/common.h common.c
  ${INCLUDE_DIR}/eventqueue.h eventqueue.c

  # Data structures
  ${INCLUDE_DIR}/hashmap.h hashmap.c
  ${INCLUDE_DIR}/array.h array.c

  # Entities
  ${INCLUDE_DIR}/entity/camera.h entity/camera.c
  ${INCLUDE_DIR}/entity/staticobject.h entity/staticobject.c
  ${INCLUDE_DIR}/entity/instancedobject.h entity/instancedobject.c
  ${INCLUDE_DIR}/entity/pointlight.h entity/pointlight.c
  ${INCLUDE_DIR}/entity/directionallight.h entity/directionallight.c
  ${INCLUDE_DIR}/entity/spotlight.h entity/spotlight.c

  # Modules
  ${INCLUDE_DIR}/module/graphics.h module/graphics.c
  ${INCLUDE_DIR}/module/asset.h module/asset.c
  ${INCLUDE_DIR}/module/entity.h module/entity.c
  ${INCLUDE_DIR}/module/renderer.h
  module/renderer_gl.c
  module/rendererbackend_gl.c

  # assets
  ${INCLUDE_DIR}/asset/shader.h asset/shader.c
  ${INCLUDE_DIR}/asset/material.h asset/material.c
  ${INCLUDE_DIR}/asset/image.h asset/image.c
  ${INCLUDE_DIR}/asset/mesh.h asset/mesh.c
  ${INCLUDE_DIR}/asset/texture.h asset/texture.c
  ${INCLUDE_DIR}/asset/config.h asset/config.c
  )

if (OPTION_BUILD_EDITOR)
  list(APPEND SOURCE ${INCLUDE_DIR}/module/editor.h module/editor.c)
  endif()

  if(WIN32)
    # Windows specific sources and libs
    list(APPEND SOURCE win32_os.c)
    set(LIBS dbghelp.lib)

    # Release
    list(APPEND Release_LIBS ${LIBS})
    list(APPEND Release_DEFINITIONS)
    list(APPEND Release_COMPILE_OPTIONS /O2)

    # RelWithDebInfo
    list(APPEND RelWithDebInfo_LIBS ${LIBS})
    list(APPEND RelWithDebInfo_DEFINITIONS ${Release_DEFINITIONS} LDK_DEBUG)
    list(APPEND RelWithDebInfo_COMPILE_OPTIONS ${Release_COMPILE_OPTIONS})

    # MinSizeRel
    list(APPEND MinSizeRel_LIBS ${LIBS})
    list(APPEND MinSizeRel_DEFINITIONS ${Relese_DEFINITIONS} LDK_DEBUG)
    list(APPEND MinSizeRel_COMPILE_OPTIONS ${Release_COMPILE_OPTIONS})

    # Debug
    list(APPEND Debug_LIBS ${LIBS})
    list(APPEND Debug_DEFINITIONS LDK_DEBUG LDK_EDITOR LDK_DEBUG)
    list(APPEND Debug_COMPILE_OPTIONS /Zi /DEBUG)
  else()
    message(FATAL_ERROR "UNSUPPORTED PLATFORM")
  endif()

  if (OPTION_LDKSHAREDLIB)
    set(LIBTYPE "SHARED")
  else()
    set(LIBTYPE "STATIC")
  endif()

  add_library(ldk ${LIBTYPE} ${SOURCE})

  target_compile_definitions(ldk PRIVATE
    LDK_ENGINE
    $<$<CONFIG:Release>:${Release_DEFINITIONS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_DEFINITIONS}>
    $<$<CONFIG:Debug>:${Debug_DEFINITIONS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_DEFINITIONS}>
    $<$<BOOL:${OPTION_BUILD_EDITOR}>:LDK_EDITOR>
    $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>
    LDK_BUILD_TYPE="$<CONFIG>"
    )

  target_compile_options(ldk PRIVATE
    $<$<CONFIG:Release>:${Release_COMPILE_OPTIONS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_COMPILE_OPTIONS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_COMPILE_OPTIONS}>
    $<$<CONFIG:Debug>:${Debug_COMPILE_OPTIONS}>
    )

  target_link_libraries(ldk PRIVATE
    $<$<CONFIG:Release>:${Release_LIBS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_LIBS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_LIBS}>
    $<$<CONFIG:Debug>:${Debug_LIBS}>
    )

  target_link_libraries(ldk PUBLIC ${OPENGL_LIBRARY})
  target_include_directories(ldk PUBLIC ${INCLUDE_DIR})
  add_dependencies(ldk print_options)

  #----------------------------------------------
  # Tests
  #----------------------------------------------

  if (OPTION_BUILD_TESTS)
    function(create_test)
      set(options "")
      set(oneValueArgs TARGET)
      set(multiValueArgs SOURCES LIBRARIES)
      cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

      if(NOT ARG_TARGET)
        message(FATAL_ERROR "TARGET parameter is required for create_test function.")
        return()
      endif()

      if(NOT ARG_SOURCES)
        message(FATAL_ERROR "SOURCES parameter is required for create_test function.")
        return()
      endif()

      if(NOT ARG_LIBRARIES)
        set(ARG_LIBRARIES ldk)
      else()
        list(APPEND ARG_LIBRARIES ldk)
      endif()

      add_executable(${ARG_TARGET} ${ARG_SOURCES})
      set_target_properties(${ARG_TARGET} PROPERTIES OUTPUT_NAME ${ARG_TARGET}${OUTPUT_NAME_SUFFIX})
      target_include_directories(${ARG_TARGET} PUBLIC ${INCLUDE_DIR})
      target_link_libraries(${ARG_TARGET} PRIVATE ${ARG_LIBRARIES})
      add_dependencies(${ARG_TARGET} ldk)

      add_custom_target(test_${ARG_TARGET} ALL
        COMMAND $<TARGET_FILE:${ARG_TARGET}>
        COMMENT "Testing $<TARGET_FILE:${ARG_TARGET}>"
        DEPENDS ${ARG_TARGET} ldk)
    endfunction()

    create_test(TARGET test_maths SOURCES tests/ldk_test.h tests/test_maths.c)
    create_test(TARGET test_arena SOURCES tests/ldk_test.h tests/test_arena.c)
    create_test(TARGET test_array SOURCES tests/ldk_test.h tests/test_array.c)
    create_test(TARGET test_hlist SOURCES tests/ldk_test.h tests/test_hlist.c)
    create_test(TARGET test_mat4 SOURCES tests/ldk_test.h tests/test_mat4.c)
    create_test(TARGET test_typesystem SOURCES tests/ldk_test.h tests/test_typesystem.c)

  endif()
