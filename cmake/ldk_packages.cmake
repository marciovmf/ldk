function(ldk_game_project_file_resolve OUT_FILE)
  if (OPTION_GAME_PROJECT_FILE)
    get_filename_component(_project_file "${OPTION_GAME_PROJECT_FILE}" ABSOLUTE)
    if (NOT EXISTS "${_project_file}")
      message(FATAL_ERROR "Game project file not found: ${_project_file}")
    endif()
  else()
    file(GLOB _project_files CONFIGURE_DEPENDS
      LIST_DIRECTORIES false
      "${OPTION_GAME_DIR}/*.ldk"
    )
    list(LENGTH _project_files _project_file_count)
    if (NOT _project_file_count EQUAL 1)
      message(FATAL_ERROR
        "Expected exactly one .ldk project file in '${OPTION_GAME_DIR}', "
        "found ${_project_file_count}. Set OPTION_GAME_PROJECT_FILE explicitly."
      )
    endif()
    list(GET _project_files 0 _project_file)
  endif()

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_project_file}"
  )
  set(${OUT_FILE} "${_project_file}" PARENT_SCOPE)
endfunction()

function(ldk_project_boxes_read PROJECT_FILE OUT_NAMES OUT_RULES)
  if (NOT EXISTS "${PROJECT_FILE}")
    message(FATAL_ERROR "Project file not found: ${PROJECT_FILE}")
  endif()

  file(STRINGS "${PROJECT_FILE}" _lines ENCODING UTF-8)

  set(_in_box FALSE)
  set(_names "")
  set(_rules "")

  foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _line)

    if (_line MATCHES "^\\[([^]]+)\\]$")
      string(STRIP "${CMAKE_MATCH_1}" _section)
      if (_section STREQUAL ".box")
        set(_in_box TRUE)
      else()
        set(_in_box FALSE)
      endif()
      continue()
    endif()

    if (NOT _in_box OR _line STREQUAL "" OR
        _line MATCHES "^[#;]")
      continue()
    endif()

    if (NOT _line MATCHES "^([^=]+)=(.*)$")
      message(FATAL_ERROR
        "Invalid [.box] entry in '${PROJECT_FILE}': ${_line}"
      )
    endif()

    string(STRIP "${CMAKE_MATCH_1}" _name)
    string(STRIP "${CMAKE_MATCH_2}" _value)

    if (_value MATCHES "^\"(.*)\"$")
      set(_value "${CMAKE_MATCH_1}")
    endif()

    if (_name STREQUAL "")
      message(FATAL_ERROR "Empty package name in '${PROJECT_FILE}'.")
    endif()

    if (_value MATCHES ";")
      message(FATAL_ERROR
        "Package rules may not contain ';'. Use '|' to separate rules."
      )
    endif()

    list(FIND _names "${_name}" _duplicate_index)
    if (NOT _duplicate_index EQUAL -1)
      message(FATAL_ERROR "Duplicate package '${_name}' in '${PROJECT_FILE}'.")
    endif()

    list(APPEND _names "${_name}")
    list(APPEND _rules "=${_value}")
  endforeach()

  set(${OUT_NAMES} "${_names}" PARENT_SCOPE)
  set(${OUT_RULES} "${_rules}" PARENT_SCOPE)
endfunction()

function(ldk_project_box_imports_read PROJECT_FILE ROOT_DIR OUT_PATHS OUT_COPY)
  if (NOT EXISTS "${PROJECT_FILE}")
    message(FATAL_ERROR "Project file not found: ${PROJECT_FILE}")
  endif()

  file(STRINGS "${PROJECT_FILE}" _lines ENCODING UTF-8)

  set(_in_box_imports FALSE)
  set(_paths "")
  set(_copy "")

  foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _line)

    if (_line MATCHES "^\\[([^]]+)\\]$")
      string(STRIP "${CMAKE_MATCH_1}" _section)
      if (_section STREQUAL ".box_imports")
        set(_in_box_imports TRUE)
      else()
        set(_in_box_imports FALSE)
      endif()
      continue()
    endif()

    if (NOT _in_box_imports OR _line STREQUAL "" OR _line MATCHES "^[#;]")
      continue()
    endif()

    if (NOT _line MATCHES "^([^=]+)=(.*)$")
      message(FATAL_ERROR
        "Invalid [.box_imports] entry in '${PROJECT_FILE}': ${_line}"
      )
    endif()

    string(STRIP "${CMAKE_MATCH_1}" _path)
    string(STRIP "${CMAKE_MATCH_2}" _value)

    if (_value MATCHES "^\"(.*)\"$")
      set(_value "${CMAKE_MATCH_1}")
    endif()

    if (_path STREQUAL "")
      message(FATAL_ERROR "Empty import path in '${PROJECT_FILE}'.")
    endif()
    if (_path MATCHES "(^|[/\\])\.\.([/\\]|$)")
      message(FATAL_ERROR
        "Import path may not contain '..' in '${PROJECT_FILE}': ${_path}"
      )
    endif()
    if (IS_ABSOLUTE "${_path}")
      message(FATAL_ERROR
        "Import paths must be relative in '${PROJECT_FILE}': ${_path}"
      )
    endif()
    if (NOT _value STREQUAL "0" AND NOT _value STREQUAL "1")
      message(FATAL_ERROR
        "Import '${_path}' in '${PROJECT_FILE}' must be 0 or 1."
      )
    endif()

    if (_path MATCHES "^@(.*)$")
      set(_relative "${CMAKE_MATCH_1}")
      if (_relative STREQUAL "" OR IS_ABSOLUTE "${_relative}")
        message(FATAL_ERROR "Invalid engine import path in '${PROJECT_FILE}': ${_path}")
      endif()
      get_filename_component(_resolved
        "${LDK_RUNTREE_DIR}/${_relative}" ABSOLUTE)
    else()
      get_filename_component(_resolved "${ROOT_DIR}/${_path}" ABSOLUTE)
    endif()

    get_filename_component(_extension "${_resolved}" LAST_EXT)
    string(TOLOWER "${_extension}" _extension)
    if (NOT _extension STREQUAL ".box")
      message(FATAL_ERROR "Imported package must be a .box file: ${_path}")
    endif()

    if (NOT EXISTS "${_resolved}" OR IS_DIRECTORY "${_resolved}")
      message(FATAL_ERROR
        "Imported package not found: ${_resolved}"
      )
    endif()

    list(FIND _paths "${_resolved}" _duplicate_index)
    if (NOT _duplicate_index EQUAL -1)
      message(FATAL_ERROR
        "Duplicate import '${_path}' in '${PROJECT_FILE}'."
      )
    endif()

    list(APPEND _paths "${_resolved}")
    list(APPEND _copy "${_value}")
  endforeach()

  set(${OUT_PATHS} "${_paths}" PARENT_SCOPE)
  set(${OUT_COPY} "${_copy}" PARENT_SCOPE)
