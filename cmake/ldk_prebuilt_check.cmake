# Checked every time a game DLL is built with OPTION_LDK_USE_PREBUILT.
# Intentionally does not build or modify engine artifacts.
foreach(_required
    LDK_PREBUILT_ROOT
    LDK_ENGINE_SOURCE_ROOT
    LDK_PREBUILT_CONFIG
    LDK_PREBUILT_PLATFORM
    LDK_PREBUILT_IMPORT_NAME)
  if (NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "Prebuilt LDK check: missing ${_required}.")
  endif()
endforeach()

set(_ldk_dll "${LDK_PREBUILT_ROOT}/bin/${LDK_PREBUILT_CONFIG}/ldk.dll")
set(_ldk_implib
  "${LDK_PREBUILT_ROOT}/lib/${LDK_PREBUILT_CONFIG}/${LDK_PREBUILT_PLATFORM}/${LDK_PREBUILT_IMPORT_NAME}")

if (NOT EXISTS "${_ldk_dll}" OR NOT EXISTS "${_ldk_implib}")
  message(FATAL_ERROR
    "Prebuilt LDK is missing for configuration '${LDK_PREBUILT_CONFIG}' "
    "and toolchain '${LDK_PREBUILT_PLATFORM}'.\n"
    "Expected engine DLL: ${_ldk_dll}\n"
    "Expected import library: ${_ldk_implib}\n"
    "Build the engine explicitly for this configuration/compiler/architecture "
    "(with OPTION_LDK_USE_PREBUILT=OFF), then build the game again. "
    "The game build will not rebuild the engine automatically.")
endif()

# The runtime DLL is shared by buildroots and its bin/ path is not split by
# architecture. Reject a mismatched PE machine even if a matching import
# library exists in the compiler-specific lib/ directory.
if (DEFINED LDK_PREBUILT_ARCH)
  set(_ldk_machine "")
  if (LDK_PREBUILT_ARCH STREQUAL "x86")
    set(_ldk_machine "4c01")
  elseif (LDK_PREBUILT_ARCH STREQUAL "x64")
    set(_ldk_machine "6486")
  elseif (LDK_PREBUILT_ARCH STREQUAL "arm64")
    set(_ldk_machine "64aa")
  endif()

  if (NOT _ldk_machine STREQUAL "")
    file(SIZE "${_ldk_dll}" _ldk_dll_size)
    if (_ldk_dll_size LESS 70)
      message(FATAL_ERROR "Prebuilt engine DLL is not a valid PE image: ${_ldk_dll}")
    endif()
    file(READ "${_ldk_dll}" _ldk_mz OFFSET 0 LIMIT 2 HEX)
    file(READ "${_ldk_dll}" _ldk_pe_offset_hex OFFSET 60 LIMIT 4 HEX)
    if (NOT _ldk_mz STREQUAL "4d5a")
      message(FATAL_ERROR "Prebuilt engine DLL has no DOS/PE signature: ${_ldk_dll}")
    endif()
    string(SUBSTRING "${_ldk_pe_offset_hex}" 0 2 _ldk_b0)
    string(SUBSTRING "${_ldk_pe_offset_hex}" 2 2 _ldk_b1)
    string(SUBSTRING "${_ldk_pe_offset_hex}" 4 2 _ldk_b2)
    string(SUBSTRING "${_ldk_pe_offset_hex}" 6 2 _ldk_b3)
    math(EXPR _ldk_pe_offset "0x${_ldk_b3}${_ldk_b2}${_ldk_b1}${_ldk_b0}")
    math(EXPR _ldk_pe_end "${_ldk_pe_offset} + 6")
    if (_ldk_pe_end GREATER _ldk_dll_size)
      message(FATAL_ERROR "Prebuilt engine DLL has an invalid PE offset: ${_ldk_dll}")
    endif()
    file(READ "${_ldk_dll}" _ldk_pe_header OFFSET ${_ldk_pe_offset} LIMIT 6 HEX)
    string(SUBSTRING "${_ldk_pe_header}" 0 8 _ldk_pe_signature)
    string(SUBSTRING "${_ldk_pe_header}" 8 4 _ldk_actual_machine)
    if (NOT _ldk_pe_signature STREQUAL "50450000")
      message(FATAL_ERROR "Prebuilt engine DLL has an invalid PE signature: ${_ldk_dll}")
    endif()
    if (NOT _ldk_actual_machine STREQUAL _ldk_machine)
      message(FATAL_ERROR
        "Prebuilt engine DLL architecture differs from the requested '${LDK_PREBUILT_ARCH}'.\n"
        "DLL: ${_ldk_dll}\n"
        "The shared bin/<config>/ldk.dll was likely built by another toolchain. "
        "Rebuild the engine for the requested architecture before building this game.")
    endif()
  endif()
endif()

# Staleness is a conservative timestamp heuristic. Check engine translation
# units and headers, but ignore editor, test, launcher and game-only sources.
# GLOB_RECURSE runs here (at build time), so newly added sources are covered.
file(GLOB_RECURSE _ldk_sources LIST_DIRECTORIES false
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.c"
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.cc"
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.cpp"
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.h"
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.hpp"
  "${LDK_ENGINE_SOURCE_ROOT}/src/*.inl"
  "${LDK_ENGINE_SOURCE_ROOT}/include/*.h"
  "${LDK_ENGINE_SOURCE_ROOT}/include/*.hpp"
  "${LDK_ENGINE_SOURCE_ROOT}/include/*.inl")

file(TIMESTAMP "${_ldk_dll}" _ldk_dll_time "%Y%m%d%H%M%S" UTC)
foreach(_ldk_source IN LISTS _ldk_sources)
  file(TO_CMAKE_PATH "${_ldk_source}" _ldk_source_path)
  if (_ldk_source_path MATCHES "/(src|include)/(editor|tests|launcher)/" OR
      _ldk_source_path MATCHES "/src/tools/" OR
      _ldk_source_path MATCHES "/src/ldk_app_")
    continue()
  endif()

  file(TIMESTAMP "${_ldk_source}" _ldk_source_time "%Y%m%d%H%M%S" UTC)
  if (_ldk_source_time STRGREATER _ldk_dll_time)
    file(RELATIVE_PATH _ldk_relative "${LDK_ENGINE_SOURCE_ROOT}" "${_ldk_source}")
    message(WARNING
      "LDK engine source '${_ldk_relative}' is newer than the prebuilt "
      "'${_ldk_dll}'.\n"
      "The game will use the existing engine binary. Rebuild the engine "
      "explicitly and restart the editor to use the modified engine. "
      "Public API changes may be incompatible with the running editor.")
    break()
  endif()
endforeach()
