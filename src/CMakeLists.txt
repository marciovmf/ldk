cmake_minimum_required(VERSION 3.9)
project("ldk-core")
find_package(OpenGL REQUIRED)

# --- GLOBAL DEFINES/OPTIONS----------------------
option(OPTION_LDKSHAREDLIB "Set to build engine as a shared library" OFF)
option(OPTION_BUILD_EDITOR "Set to build the editor" OFF)
option(OPTION_BUILD_TESTS "Set to built and run tests" OFF)

add_custom_target(print_options ALL
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "CONFIG                    $<CONFIG>"
  COMMAND ${CMAKE_COMMAND} -E echo "OUTPUT_DIR                ${LDK_RUNTREE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_ADDRESS_SANITIZER  ${OPTION_ADDRESS_SANITIZER}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_LDKSHAREDLIB       ${OPTION_LDKSHAREDLIB}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_EDITOR       ${OPTION_BUILD_EDITOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_TESTS        ${OPTION_BUILD_TESTS}"
  COMMAND ${CMAKE_COMMAND} -E echo ""
  )

# --- SOURCES ------------------------------------

set(INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

list(APPEND SOURCE
  # Core
  ${INCLUDE_DIR}/ldk_os.h
  ${INCLUDE_DIR}/ldk_event.h 
  ${INCLUDE_DIR}/ldk_geom.h 
  ${INCLUDE_DIR}/ldk_color.h 
  ${INCLUDE_DIR}/ldk_game.h
  ${INCLUDE_DIR}/ldk.h            ldk.c
  ${INCLUDE_DIR}/ldk_common.h     ldk_common.c
  ${INCLUDE_DIR}/ldk_argparse.h   ldk_argparse.c
  # modules
  ${INCLUDE_DIR}/ldk_component.h  module/ldk_component.c
  ${INCLUDE_DIR}/ldk_entity.h     module/ldk_entity.c
  ${INCLUDE_DIR}/ldk_eventqueue.h module/ldk_eventqueue.c
  ${INCLUDE_DIR}/ldk_system.h     module/ldk_system.c
  # systems
  ${INCLUDE_DIR}/ldk_system_scenegraph.h system/ldk_system_scenegraph.c
  # components
  ${INCLUDE_DIR}/ldk_component_transform.h component/ldk_component_transform.c
  ${INCLUDE_DIR}/ldk_gl.h         ldk_gl.c
  )

  # Use this list when building games

  #if (OPTION_BUILD_EDITOR)
  #  #list(APPEND SOURCE ${INCLUDE_DIR}/module/editor.h module/editor.c)
  #endif()

  if(WIN32)
    # Windows specific sources and libs
    list(APPEND SOURCE ldk_os_win32.c)
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

      target_compile_definitions(${ARG_TARGET} PRIVATE 
      $<$<BOOL:${OPTION_LDKSHAREDLIB}>:LDK_SHAREDLIB>)
      set_target_properties(${ARG_TARGET} PROPERTIES OUTPUT_NAME ${ARG_TARGET}${OUTPUT_NAME_SUFFIX})
      target_include_directories(${ARG_TARGET} PUBLIC ${INCLUDE_DIR})
      target_link_libraries(${ARG_TARGET} PRIVATE ${ARG_LIBRARIES})
      add_dependencies(${ARG_TARGET} ldk)

      add_custom_target(test_${ARG_TARGET} ALL
        COMMAND $<TARGET_FILE:${ARG_TARGET}>
        COMMENT "Testing $<TARGET_FILE:${ARG_TARGET}>"
        DEPENDS ${ARG_TARGET} ldk)
    endfunction()

    create_test(TARGET test_module_entity SOURCES tests/test_ldk_entity.c)
    create_test(TARGET test_module_component SOURCES tests/test_ldk_component.c)
    create_test(TARGET test_module_system SOURCES tests/test_ldk_system.c)
    create_test(TARGET test_module_transform SOURCES tests/test_ldk_transform.c)

  endif()
