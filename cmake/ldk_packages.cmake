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

function(ldk_project_packages_read PROJECT_FILE OUT_NAMES OUT_RULES)
  if (NOT EXISTS "${PROJECT_FILE}")
    message(FATAL_ERROR "Project file not found: ${PROJECT_FILE}")
  endif()

  file(STRINGS "${PROJECT_FILE}" _lines ENCODING UTF-8)

  set(_in_packages FALSE)
  set(_names "")
  set(_rules "")

  foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _line)

    if (_line MATCHES "^\\[([^]]+)\\]$")
      string(STRIP "${CMAKE_MATCH_1}" _section)
      if (_section STREQUAL ".packages")
        set(_in_packages TRUE)
      else()
        set(_in_packages FALSE)
      endif()
      continue()
    endif()

    if (NOT _in_packages OR _line STREQUAL "" OR
        _line MATCHES "^[#;]")
      continue()
    endif()

    if (NOT _line MATCHES "^([^=]+)=(.*)$")
      message(FATAL_ERROR
        "Invalid [.packages] entry in '${PROJECT_FILE}': ${_line}"
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

function(ldk_game_packages_target_add
    TARGET_NAME ROOT_DIR OUTPUT_DIR PROJECT_FILE)
  ldk_project_packages_read("${PROJECT_FILE}" _package_names _package_rules)
  list(LENGTH _package_names _package_count)

  if (_package_count EQUAL 0)
    return()
  endif()

  set(_box_args "")
  math(EXPR _package_last "${_package_count} - 1")
  foreach(_index RANGE 0 ${_package_last})
    list(GET _package_names ${_index} _package_name)
    list(GET _package_rules ${_index} _package_rule_encoded)
    string(SUBSTRING "${_package_rule_encoded}" 1 -1 _package_rule)
    list(APPEND _box_args
      --package "${_package_name}" "${_package_rule}"
    )
  endforeach()

  add_custom_target(${TARGET_NAME}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
    COMMAND $<TARGET_FILE:ldk_box>
      pack
      --root "${ROOT_DIR}"
      --output "${OUTPUT_DIR}"
      ${_box_args}
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Building game .box packages"
  )
  add_dependencies(${TARGET_NAME} ldk_box)
endfunction()
