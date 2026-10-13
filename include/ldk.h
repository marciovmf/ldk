/**
 * # Engine lifecycle and game input
 *
 * Engine root and global module access
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
extern "C"
{
#endif

  // Utility logging macros

#define ldk_log_raw(out, level, fg, bg, components, fmt, ...)                  \
  x_log_raw(ldk_module_get(LDK_MODULE_LOG), (out), (level), (fg), (bg),        \
      (components), fmt, ##__VA_ARGS__)

#define ldk_log_debug(fmt, ...)                                                \
  x_log_debug(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_info(fmt, ...)                                                 \
  x_log_info(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_warning(fmt, ...)                                              \
  x_log_warning(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_error(fmt, ...)                                                \
  x_log_error(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

#define ldk_log_fatal(fmt, ...)                                                \
  x_log_fatal(ldk_module_get(LDK_MODULE_LOG), fmt, ##__VA_ARGS__)

  typedef struct LDKRoot LDKRoot;

  typedef enum LDKModuleType
  {
    LDK_MODULE_ASSET_MANAGER,
    LDK_MODULE_ECS,
    LDK_MODULE_EVENT,
    LDK_MODULE_LOG,
    LDK_MODULE_RENDERER,
    LDK_MODULE_SCENE_MANAGER,
    LDK_MODULE_ASSET_SOURCE,
    LDK_MODULE_AUDIO,
    LDK_MODULE_JOBS,
  } LDKModuleType;

  struct LDKGame;
  typedef struct LDKGame LDKGame;

  typedef struct LDKConfig
  {
    XSmallstr title;
    XFSPath config_file_path;
    XFSPath runtree_path;
    XFSPath icon_path;
    XFSPath asset_root;
    XFSPath log_file;
    XFSPath game_dll;
    i32 display_width;
    i32 display_height;
    i32 resolution_width;
    i32 resolution_height;
    u32 shadow_map_resolution;
    float shadow_distance;
    i32 initial_ui_index_capacity;
    i32 initial_ui_vertex_capacity;
    bool fullscreen;
  } LDKConfig;

  /**
   * @brief Initialize the engine.
   * @param config_ini_path Path to the engine configuration INI.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_engine_initialize(const char *config_ini_path);
  /**
   * @brief Populate an engine configuration from parsed INI settings.
   * @param out_config Receives the resulting config.
   * @param ini Ini used by this operation.
   * @param config_ini_path Path to the engine configuration INI.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_engine_config_from_ini(
      LDKConfig *out_config, XIni *ini, const char *config_ini_path);
  /**
   * @brief Initialize the engine from a configuration supplied by the caller.
   * @param config Configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_engine_initialize_with_config(const LDKConfig *config);
  /**
   * @brief Set the internal resolution used for game rendering.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_engine_render_resolution_set(i32 width, i32 height);
  /**
   * @brief Set shadow map resolution and shadow distance.
   * @param resolution Resolution setting.
   * @param distance Distance in world-space units.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_engine_shadow_settings_set(u32 resolution, float distance);
  /**
   * @brief Check whether the engine is initialized.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_engine_is_initialized(
      void); // Checks if the engine was initialized
  /**
   * @brief Process one engine frame.
   */
  LDK_API void ldk_engine_frame(void);
  /**
   * @brief Access the context of a registered engine module.
   * @param module_type Module type used by this operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API void *ldk_module_get(LDKModuleType
          module_type); // Returns the context pointer of a given engine module
  /**
   * @brief Enter the engine main loop and return its exit code.
   * @return Process or engine exit code.
   */
  LDK_API i32 ldk_engine_run(void);

  /* Game-facing input. Unlike ldk_os_* input, these functions account for
   * the game being embedded in the editor. */
  /**
   * @brief Get the input mouse state.
   * @param out_state Receives the requested input state snapshot.
   */
  LDK_API void ldk_input_mouse_state_get(LDKMouseState *out_state);
  /**
   * @brief Test whether the button is currently held.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_input_mouse_button_is_pressed(
      LDKMouseState *state, LDKMouseButton button);
  /**
   * @brief Test whether the button was pressed during the current input update.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_input_mouse_button_down(
      LDKMouseState *state, LDKMouseButton button);
  /**
   * @brief Test whether the button was released during the current input update.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_input_mouse_button_up(
      LDKMouseState *state, LDKMouseButton button);
  /**
   * @brief Get the accumulated mouse wheel movement in the input snapshot.
   * @param state Input snapshot or state object to query.
   * @return The resulting integer value.
   */
  LDK_API i32 ldk_input_mouse_wheel_delta(LDKMouseState *state);
  /**
   * @brief Get the mouse cursor position.
   * @param state Input snapshot or state object to query.
   * @return The resulting point.
   */
  LDK_API LDKPoint ldk_input_mouse_cursor(LDKMouseState *state);
  /**
   * @brief Get the mouse cursor position relative to the active client area.
   * @param state Input snapshot or state object to query.
   * @return The resulting point.
   */
  LDK_API LDKPoint ldk_input_mouse_cursor_relative(LDKMouseState *state);

  /**
   * @brief Get the input keyboard state.
   * @param out_state Receives the requested input state snapshot.
   */
  LDK_API void ldk_input_keyboard_state_get(LDKKeyboardState *out_state);
  /**
   * @brief Test whether the key is currently held.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_input_keyboard_key_is_pressed(
      LDKKeyboardState *state, LDKKeycode keycode);
  /**
   * @brief Test whether the key was pressed during the current input update.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_input_keyboard_key_down(
      LDKKeyboardState *state, LDKKeycode keycode);
  /**
   * @brief Test whether the key was released during the current input update.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_input_keyboard_key_up(
      LDKKeyboardState *state, LDKKeycode keycode);

#ifdef LDK_EDITOR
  /**
   * @brief Set the embedded editor game viewport used for input remapping.
   * @param x X coordinate or component.
   * @param y Y coordinate or component.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param game_width Game width used by this operation.
   * @param game_height Game height used by this operation.
   */
  LDK_API void ldk_input_game_view_set(float x, float y, float width,
      float height, u32 game_width, u32 game_height);
  /**
   * @brief Clear the editor game viewport input remapping.
   */
  LDK_API void ldk_input_game_view_clear(void);