endfunction()

function(ldk_game_packages_target_add
    TARGET_NAME ROOT_DIR OUTPUT_DIR PROJECT_FILE)
  ldk_project_boxes_read("${PROJECT_FILE}" _package_names _package_rules)
  ldk_project_box_imports_read(
    "${PROJECT_FILE}" "${ROOT_DIR}" _import_paths _import_copy)

  list(LENGTH _package_names _package_count)
  list(LENGTH _import_paths _import_count)

  set(_box_args "")
  set(_manifest_args "")
  set(_output_names "")
  foreach(_package_name IN LISTS _package_names)
    string(TOLOWER "${_package_name}" _output_name)
    list(APPEND _output_names "${_output_name}")
    list(APPEND _manifest_args --package "${OUTPUT_DIR}/${_package_name}")
  endforeach()

  if (_package_count GREATER 0)
    math(EXPR _package_last "${_package_count} - 1")
    foreach(_index RANGE 0 ${_package_last})
      list(GET _package_names ${_index} _package_name)
      list(GET _package_rules ${_index} _package_rule_encoded)
      string(SUBSTRING "${_package_rule_encoded}" 1 -1 _package_rule)
      list(APPEND _box_args
        --package "${_package_name}" "${_package_rule}"
      )
    endforeach()
  endif()

  set(_copy_commands "")
  set(_copy_count 0)
  if (_import_count GREATER 0)
    math(EXPR _import_last "${_import_count} - 1")
    foreach(_index RANGE 0 ${_import_last})
      list(GET _import_paths ${_index} _import_path)
      list(GET _import_copy ${_index} _copy)
      if (NOT _copy STREQUAL "1")
        continue()
      endif()

      get_filename_component(_import_name "${_import_path}" NAME)
      string(TOLOWER "${_import_name}" _output_name)
      list(FIND _output_names "${_output_name}" _collision_index)
      if (NOT _collision_index EQUAL -1)
        message(FATAL_ERROR
          "Package output collision for '${_import_name}' in '${PROJECT_FILE}'."
        )
      endif()

      list(APPEND _output_names "${_output_name}")
      list(APPEND _copy_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_import_path}" "${OUTPUT_DIR}/${_import_name}"
      )
      list(APPEND _manifest_args --package "${OUTPUT_DIR}/${_import_name}")
      math(EXPR _copy_count "${_copy_count} + 1")
    endforeach()
  endif()

  if (_package_count EQUAL 0 AND _copy_count EQUAL 0)
    return()
  endif()

  if (_package_count GREATER 0)
    add_custom_target(${TARGET_NAME}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
      COMMAND $<TARGET_FILE:ldk_box>
        pack
        --root "${ROOT_DIR}"
        --output "${OUTPUT_DIR}"
        ${_box_args}
      ${_copy_commands}
      COMMAND $<TARGET_FILE:ldk_box>
        manifest
        --input "${ROOT_DIR}/game.ini"
        ${_manifest_args}
      COMMAND_EXPAND_LISTS
      VERBATIM
      COMMENT "Building game .box packages"
    )
    add_dependencies(${TARGET_NAME} ldk_box)
  else()
    add_custom_target(${TARGET_NAME}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
      ${_copy_commands}
      COMMAND $<TARGET_FILE:ldk_box>
        manifest
        --input "${ROOT_DIR}/game.ini"
        ${_manifest_args}
      COMMAND_EXPAND_LISTS
      VERBATIM
      COMMENT "Copying imported game .box packages"
    )
    add_dependencies(${TARGET_NAME} ldk_box)
  endif()
endfunction()
