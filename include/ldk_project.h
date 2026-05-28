/**
 * @file   ldk_project.h
 * @brief  Editor-side LDK project descriptor utilities.
 *
 * LDKProject represents the development-time project manifest. It is not a
 * runtime concept. Runtime code should consume only runtree/game.ini.
 */

#ifndef LDK_PROJECT_H
#define LDK_PROJECT_H

#include <ldk_common.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LDK_EDITOR

  typedef struct LDKProjectCreateDesc
  {
    const char* project_name;
    const char* project_root_path;
    const char* cmake_generator;
  } LDKProjectCreateDesc;

  typedef struct LDKProjectBuildDesc
  {
    const char* ldk_source_root_path;
    const char* config;
    const char* prebuilt_ldk_path;
    bool        use_prebuilt_ldk;
  } LDKProjectBuildDesc;

  typedef struct LDKProject
  {
    bool loaded;
    XSmallstr name;
    XSmallstr cmake_generator;
    XFSPath project_file_path;
    XFSPath project_root_path;
    XFSPath cache_path;
    XFSPath cmake_root_path;
    XFSPath run_root_path;
    XFSPath runtime_ini_path;
    XFSPath game_cmake_path;
    XFSPath game_dll_path;
  } LDKProject;

  LDK_API bool ldk_project_create(const LDKProjectCreateDesc* desc);
  LDK_API bool ldk_project_load(LDKProject* project, const char* project_file_path);
  LDK_API void ldk_project_unload(LDKProject* project);

  LDK_API bool ldk_project_write_runtime_ini(const LDKProject* project);
  LDK_API bool ldk_project_build_game_dll(const LDKProject* project, const LDKProjectBuildDesc* desc);

#endif // LDK_EDITOR

#ifdef __cplusplus
}
#endif

#endif // LDK_PROJECT_H
