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
#include <stdx/stdx_ini.h>

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
    LDK_MODULE_ASSET_MANAGER,
    LDK_MODULE_ECS,
    LDK_MODULE_EVENT,
    LDK_MODULE_LOG,
    LDK_MODULE_RENDERER,
  } LDKModuleType;

  struct LDKGame;
  typedef struct LDKGame LDKGame;

  typedef struct LDKConfig
  {
    XSmallstr title;
    XFSPath   config_file_path;
    XFSPath   runtree_path;
    XFSPath   icon_path;
    XFSPath   asset_root;
    XFSPath   log_file;
    XFSPath   game_dll;
    i32       width;
    i32       height;
    i32       initial_ui_index_capacity;
    i32       initial_ui_vertex_capacity;
    bool      fullscreen;
  } LDKConfig;

  LDK_API bool  ldk_engine_initialize(const char* config_ini_path);
  LDK_API bool ldk_engine_config_from_ini(LDKConfig* out_config, XIni* ini, const char* config_ini_path);
  LDK_API bool  ldk_engine_initialize_with_config(const LDKConfig* config);
  LDK_API bool  ldk_engine_is_initialized(void); // Checks if the engine was initialized
  LDK_API void  ldk_engine_frame(void);
  LDK_API void* ldk_module_get(LDKModuleType module_type); // Returns the context pointer of a given engine module
  LDK_API i32   ldk_engine_run(void);

#ifndef LDK_MONOLITHIC
  LDK_API LDKGame* ldk_game_get(void);
  LDK_API bool ldk_game_instance_load_static(void);
#endif
  LDK_API void  ldk_engine_terminate(void); // finalizes the engine
  LDK_API void  ldk_engine_stop(i32 exit_code);


LDK_API bool ldk_game_instance_load_from_shared_lib(const char* path);
LDK_API bool ldk_game_instance_initialize(void);
LDK_API bool ldk_game_instance_start(void);
LDK_API void ldk_game_instance_terminate(void);
LDK_API bool ldk_game_instance_unload(void);
LDK_API LDKGame* ldk_game_get(void);
LDK_API LDKWindow ldk_engine_main_window_get(void);
LDK_API LDKWindow ldk_main_window(void);
LDK_API LDKWindow ldk_main_window(void);
LDK_API LDKWindow ldk_main_window(void);


#ifdef __cplusplus
}
#endif

#endif // LDK_H
