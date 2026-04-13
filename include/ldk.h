/**
 * @file   ldk.h
 * @brief  Engine root and global module access
 *
 * Entry point for the LDK engine. Provides initialization, shutdown,
 * and access to core submodules.
 */

#ifndef LDK_H
#define LDK_H

#include <ldk_common.h>
#include <stdx/stdx_log.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_string.h>

#ifdef __cplusplus
extern "C" {
#endif

  //
  // Utility logging macros
  //

#define ldk_log_raw(out, level, fg, bg, components, fmt, ...) \
  x_log_raw(ldk_module_get(LDK_MODULE_LOG), (out), (level), (fg), (bg), (components), fmt, ##__VA_ARGS__)

#define ldk_log_debug(fmt, ...) \
  x_log_debug(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_info(fmt, ...) \
  x_log_info(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_warning(fmt, ...) \
  x_log_warning(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_error(fmt, ...) \
  x_log_error(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_fatal(fmt, ...) \
  x_log_fatal(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

  typedef struct LDKRoot LDKRoot;

  typedef enum LDKModuleType
  {
    LDK_MODULE_NONE = 0,
    LDK_MODULE_ENTITY,
    LDK_MODULE_COMPONENT,
    LDK_MODULE_SYSTEM,
    LDK_MODULE_EVENT,
    LDK_MODULE_LOG,
  } LDKModuleType;

  struct LDKGame;
  typedef struct LDKGame LDKGame;

  typedef struct LDKConfig
  {
    XFSPath   config_file_path;
    XFSPath   runtree_path;
    XFSPath   icon_path;
    XFSPath   asset_root;
    XFSPath   log_file;
    XFSPath   game_dll;
    XSmallstr title;
    i32       width;
    i32       height;
    bool      fullscreen;
  } LDKConfig;

  LDK_API bool  ldk_engine_initialize(const LDKGame* game, const char* config_ini_path);
  LDK_API bool  ldk_engine_initialize_with_config(const LDKGame* game, const LDKConfig* config);
  LDK_API bool  ldk_engine_is_initialized(void); // Checks if the engine was initialized
  LDK_API bool  ldk_engine_is_playing(void);
  /*
   * Starts runtime simulation. In editor builds the scene may already be visible
   * and editable before play starts; play mode only enables game/runtime logic.
   */
  LDK_API bool  ldk_engine_play_start(void);
  /*
   * Stops runtime simulation and returns to editor-only preview state.
   */
  LDK_API void  ldk_engine_play_stop(void);
  /*
   * Advances one engine frame. This always drives editor/view rendering and only
   * calls game.update while play mode is active.
   */
  LDK_API void  ldk_engine_frame(void);
  LDK_API void* ldk_module_get(LDKModuleType module_type); // Returns the context pointer of a given engine module
  LDK_API i32   ldk_engine_run(void);
  LDK_API void  ldk_engine_stop(i32 exit_code);
  LDK_API void  ldk_engine_terminate(void); // finalizes the engine

#ifdef __cplusplus
}
#endif

#endif // LDK_H
