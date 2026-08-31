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
extern "C"
{
#endif

#ifdef LDK_EDITOR

  typedef struct LDKProjectCreateDesc
  {
    const char *project_name;
    const char *project_root_path;
    const char *cmake_generator;
    const char *cmake_arch;
  } LDKProjectCreateDesc;

  typedef struct LDKProjectBuildDesc
  {
    const char *cmake_path;
    const char *ldk_root_path;
    const char *config;
    bool new_console;
  } LDKProjectBuildDesc;

  typedef struct LDKProject
  {
    bool loaded;
    XSmallstr name;
    XSmallstr cmake_generator;
    XSmallstr cmake_arch;
    XFSPath assets_path;
    XFSPath cache_path;
    XFSPath cmake_root_path;
    XFSPath game_cmake_path;
    XFSPath game_dll_path;
    XFSPath project_file_path;
    XFSPath project_root_path;
    XFSPath run_root_path;
    XFSPath runtime_ini_path;
    XFSPath source_root_path;
    i32 project_resolution_width;
    i32 project_resolution_height;
  } LDKProject;

  LDK_API bool ldk_project_create(const LDKProjectCreateDesc *desc);
  LDK_API bool ldk_project_load(
      LDKProject *project, const char *project_file_path);
  LDK_API void ldk_project_unload(LDKProject *project);
  LDK_API bool ldk_project_write_runtime_ini(const LDKProject *project);
  LDK_API bool ldk_project_generate_game_module(
      const LDKProject *project, const LDKProjectBuildDesc *desc);
  LDK_API bool ldk_project_build_game_module(
      const LDKProject *project, const LDKProjectBuildDesc *desc);

#endif // LDK_EDITOR

#ifdef __cplusplus
}
#endif

#endif // LDK_PROJECT_H
