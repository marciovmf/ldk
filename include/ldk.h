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
#include <ldk_os.h>
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
    i32       display_width;
    i32       display_height;
    i32       resolution_width;
    i32       resolution_height;
    i32       initial_ui_index_capacity;
    i32       initial_ui_vertex_capacity;
    bool      fullscreen;
  } LDKConfig;

  LDK_API bool  ldk_engine_initialize(const char* config_ini_path);
  LDK_API bool ldk_engine_config_from_ini(LDKConfig* out_config, XIni* ini, const char* config_ini_path);
  LDK_API bool  ldk_engine_initialize_with_config(const LDKConfig* config);
  LDK_API bool  ldk_engine_render_resolution_set(i32 width, i32 height);
  LDK_API bool  ldk_engine_is_initialized(void); // Checks if the engine was initialized
  LDK_API void  ldk_engine_frame(void);
  LDK_API void* ldk_module_get(LDKModuleType module_type); // Returns the context pointer of a given engine module
  LDK_API i32   ldk_engine_run(void);

  /* Game-facing input. Unlike ldk_os_* input, these functions account for
   * the game being embedded in the editor. */
  LDK_API void ldk_input_mouse_state_get(LDKMouseState* out_state);
  LDK_API bool ldk_input_mouse_button_is_pressed(LDKMouseState* state,
      LDKMouseButton button);
  LDK_API bool ldk_input_mouse_button_down(LDKMouseState* state,
      LDKMouseButton button);
  LDK_API bool ldk_input_mouse_button_up(LDKMouseState* state,
      LDKMouseButton button);
  LDK_API i32 ldk_input_mouse_wheel_delta(LDKMouseState* state);
  LDK_API LDKPoint ldk_input_mouse_cursor(LDKMouseState* state);
  LDK_API LDKPoint ldk_input_mouse_cursor_relative(LDKMouseState* state);

  LDK_API void ldk_input_keyboard_state_get(LDKKeyboardState* out_state);
  LDK_API bool ldk_input_keyboard_key_is_pressed(LDKKeyboardState* state,
      LDKKeycode keycode);
  LDK_API bool ldk_input_keyboard_key_down(LDKKeyboardState* state,
      LDKKeycode keycode);
  LDK_API bool ldk_input_keyboard_key_up(LDKKeyboardState* state,
      LDKKeycode keycode);

#ifdef LDK_EDITOR
  LDK_API void ldk_input_game_view_set(float x, float y, float width,
      float height, u32 game_width, u32 game_height);
  LDK_API void ldk_input_game_view_clear(void);
#endif

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
LDK_API const LDKConfig* ldk_engine_config_get(void);


#ifdef __cplusplus
}
#endif

#endif // LDK_H