#endif

#ifndef LDK_MONOLITHIC
  /**
   * @brief Get the game.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKGame *ldk_game_get(void);
#endif
#ifdef LDK_MONOLITHIC
  /**
   * @brief Load a statically linked game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_load_static(void);
#endif
  /**
   * @brief Shut down the engine.
   */
  LDK_API void ldk_engine_terminate(void); // finalizes the engine
  /**
   * @brief Request termination of the engine main loop with an exit code.
   * @param exit_code Exit code used by this operation.
   */
  LDK_API void ldk_engine_stop(i32 exit_code);

  /**
   * @brief Load a game module from a dynamic library.
   * @param path Path identifying the file or resource.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_load_from_shared_lib(const char *path);
  /**
   * @brief Initialize the game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_initialize(void);
  /**
   * @brief Start the game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_start(void);

  /**
   * Stop gameplay without unloading the game module or clearing the ECS.
   * The editor may then reload the original scene from disk. A stop requested
   * during an update is completed at the safe frame boundary.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_stop(void);
  /**
   * @brief Pause the game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_pause(void);
  /**
   * @brief Resume the game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_resume(void);
  /**
   * @brief Advance a paused game by one update step.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_step(void);
  /**
   * @brief Check whether the game instance is started.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_game_instance_is_started(void);
  /**
   * @brief Check whether the game instance is paused.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_game_instance_is_paused(void);
  /**
   * @brief Check whether the game instance is in a single-step update.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_game_instance_is_stepping(void);
  /**
   * @brief Check whether the game instance is currently updating.
   * @return True when the queried condition holds; false otherwise.
   */
  LDK_API bool ldk_game_instance_is_updating(void);
  /**
   * @brief Shut down the game instance.
   */
  LDK_API void ldk_game_instance_terminate(void);
  /**
   * @brief Unload the game instance.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_game_instance_unload(void);
  /**
   * @brief Get the game.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKGame *ldk_game_get(void);
  /**
   * @brief Get the engine main window.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKWindow ldk_engine_main_window_get(void);
  /**
   * @brief Get the engine main window handle.
   * @return Handle to the requested object or resource.
   */
  LDK_API LDKWindow ldk_main_window(void);
  /**
   * @brief Get a read-only pointer to the active engine configuration.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API const LDKConfig *ldk_engine_config_get(void);

#ifdef __cplusplus
}
#endif

#endif // LDK_H
