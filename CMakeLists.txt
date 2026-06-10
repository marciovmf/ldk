cmake_minimum_required(VERSION 4.3)

project("ldk")

find_package(OpenGL REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- GLOBAL OPTIONS ---------------------------------------------------------
option(OPTION_ADDRESS_SANITIZER           "Enable address sanitizer" OFF)
option(OPTION_BUILD_TESTS                 "Build and run tests" OFF)
option(OPTION_BUILD_EDITOR                "Build the editor executable" OFF)
option(OPTION_BUILD_GAME                  "Build the game as a DLL. Requires OPTION_GAME_DIR to be set." OFF)
option(OPTION_BUILD_GAME_LAUNCHER         "Build the game launcher executable. Requires OPTION_GAME_DIR to be set." OFF)
set(OPTION_GAME_DIR                    "" CACHE PATH "Game project root folder")
option(OPTION_LDK_USE_PREBUILT            "Use prebuilt LDK instead of building engine" OFF)
set(OPTION_LDK_PREBUILT_DIR            "" CACHE PATH "Prebuilt LDK root folder")

if (OPTION_ADDRESS_SANITIZER)
  add_compile_options(-fsanitize=address)
endif()

# --- GLOBAL OUTPUT DIRECTORIES ---------------------------------------------
set(LDK_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(LDK_OUTPUT_DIR "${LDK_ROOT_DIR}/bin/$<CONFIG>/")
set(LDK_LIBRARY_DIR "${LDK_ROOT_DIR}/lib/$<CONFIG>/")
set(LDK_RUNTREE_DIR "${LDK_ROOT_DIR}/runtree")
set(INCLUDE_DIR "${LDK_ROOT_DIR}/include")

# --- PRINT OPTIONS ----------------------------------------------------------
add_custom_target(print_options ALL
  COMMAND ${CMAKE_COMMAND} -E echo ""
  COMMAND ${CMAKE_COMMAND} -E echo "CONFIG                           $<CONFIG>"
  COMMAND ${CMAKE_COMMAND} -E echo "LDK_OUTPUT_DIR                   ${LDK_OUTPUT_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "LDK_LIBRARY_DIR                  ${LDK_LIBRARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "LDK_RUNTREE_DIR                  ${LDK_RUNTREE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_ADDRESS_SANITIZER         ${OPTION_ADDRESS_SANITIZER}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_EDITOR              ${OPTION_BUILD_EDITOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_TESTS               ${OPTION_BUILD_TESTS}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_GAME                ${OPTION_BUILD_GAME}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_BUILD_GAME_LAUNCHER       ${OPTION_BUILD_GAME_LAUNCHER}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_GAME_DIR                  ${OPTION_GAME_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_LDK_USE_PREBUILT          ${OPTION_LDK_USE_PREBUILT}"
  COMMAND ${CMAKE_COMMAND} -E echo "OPTION_LDK_PREBUILT_DIR          ${OPTION_LDK_PREBUILT_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo ""
)

# --- TARGET HELPERS ---------------------------------------------------------
function(ldk_target_defaults TARGET_NAME)

  add_dependencies(${TARGET_NAME} print_options)

  target_compile_definitions(${TARGET_NAME} PRIVATE
    $<$<CONFIG:Release>:${Release_DEFINITIONS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_DEFINITIONS}>
    $<$<CONFIG:Debug>:${Debug_DEFINITIONS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_DEFINITIONS}>
    LDK_BUILD_TYPE="$<CONFIG>"
  )

  target_compile_options(${TARGET_NAME} PRIVATE
    $<$<CONFIG:Release>:${Release_COMPILE_OPTIONS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_COMPILE_OPTIONS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_COMPILE_OPTIONS}>
    $<$<CONFIG:Debug>:${Debug_COMPILE_OPTIONS}>
  )

  target_link_libraries(${TARGET_NAME} PRIVATE
    $<$<CONFIG:Release>:${Release_LIBS}>
    $<$<CONFIG:RelWithDebInfo>:${RelWithDebInfo_LIBS}>
    $<$<CONFIG:MinSizeRel>:${MinSizeRel_LIBS}>
    $<$<CONFIG:Debug>:${Debug_LIBS}>
  )
endfunction()

function(ldk_target_output_dirs TARGET_NAME)
  set(options)
  set(oneValueArgs RUNTIME LIBRARY ARCHIVE)
  set(multiValueArgs)

  cmake_parse_arguments(
    OUT
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  if (OUT_RUNTIME)
    set_target_properties(${TARGET_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${OUT_RUNTIME}"
    )
  endif()

  if (OUT_LIBRARY)
    set_target_properties(${TARGET_NAME} PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY "${OUT_LIBRARY}"
    )
  endif()

  if (OUT_ARCHIVE)
    set_target_properties(${TARGET_NAME} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${OUT_ARCHIVE}"
    )
  endif()
endfunction()

function(ldk_engine_target_defaults TARGET_NAME)
  ldk_target_defaults(${TARGET_NAME})

  target_compile_definitions(${TARGET_NAME} PRIVATE
    LDK_ENGINE
  )

  target_include_directories(${TARGET_NAME} PUBLIC
    ${INCLUDE_DIR}
  )

  target_link_libraries(${TARGET_NAME} PRIVATE
    ${LIBS}
    ${OPENGL_LIBRARY}
  )
endfunction()

set(HEADER_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated"
)

# ----------------------------------------------------------------------------
# Collect full path of all .h files in the folder specified by DIR.
# ----------------------------------------------------------------------------
function(ldk_collect_headers_from_dir OUT_VAR DIR)
  if(NOT IS_ABSOLUTE "${DIR}")
    get_filename_component(DIR "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}" ABSOLUTE)
  endif()

  file(GLOB headers_in_dir CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${DIR}/*.h"
  )

  set(${OUT_VAR} "${headers_in_dir}" PARENT_SCOPE)
endfunction()


function(ldk_game_build)
  set(options)
  set(oneValueArgs TARGET TYPE ENGINE_TARGET OUTPUT_DIR)
  set(multiValueArgs
    SOURCES
    INCLUDE_DIRS
    DEFINITIONS
    LIBRARIES
    COMPONENT_DIRS
  )
  
  cmake_parse_arguments(
    GAME
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )

  if (NOT GAME_TARGET)
    message(FATAL_ERROR "ldk_game_build requires TARGET")
  endif()

  if (NOT GAME_TYPE)
    message(FATAL_ERROR "ldk_game_build requires TYPE")
  endif()

  if (NOT GAME_ENGINE_TARGET)
    message(FATAL_ERROR "ldk_game_build requires ENGINE_TARGET")
  endif()

  if (NOT GAME_SOURCES)
    message(FATAL_ERROR "ldk_game_build requires SOURCES")
  endif()

  if (NOT TARGET ${GAME_ENGINE_TARGET})
    message(FATAL_ERROR "ldk_game_build: ENGINE_TARGET '${GAME_ENGINE_TARGET}' does not exist")
  endif()

  add_library(${GAME_TARGET} ${GAME_TYPE}
    ${GAME_SOURCES}
  )

  if ("${GAME_TYPE}" STREQUAL "SHARED")
    add_dependencies(${GAME_TARGET} comet) # make sure the tools are built
  endif()

  ldk_target_defaults(${GAME_TARGET})

  if (GAME_OUTPUT_DIR)
    set(GAME_TARGET_OUTPUT_DIR "${GAME_OUTPUT_DIR}")
  else()
    set(GAME_TARGET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${GAME_TARGET}")
  endif()

  set(GAME_METADATA_DIR "${GAME_TARGET_OUTPUT_DIR}/generated")
  set(GAME_METADATA_HEADER "${GAME_METADATA_DIR}/generated_component_metadata.h")

  # ------------------------------------------------------------
  # Component metadata generation
  # ------------------------------------------------------------

  if (GAME_COMPONENT_DIRS)
    set(GAME_COMPONENT_HEADERS "")
    
    foreach(component_dir IN LISTS GAME_COMPONENT_DIRS)
      ldk_collect_headers_from_dir(headers_in_dir "${component_dir}")
      list(APPEND GAME_COMPONENT_HEADERS ${headers_in_dir})
    endforeach()

    list(REMOVE_DUPLICATES GAME_COMPONENT_HEADERS)

    add_custom_command(
      OUTPUT "${GAME_METADATA_HEADER}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_METADATA_DIR}"
      COMMAND $<TARGET_FILE:comet> "${GAME_METADATA_HEADER}" ${GAME_COMPONENT_HEADERS}
      DEPENDS
        comet
        ${GAME_COMPONENT_HEADERS}
      COMMAND_EXPAND_LISTS
      VERBATIM
      COMMENT "Running component metadata extraction tool..."
    )

    add_custom_target(${GAME_TARGET}_metadata
      DEPENDS "${GAME_METADATA_HEADER}"
    )

    add_dependencies(${GAME_TARGET}
      ${GAME_TARGET}_metadata
    )
  endif()

  # ------------------------------------------------------------
  # Normal game target setup
  # ------------------------------------------------------------

  target_include_directories(${GAME_TARGET} PRIVATE
    "${INCLUDE_DIR}"
    "${GAME_METADATA_DIR}"
    ${GAME_INCLUDE_DIRS}
  )

  target_compile_definitions(${GAME_TARGET} PRIVATE
    ${GAME_DEFINITIONS}
  )

  target_link_libraries(${GAME_TARGET} PRIVATE
    ${GAME_ENGINE_TARGET}
    ${GAME_LIBRARIES}
  )

  if ("${GAME_TYPE}" STREQUAL "SHARED")
    target_compile_definitions(${GAME_TARGET} PRIVATE
      LDK_SHAREDLIB
    )
  endif()

  if (GAME_OUTPUT_DIR)
    ldk_target_output_dirs(${GAME_TARGET}
      RUNTIME "${GAME_OUTPUT_DIR}"
      LIBRARY "${GAME_OUTPUT_DIR}"
      ARCHIVE "${GAME_OUTPUT_DIR}"
    )
  endif()
endfunction()



function(ldk_test_build)
  set(options "")
  set(oneValueArgs TARGET)
  set(multiValueArgs SOURCES LIBRARIES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT ARG_TARGET)
    message(FATAL_ERROR "TARGET parameter is required for ldk_test_build function.")
    return()
  endif()

  if (NOT ARG_SOURCES)
    message(FATAL_ERROR "SOURCES parameter is required for ldk_test_build function.")
    return()
  endif()

  if (NOT ARG_LIBRARIES)
    set(ARG_LIBRARIES ldk)
  else()
    list(APPEND ARG_LIBRARIES ldk)
  endif()

  add_executable(${ARG_TARGET} ${ARG_SOURCES})
  target_compile_definitions(${ARG_TARGET} PRIVATE LDK_SHAREDLIB)
  set_target_properties(${ARG_TARGET} PROPERTIES OUTPUT_NAME ${ARG_TARGET}${OUTPUT_NAME_SUFFIX})
  target_include_directories(${ARG_TARGET} PUBLIC ${INCLUDE_DIR})
  target_link_libraries(${ARG_TARGET} PRIVATE ${ARG_LIBRARIES})
  add_dependencies(${ARG_TARGET} ldk print_options)

  add_custom_target(test_${ARG_TARGET} ALL
    COMMAND $<TARGET_FILE:${ARG_TARGET}>
    COMMENT "Testing $<TARGET_FILE:${ARG_TARGET}>"
    DEPENDS ${ARG_TARGET} ldk
  )

  ldk_target_output_dirs(${ARG_TARGET}
    RUNTIME "${LDK_OUTPUT_DIR}"
    LIBRARY "${LDK_OUTPUT_DIR}"
    ARCHIVE "${LDK_OUTPUT_DIR}"
  )
endfunction()

# --- SOURCES ----------------------------------------------------------------
list(APPEND LDK_SOURCE
  # GL specific
  src/ldk_gl.h src/ldk_gl.c
  src/ldk_rhi_gl33.h src/ldk_rhi_gl33.c

  # Core
  ${INCLUDE_DIR}/ldk.h              src/ldk.c
  ${INCLUDE_DIR}/ldk_os.h
  ${INCLUDE_DIR}/ldk_event.h
  ${INCLUDE_DIR}/ldk_color.h
  ${INCLUDE_DIR}/ldk_game.h
  ${INCLUDE_DIR}/ldk_common.h
  ${INCLUDE_DIR}/ldk_mesh.h
  ${INCLUDE_DIR}/ldk_resource.h
  ${INCLUDE_DIR}/ldk_geom.h         src/ldk_geom.c
  ${INCLUDE_DIR}/ldk_argparse.h     src/ldk_argparse.c
  ${INCLUDE_DIR}/ldk_ttf.h          src/ldk_ttf.c
  ${INCLUDE_DIR}/ldk_image.h        src/ldk_image.c
  ${INCLUDE_DIR}/ldk_project.h      src/ldk_project.c

  # Modules
  ${INCLUDE_DIR}/module/ldk_asset_manager.h   src/module/ldk_asset_manager.c
  ${INCLUDE_DIR}/module/ldk_component.h       src/module/ldk_component.c
  ${INCLUDE_DIR}/module/ldk_entity.h          src/module/ldk_entity.c
  ${INCLUDE_DIR}/module/ldk_eventqueue.h      src/module/ldk_eventqueue.c
  ${INCLUDE_DIR}/module/ldk_ecs.h             src/module/ldk_ecs.c
  ${INCLUDE_DIR}/module/ldk_renderer.h        src/module/ldk_renderer.c
  ${INCLUDE_DIR}/module/ldk_rhi.h             src/module/ldk_rhi.c
  ${INCLUDE_DIR}/module/ldk_system.h          src/module/ldk_system.c
  ${INCLUDE_DIR}/module/ldk_ui.h              src/module/ldk_ui.c
  ${INCLUDE_DIR}/module/ldk_scenegraph.h      src/module/ldk_scenegraph.c

  # Components
  ${INCLUDE_DIR}/component/ldk_transform.h    src/component/ldk_transform.c
  ${INCLUDE_DIR}/component/ldk_camera.h       src/component/ldk_camera.c
  ${INCLUDE_DIR}/component/ldk_mesh_source.h  src/component/ldk_mesh_source.c
)

if (WIN32)
  list(APPEND LDK_SOURCE src/ldk_os_win32.c)
  set(LIBS dbghelp.lib)

  list(APPEND Release_LIBS ${LIBS})
  list(APPEND Release_DEFINITIONS)
  list(APPEND Release_COMPILE_OPTIONS /O2)

  list(APPEND RelWithDebInfo_LIBS ${LIBS})
  list(APPEND RelWithDebInfo_DEFINITIONS ${Release_DEFINITIONS} LDK_DEBUG)
  list(APPEND RelWithDebInfo_COMPILE_OPTIONS ${Release_COMPILE_OPTIONS})

  list(APPEND MinSizeRel_LIBS ${LIBS})
  list(APPEND MinSizeRel_DEFINITIONS ${Release_DEFINITIONS} LDK_DEBUG)
  list(APPEND MinSizeRel_COMPILE_OPTIONS ${Release_COMPILE_OPTIONS})

  list(APPEND Debug_LIBS ${LIBS})
  list(APPEND Debug_DEFINITIONS LDK_DEBUG)
  list(APPEND Debug_COMPILE_OPTIONS /Zi /DEBUG)
else()
  message(FATAL_ERROR "UNSUPPORTED PLATFORM")
endif()

# --- ENGINE SHARED / IMPORTED -----------------------------------------------
if (OPTION_LDK_USE_PREBUILT)
  if (NOT OPTION_LDK_PREBUILT_DIR)
    message(FATAL_ERROR "OPTION_LDK_USE_PREBUILT requires OPTION_LDK_PREBUILT_DIR")
  endif()

  add_library(ldk SHARED IMPORTED GLOBAL)

  set_target_properties(ldk PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPTION_LDK_PREBUILT_DIR}/include"

    IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"

    IMPORTED_IMPLIB_DEBUG "${OPTION_LDK_PREBUILT_DIR}/bin/Debug/lib/ldk.lib"
    IMPORTED_LOCATION_DEBUG "${OPTION_LDK_PREBUILT_DIR}/bin/Debug/ldk.dll"

    IMPORTED_IMPLIB_RELEASE "${OPTION_LDK_PREBUILT_DIR}/bin/Release/lib/ldk.lib"
    IMPORTED_LOCATION_RELEASE "${OPTION_LDK_PREBUILT_DIR}/bin/Release/ldk.dll"

    IMPORTED_IMPLIB_RELWITHDEBINFO "${OPTION_LDK_PREBUILT_DIR}/bin/RelWithDebInfo/lib/ldk.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${OPTION_LDK_PREBUILT_DIR}/bin/RelWithDebInfo/ldk.dll"

    IMPORTED_IMPLIB_MINSIZEREL "${OPTION_LDK_PREBUILT_DIR}/bin/MinSizeRel/lib/ldk.lib"
    IMPORTED_LOCATION_MINSIZEREL "${OPTION_LDK_PREBUILT_DIR}/bin/MinSizeRel/ldk.dll"
  )
else()
  add_library(ldk SHARED ${LDK_SOURCE})
  ldk_engine_target_defaults(ldk)

  target_compile_definitions(ldk PRIVATE
    LDK_SHAREDLIB
    LDK_EDITOR
  )

  ldk_target_output_dirs(ldk
    RUNTIME "${LDK_OUTPUT_DIR}"
    LIBRARY "${LDK_OUTPUT_DIR}"
    ARCHIVE "${LDK_LIBRARY_DIR}"
  )

  add_dependencies(ldk print_options)
endif()

# --- COMET TOOL ---------------------------------------------------------------

# component meta generator tool
add_executable(comet src/tools/ldk_tool_comet.c)
ldk_target_defaults(comet)
target_include_directories(comet PRIVATE ${INCLUDE_DIR})
ldk_target_output_dirs(comet
  RUNTIME "${LDK_OUTPUT_DIR}"
  LIBRARY "${LDK_OUTPUT_DIR}"
  ARCHIVE "${LDK_LIBRARY_DIR}"
)

# --- EDITOR -----------------------------------------------------------------
if (OPTION_BUILD_EDITOR)
  if (OPTION_LDK_USE_PREBUILT)
    message(FATAL_ERROR "OPTION_BUILD_EDITOR cannot use OPTION_LDK_USE_PREBUILT. Build the editor with the engine sources.")
  endif()

  add_executable(ldk_editor src/ldk_app_editor.c)
  add_dependencies(ldk_editor comet) # make sure the tools are built
  ldk_target_defaults(ldk_editor)

  target_include_directories(ldk_editor PRIVATE
    ${INCLUDE_DIR}
  )

  target_compile_definitions(ldk_editor PRIVATE
    LDK_SHAREDLIB
    LDK_EDITOR
  )

  target_link_libraries(ldk_editor PRIVATE
    ldk
  )

  ldk_target_output_dirs(ldk_editor
    RUNTIME "${LDK_OUTPUT_DIR}"
    LIBRARY "${LDK_OUTPUT_DIR}"
    ARCHIVE "${LDK_LIBRARY_DIR}"
  )
endif()

# --- GAME CONFIG ------------------------------------------------------------
if (OPTION_BUILD_GAME OR OPTION_BUILD_GAME_LAUNCHER)
  if (NOT OPTION_GAME_DIR)
    message(FATAL_ERROR "Game build requires OPTION_GAME_DIR to be set.")
  endif()

  set(LDK_GAME_CMAKE "${OPTION_GAME_DIR}/ldk_game.cmake")

  if (NOT EXISTS "${LDK_GAME_CMAKE}")
    message(FATAL_ERROR "Game config not found: ${LDK_GAME_CMAKE}")
  endif()

  include("${LDK_GAME_CMAKE}")
endif()

# --- GAME MODULE ------------------------------------------------------------
if (OPTION_BUILD_GAME)
  if (OPTION_LDK_USE_PREBUILT)
    set(LDK_GAME_TARGET_NAME game)
  else()
    set(LDK_GAME_TARGET_NAME game_shared)
  endif()

  list(APPEND LDK_GAME_COMPONENT_DIRS
  "${INCLUDE_DIR}/component")

  # Includes game metadata source
  list(APPEND LDK_GAME_SOURCES )
  
  ldk_game_build(
    TARGET ${LDK_GAME_TARGET_NAME}
    TYPE SHARED
    ENGINE_TARGET ldk
    SOURCES ${LDK_GAME_SOURCES} "src/ldk_game_metadata.c"
    INCLUDE_DIRS ${LDK_GAME_INCLUDE_DIRS}
    COMPONENT_DIRS ${ENGINE_COMPONENT_DIR} ${LDK_GAME_COMPONENT_DIRS}
    DEFINITIONS
    ${LDK_GAME_DEFINITIONS}
    LDK_GAME_DLL
    LIBRARIES ${LDK_GAME_LIBRARIES}
    OUTPUT_DIR "${OPTION_GAME_DIR}/.ldk/$<CONFIG>"
  )
endif()

# --- GAME LAUNCHER ----------------------------------------------------------
if (OPTION_BUILD_GAME_LAUNCHER)
  if (OPTION_LDK_USE_PREBUILT)
    message(FATAL_ERROR "OPTION_BUILD_GAME_LAUNCHER cannot use OPTION_LDK_USE_PREBUILT. The launcher needs a static engine build.")
  endif()

  add_library(ldk_monolithic STATIC ${LDK_SOURCE})
  add_dependencies(ldk_monolithic print_options)
  ldk_engine_target_defaults(ldk_monolithic)
  target_compile_definitions(ldk_monolithic PRIVATE LDK_MONOLITHIC)

  ldk_target_output_dirs(ldk_monolithic
    RUNTIME "${OPTION_GAME_DIR}/bin/$<CONFIG>"
    LIBRARY "${OPTION_GAME_DIR}/bin/$<CONFIG>"
    ARCHIVE "${OPTION_GAME_DIR}/bin/$<CONFIG>"
  )

  ldk_game_build(
    TARGET game_static
    TYPE STATIC
    ENGINE_TARGET ldk_monolithic
    SOURCES ${LDK_GAME_SOURCES}
    INCLUDE_DIRS ${LDK_GAME_INCLUDE_DIRS}
    DEFINITIONS
    ${LDK_GAME_DEFINITIONS}
    LDK_MONOLITHIC
    LIBRARIES ${LDK_GAME_LIBRARIES}
    OUTPUT_DIR "${OPTION_GAME_DIR}/bin/$<CONFIG>"
  )

  add_executable(ldk_launcher src/ldk_app_launcher.c)
  ldk_target_defaults(ldk_launcher)

  target_include_directories(ldk_launcher PRIVATE
    ${INCLUDE_DIR}
  )

  target_compile_definitions(ldk_launcher PRIVATE
    LDK_MONOLITHIC
  )

  target_link_libraries(ldk_launcher PRIVATE
    ldk_monolithic
    game_static
  )

  ldk_target_output_dirs(ldk_launcher
    RUNTIME "${OPTION_GAME_DIR}/bin/$<CONFIG>"
    LIBRARY "${OPTION_GAME_DIR}/bin/$<CONFIG>"
    ARCHIVE "${OPTION_GAME_DIR}/bin/$<CONFIG>"
  )
endif()

# --- TESTS ------------------------------------------------------------------
if (OPTION_BUILD_TESTS AND TARGET ldk)
  if (OPTION_LDK_USE_PREBUILT)
    message(FATAL_ERROR "OPTION_BUILD_TESTS cannot use OPTION_LDK_USE_PREBUILT. Tests require building the engine target.")
  endif()

  ldk_test_build(TARGET test_module_entity SOURCES src/tests/test_ldk_entity.c)
  ldk_test_build(TARGET test_module_component SOURCES src/tests/test_ldk_component.c)
  ldk_test_build(TARGET test_module_system SOURCES src/tests/test_ldk_system.c)
  ldk_test_build(TARGET test_module_transform SOURCES src/tests/test_ldk_transform.c)
  ldk_test_build(TARGET test_module_rhi SOURCES src/tests/test_ldk_rhi.c)
endif()
