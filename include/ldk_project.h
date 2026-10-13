/**
 * # Editor projects
 *
 * Editor-side LDK project descriptor utilities.
 *
 * LDKProject represents the development-time project manifest. It is not a
 * runtime concept. Runtime code should consume only runtree/game.ini.
 */

#ifndef LDK_PROJECT_H
#define LDK_PROJECT_H

#include <ldk_common.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_string.h>
#include <ldk_scene.h>

#ifdef LDK_EDITOR
#include "ldk_os.h"
#endif

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
    bool play_current_scene;
    bool build_on_play;
    XSmallstr name;
    XSmallstr game_name;
    XSmallstr build_config;
    XSmallstr cmake_generator;
    XSmallstr cmake_arch;
    XFSPath icon_path;
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
    u32 shadow_map_resolution;
    float shadow_distance;
  } LDKProject;

  /* Optional main-thread observer for project manifest load errors.
   * Each failure is also written to the engine logger. */
  /**
   * @brief Callback for reporting a project diagnostic.
   * @param message Message displayed to the user.
   * @param user Opaque caller-supplied data passed to the callback.
   */
  typedef void (*LDKProjectDiagnosticFn)(const char *message, void *user);
  /**
   * @brief Set the project diagnostic handler.
   * @param handler Handler called for the corresponding event or diagnostic.
   * @param user Opaque caller-supplied data passed to the callback.
   */
  LDK_API void ldk_project_diagnostic_handler_set(
      LDKProjectDiagnosticFn handler, void *user);

  /**
   * @brief Create the project.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_create(const LDKProjectCreateDesc *desc);
  /**
   * @brief Load the project.
   * @param project Project whose settings or files are used.
   * @param project_file_path Path to the LDK project manifest.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_load(
      LDKProject *project, const char *project_file_path);
  /**
   * @brief Unload the project.
   * @param project Project whose settings or files are used.
   */
  LDK_API void ldk_project_unload(LDKProject *project);
  /**
   * @brief Save the project.
   * @param project Project whose settings or files are used.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_save(LDKProject *project);
  /**
   * @brief Write project runtime INI.
   * @param project Project whose settings or files are used.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_write_runtime_ini(const LDKProject *project);
  /** Replace only [scenes] and export public sections. No ECS operations.
   * @param project Project whose settings or files are used.
   * @param scenes Scenes used by this operation.
   * @param count Number of items to process.
   * @param result Optional result structure populated with error details.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_scene_catalog_save(const LDKProject *project,
      const LDKScene *scenes, u32 count, LDKSceneResult *result);
  /**
   * @brief Generate build files for a game module.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_generate_game_module(
      const LDKProject *project, const LDKProjectBuildDesc *desc);
  /**
   * @brief Build the project game module.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_build_game_module(
      const LDKProject *project, const LDKProjectBuildDesc *desc);
  /**
   * @brief Generate build files for the standalone game launcher.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_generate_game_launcher(
      const LDKProject *project, const LDKProjectBuildDesc *desc);
  /**
   * @brief Build the standalone game launcher.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_build_game_launcher(
      const LDKProject *project, const LDKProjectBuildDesc *desc);

  /**
   * @brief Start the project generate game module.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKOSProcess *ldk_project_generate_game_module_start(
      const LDKProject *project, const LDKProjectBuildDesc *desc,
      LDKOSProcessResult *out_result);
  /**
   * @brief Start the project build game module.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKOSProcess *ldk_project_build_game_module_start(
      const LDKProject *project, const LDKProjectBuildDesc *desc,
      LDKOSProcessResult *out_result);
  /**
   * @brief Start the project clean game module.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKOSProcess *ldk_project_clean_game_module_start(
      const LDKProject *project, const LDKProjectBuildDesc *desc,
      LDKOSProcessResult *out_result);
  /**
   * @brief Start the project generate game launcher.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKOSProcess *ldk_project_generate_game_launcher_start(
      const LDKProject *project, const LDKProjectBuildDesc *desc,
      LDKOSProcessResult *out_result);
  /**
   * @brief Start the project build game launcher.
   * @param project Project whose settings or files are used.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKOSProcess *ldk_project_build_game_launcher_start(
      const LDKProject *project, const LDKProjectBuildDesc *desc,
      LDKOSProcessResult *out_result);

  /**
   * @brief Get the project game module output path.
   * @param project Project whose settings or files are used.
   * @param config Configuration for this operation.
   * @param out_path Receives the resulting path.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_game_module_output_path_get(
      const LDKProject *project, const char *config, XFSPath *out_path);
  /**
   * @brief Get the project game launcher output path.
   * @param project Project whose settings or files are used.
   * @param config Configuration for this operation.
   * @param out_path Receives the resulting path.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_project_game_launcher_output_path_get(
      const LDKProject *project, const char *config, XFSPath *out_path);

#endif // LDK_EDITOR

#ifdef __cplusplus
}
#endif

#endif // LDK_PROJECT_H
