/**
 * # Operating-system services
 *
 * Platform abstraction layer
 *
 * Windowing, input, timing, memory, and system integration.
 */

#ifndef LDK_OS_H
#define LDK_OS_H

#include <ldk_common.h>
#include <ldk_geom.h>
#include <module/ldk_eventqueue.h>

#include <stdx/stdx_string.h>
#include <stdx/stdx_filesystem.h>

#ifdef __cplusplus
extern "C"
{
#endif

  // ---------------------------------------------------------------------------
  // Initialization / Termination
  // ---------------------------------------------------------------------------
  /**
   * @brief Initialize the OS.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_initialize(void);
  /**
   * @brief Shut down the OS.
   * @group ldk.os
   */
  LDK_API void ldk_os_terminate(void);
  /**
   * @brief Print the current stack trace for diagnostics.
   * @group ldk.os
   */
  LDK_API void ldk_os_stack_trace_print(void);

  // ---------------------------------------------------------------------------
  // Memory
  // ---------------------------------------------------------------------------
  /**
   * @brief Allocate memory using the OS abstraction.
   * @param size Size of the data or region, in bytes where applicable.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.os
   */
  LDK_API void *ldk_os_memory_alloc(size_t size);
  /**
   * @brief Release an OS-allocated memory block.
   * @param memory Memory used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_memory_free(void *memory);
  /**
   * @brief Resize an OS-allocated block of memory.
   * @param memory Memory used by this operation.
   * @param size Size of the data or region, in bytes where applicable.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.os
   */
  LDK_API void *ldk_os_memory_resize(void *memory, size_t size);

  // ---------------------------------------------------------------------------
  // Time
  // ---------------------------------------------------------------------------
  /**
   * @brief Read the high-resolution time counter.
   * @return The resulting numeric value.
   * @group ldk.os
   */
  LDK_API u64 ldk_os_time_ticks_get(void);
  /**
   * @brief Calculate elapsed time between two counter ticks in seconds.
   * @param start Starting timestamp or location for this operation.
   * @param end Ending timestamp or location for this operation.
   * @return Elapsed time in seconds.
   * @group ldk.os
   */
  LDK_API double ldk_os_time_ticks_interval_get_seconds(u64 start, u64 end);
  /**
   * @brief Calculate elapsed time between two counter ticks in milliseconds.
   * @param start Starting timestamp or location for this operation.
   * @param end Ending timestamp or location for this operation.
   * @return Elapsed time in milliseconds.
   * @group ldk.os
   */
  LDK_API double ldk_os_time_ticks_interval_get_milliseconds(
      u64 start, u64 end);
  /**
   * @brief Calculate elapsed time between two counter ticks in nanoseconds.
   * @param start Starting timestamp or location for this operation.
   * @param end Ending timestamp or location for this operation.
   * @return Elapsed time in nanoseconds.
   * @group ldk.os
   */
  LDK_API double ldk_os_time_ticks_interval_get_nanoseconds(u64 start, u64 end);

  // ---------------------------------------------------------------------------
  // Windowing
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef enum
  {

    LDK_WINDOW_FLAG_NORMAL = 1 << 1,
    LDK_WINDOW_FLAG_MAXIMIZED = 1 << 2,
    LDK_WINDOW_FLAG_MINIMIZED = 1 << 3,
    LDK_WINDOW_FLAG_NORESIZE = 1 << 4,
    LDK_WINDOW_FLAG_HIDDEN = 1 << 5,
    LDK_WINDOW_FLAG_CENTERED = 1 << 6,
    LDK_WINDOW_FLAG_NOTITLEBAR = 1 << 7,
  } LDKWindowFlags;

  /**
   * @brief Process pending operating-system events.
   * @group ldk.os
   */
  LDK_API void ldk_os_events_pump(void);
  /**
   * @brief Dequeue the next pending operating-system event.
   * @param event Event to inspect or receive.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_event_next(LDKEvent *event);
  /**
   * @brief Poll for the next platform event.
   * @param event Event to inspect or receive.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_events_poll(LDKEvent *event);
  /**
   * @brief Create a platform window with explicit window flags.
   * @param title Title shown by the window or dialog.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param flags Bitmask of options controlling this operation.
   * @return Handle to the requested object or resource.
   * @group ldk.os
   */
  LDK_API LDKWindow ldk_os_window_create_with_flags(
      const char *title, i32 width, i32 height, LDKWindowFlags flags);
  /**
   * @brief Create the OS window.
   * @param title Title shown by the window or dialog.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @return Handle to the requested object or resource.
   * @group ldk.os
   */
  LDK_API LDKWindow ldk_os_window_create(
      const char *title, i32 width, i32 height);
  /**
   * @brief Check whether a native window has requested to close.
   * @param window Native window handle.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_window_should_close(LDKWindow window);
  /**
   * @brief Present a window by swapping its graphics buffers.
   * @param window Native window handle.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_buffers_swap(LDKWindow window);
  /**
   * @brief Destroy the OS window.
   * @param window Native window handle.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_destroy(LDKWindow window);
  /**
   * @brief Get the OS window fullscreen.
   * @param window Native window handle.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_window_fullscreen_get(LDKWindow window);
  /**
   * @brief Set the OS window fullscreen.
   * @param window Native window handle.
   * @param fs Whether fullscreen mode should be enabled.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_window_fullscreen_set(LDKWindow window, bool fs);
  /**
   * @brief Get the OS window client area size.
   * @param window Native window handle.
   * @return The resulting size.
   * @group ldk.os
   */
  LDK_API LDKSize ldk_os_window_client_area_size_get(LDKWindow window);
  /**
   * @brief Set the OS window client area size.
   * @param window Native window handle.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_client_area_size_set(
      LDKWindow window, int width, int height);
  /**
   * @brief Get the OS window position.
   * @param window Native window handle.
   * @return The resulting point.
   * @group ldk.os
   */
  LDK_API LDKPoint ldk_os_window_position_get(LDKWindow window);
  /**
   * @brief Set the OS window position.
   * @param window Native window handle.
   * @param x X coordinate or component.
   * @param y Y coordinate or component.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_position_set(LDKWindow window, i32 x, i32 y);
  /**
   * @brief Set the OS window size.
   * @param window Native window handle.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_size_set(LDKWindow window, i32 width, i32 height);
  /**
   * @brief Get the OS window size.
   * @param window Native window handle.
   * @return The resulting size.
   * @group ldk.os
   */
  LDK_API LDKSize ldk_os_window_size_get(LDKWindow window);
  /**
   * @brief Change the title shown by a native window.
   * @param window Native window handle.
   * @param title Title shown by the window or dialog.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_title_set(LDKWindow window, const char *title);
  /**
   * @brief Copy the current native window title into a string.
   * @param window Native window handle.
   * @param outTitle Receives the current window title.
   * @return The resulting numeric value.
   * @group ldk.os
   */
  LDK_API size_t ldk_os_window_title_get(LDKWindow window, XSmallstr *outTitle);

  /**
   * If a window was created with LDK_WINDOW_FLAG_NOTITLEBAR, this function sets
   * the area where the mouse cursor can drag to mode the window. The default
   * value is the whole window.
   * If the window does not have the LDK_WINDOW_FLAG_NOTITLEBAR flag, this
   * function does nothing.
   * @param window Native window handle.
   * @param rect UI or geometry rectangle.
   * @group ldk.os
   */
  LDK_API void ldk_os_window_draggable_area_set(LDKWindow window, LDKRect rect);

  /**
   * @brief Set the OS window icon.
   * @param window Native window handle.
   * @param iconPath Path of the image used as a window icon.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_window_icon_set(LDKWindow window, const char *iconPath);
  /**
   * Show or hide a window.
   * @param window The LDK window to show or hide.
   * @param show true to show, false to hide the window.
   * @return
   *  If the window was previously visible, the return value is nonzero.
   *  If the window was previously hidden, the return value is zero.
   * @group ldk.os
   */
  LDK_API bool ldk_os_window_show(LDKWindow window, bool show);

  /**
   * Maximizes a window
   * @param window The LDK window to maximize. The window is shown if previously
   * hidden.
   * @return void
   * @group ldk.os
   */
  LDK_API void ldk_os_window_maximize(LDKWindow window);

  /**
   * Minimizes a window
   * @param window The LDK window to minimize. The window is shown if previously
   * hidden.
   * @return void
   * @group ldk.os
   */
  LDK_API void ldk_os_window_minimize(LDKWindow window);

  /**
   * Restores a minimized or maximized window
   * @param window The LDK window to restore. The window is shown if previously
   * hidden.
   * @return void
   * @group ldk.os
   */
  LDK_API void ldk_os_window_restore(LDKWindow window);

  // ---------------------------------------------------------------------------
  // Clipboard
  // ---------------------------------------------------------------------------
  /** Store UTF-8 text in the system clipboard.
   * @param window Native window handle.
   * @param text Text used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_clipboard_text_set(LDKWindow window, const char *text);

  /**
   * Read UTF-8 text from the system clipboard.
   *
   * Passing NULL and zero queries the required byte length. If out_size is too
   * small, out_text is left empty and the required byte length is returned.
   * The returned length excludes the null terminator.
   * @param window Native window handle.
   * @param out_text Destination buffer for the resulting text.
   * @param out_size Receives or specifies the byte size of the output buffer, as defined by the operation.
   * @return The resulting numeric value.
   * @group ldk.os
   */
  LDK_API size_t ldk_os_clipboard_text_get(
      LDKWindow window, char *out_text, size_t out_size);

  // ---------------------------------------------------------------------------
  // Graphics
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef void *LDKGCtx;
  /**
   * @brief Create the OS graphics context opengl.
   * @param version_major Requested major graphics API version.
   * @param version_minor Requested minor graphics API version.
   * @param color_bits Requested number of color bits.
   * @param depth_bits Requested number of depth bits.
   * @return The requested value.
   * @group ldk.os
   */
  LDK_API LDKGCtx ldk_os_graphics_context_opengl_create(
      i32 version_major, i32 version_minor, i32 color_bits, i32 depth_bits);
  /**
   * @brief Create the OS graphics context opengles.
   * @param version_major Requested major graphics API version.
   * @param version_minor Requested minor graphics API version.
   * @param color_bits Requested number of color bits.
   * @param depth_bits Requested number of depth bits.
   * @return The requested value.
   * @group ldk.os
   */
  LDK_API LDKGCtx ldk_os_graphics_context_opengles_create(
      i32 version_major, i32 version_minor, i32 color_bits, i32 depth_bits);
  /**
   * @brief Make a graphics context current for a window.
   * @param window Native window handle.
   * @param context Context used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_graphics_context_make_current(
      LDKWindow window, LDKGCtx context);
  /**
   * @brief Destroy the OS graphics context.
   * @param context Context used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_graphics_context_destroy(LDKGCtx context);
  /**
   * @brief Set the OS graphics vsync.
   * @param vsync Vsync used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_graphics_vsync_set(bool vsync);
  /**
   * @brief Query the currently configured vertical synchronization setting.
   * @return The resulting integer value.
   * @group ldk.os
   */
  LDK_API i32 ldk_os_graphics_vsync_get(void);

  // ---------------------------------------------------------------------------
  // Shared Library interface
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef void LDKLibrary;
  /**
   * @brief Load the OS library.
   * @param path Path identifying the file or resource.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.os
   */
  LDK_API LDKLibrary *ldk_os_library_load(const char *path);
  /**
   * @brief Unload the OS library.
   * @param library Library used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_library_unload(LDKLibrary *library);
  /**
   * @brief Find an exported function pointer in a dynamic library.
   * @param library Library used by this operation.
   * @param name Name identifying the object or setting.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.os
   */
  LDK_API void *ldk_os_library_fuction_ptr_get(
      LDKLibrary *library, const char *name);


  // ---------------------------------------------------------------------------
  // Process
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef struct LDKOSProcessDesc
  {
    const char *executable;
    const char *arguments;
    const char *working_directory;
    bool new_console;
  } LDKOSProcessDesc;

  /** @group ldk.os */
  typedef struct LDKOSProcessResult
  {
    bool started;
    bool completed;
    u32 exit_code;
    u32 os_error;
  } LDKOSProcessResult;

  /** @group ldk.os */
  typedef struct LDKOSProcess LDKOSProcess;

  /**
   * @brief Run a child process using the supplied process description.
   * @param desc Description or configuration for this operation.
   * @return Result of the operation.
   * @group ldk.os
   */
  LDK_API LDKOSProcessResult ldk_os_process_run(
      const LDKOSProcessDesc *desc);

  /**
   * Launches a process without waiting for it to exit and without retaining a
   * process handle. This is intended for opening external editor applications
   * rather than managed editor jobs.
   *
   * On success started is true and completed is false.
   * @param desc Description or configuration for this operation.
   * @return Result of the operation.
   * @group ldk.os
   */
  LDK_API LDKOSProcessResult ldk_os_process_launch(
      const LDKOSProcessDesc *desc);

  /**
   * Opens a file or directory with the operating system default handler.
   * @param path Path identifying the file or resource.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_open_default(const char *path);

  /**
   * Starts a process and captures stdout and stderr into one output stream.
   *
   * The returned process must be destroyed with ldk_os_process_destroy().
   * When out_result is not NULL it receives the initial process result. On
   * success started is true and completed is false. On failure NULL is returned
   * and os_error contains the platform error when available.
   * @param desc Description or configuration for this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.os
   */
  LDK_API LDKOSProcess *ldk_os_process_start(
      const LDKOSProcessDesc *desc, LDKOSProcessResult *out_result);

  /**
   * Reads currently available captured output without blocking.
   *
   * stdout and stderr are merged and returned in process order as observed by
   * the shared pipe. The returned byte count may be zero while the process is
   * still running.
   * @param process Process used by this operation.
   * @param out_buffer Destination buffer that receives the result.
   * @param buffer_size Capacity of the buffer in bytes.
   * @return The resulting numeric value.
   * @group ldk.os
   */
  LDK_API size_t ldk_os_process_output_read(
      LDKOSProcess *process, char *out_buffer, size_t buffer_size);

  /**
   * Polls the current process state without blocking.
   *
   * Returns false only when polling fails. out_result is always updated when it
   * is not NULL. completed becomes true after the child process exits.
   * @param process Process used by this operation.
   * @param out_result Receives additional status or diagnostic information.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_process_poll(
      LDKOSProcess *process, LDKOSProcessResult *out_result);

  /**
   * Cancels the process and its child process tree.
   * @param process Process used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_process_cancel(LDKOSProcess *process);

  /**
   * Returns true when cancellation was successfully requested for the process.
   * @param process Process used by this operation.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_process_was_cancelled(const LDKOSProcess *process);

  /**
   * Releases the process resources. A still-running process tree is terminated.
   * @param process Process used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_process_destroy(LDKOSProcess *process);
  

  // ---------------------------------------------------------------------------
  // Mouse
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef enum
  {
    LDK_MOUSE_BUTTON_LEFT = 0,
    LDK_MOUSE_BUTTON_RIGHT = 1,
    LDK_MOUSE_BUTTON_MIDDLE = 2,
    LDK_MOUSE_BUTTON_EXTRA_0 = 3,
    LDK_MOUSE_BUTTON_EXTRA_1 = 4,

    LDK_MOUSE_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_MOUSE_PRESSED_BIT = 1,
    LDK_MOUSE_MAX_BUTTONS = 5
  } LDKMouseButton;

  /** @group ldk.os */
  typedef enum
  {
    LDK_CURSOR_ARROW = 0,
    LDK_CURSOR_TEXT_SELECT = 1,
    LDK_CURSOR_SIZE_NWSE = 2,
    LDK_CURSOR_SIZE_NESW = 3,
    LDK_CURSOR_SIZE_WE = 4,
    LDK_CURSOR_SIZE_NS = 5,
  } LDKCursorType;

  /**
   * @brief Set the OS cursor type.
   * @param type Type identifier or kind used by the operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_cursor_type_set(LDKCursorType type);
  /**
   * @brief Get the OS cursor type.
   * @return The requested value.
   * @group ldk.os
   */
  LDK_API LDKCursorType ldk_os_cursor_type_get();
  /**
   * @brief Check whether the pointer is inside the active window client area.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_cursor_is_in_client_area();

  // LDKMouseState
  /** @group ldk.os */
  typedef struct
  {
    i32 wheel_delta;
    LDKPoint cursor;
    LDKPoint cursor_relative;
    unsigned char button[LDK_MOUSE_MAX_BUTTONS];
  } LDKMouseState;

  /**
   * @brief Get the OS mouse state.
   * @param outState Receives the requested input state snapshot.
   * @group ldk.os
   */
  LDK_API void ldk_os_mouse_state_get(LDKMouseState *outState);
  /**
   * @brief Test whether the button is currently held.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_mouse_button_is_pressed(LDKMouseState *state,
      LDKMouseButton button); // True while button is pressed
  /**
   * @brief Test whether the button was pressed during the current input update.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_mouse_button_down(LDKMouseState *state,
      LDKMouseButton button); // True in the frame the button was pressed
  /**
   * @brief Test whether the button was released during the current input update.
   * @param state Input snapshot or state object to query.
   * @param button Mouse or input button to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_mouse_button_up(LDKMouseState *state,
      LDKMouseButton button); // True in the frame the button was released
  /**
   * @brief Get the accumulated mouse wheel movement in the input snapshot.
   * @param state Input snapshot or state object to query.
   * @return The resulting integer value.
   * @group ldk.os
   */
  LDK_API i32 ldk_os_mouse_wheel_delta(LDKMouseState *state);
  /**
   * @brief Get the mouse cursor position.
   * @param state Input snapshot or state object to query.
   * @return The resulting point.
   * @group ldk.os
   */
  LDK_API LDKPoint ldk_os_mouse_cursor(LDKMouseState *state);
  /**
   * @brief Get the mouse cursor position relative to the active client area.
   * @param state Input snapshot or state object to query.
   * @return The resulting point.
   * @group ldk.os
   */
  LDK_API LDKPoint ldk_os_mouse_cursor_relative(LDKMouseState *state);

  // ---------------------------------------------------------------------------
  // Keyboard
  // ---------------------------------------------------------------------------
/** @group ldk.os */
#define LDK_KEYBOARD_KEYCODES                                                  \
  X(LDK_KEYCODE_INVALID, "", 0x00)                                             \
  X(LDK_KEYCODE_BACKSPACE, "BACK", 0x08)                                       \
  X(LDK_KEYCODE_TAB, "TAB", 0x09)                                              \
  X(LDK_KEYCODE_CLEAR, "CLEAR", 0x0C)                                          \
  X(LDK_KEYCODE_RETURN, "RETURN", 0x0D)                                        \
  X(LDK_KEYCODE_ENTER, "RETURN", 0x0D)                                         \
  X(LDK_KEYCODE_SHIFT, "SHIFT", 0x10)                                          \
  X(LDK_KEYCODE_CONTROL, "CONTROL", 0x11)                                      \
  X(LDK_KEYCODE_ALT, "ALT", 0x12)                                              \
  X(LDK_KEYCODE_PAUSE, "PAUSE", 0x13)                                          \
  X(LDK_KEYCODE_CAPITAL, "CAPITAL", 0x14)                                      \
  X(LDK_KEYCODE_ESCAPE, "ESCAPE", 0x1B)                                        \
  X(LDK_KEYCODE_CONVERT, "CONVERT", 0x1C)                                      \
  X(LDK_KEYCODE_NONCONVERT, "NONCONVERT", 0x1D)                                \
  X(LDK_KEYCODE_ACCEPT, "ACCEPT", 0x1E)                                        \
  X(LDK_KEYCODE_MODECHANGE, "MODECHANGE", 0x1F)                                \
  X(LDK_KEYCODE_SPACE, "SPACE", 0x20)                                          \
  X(LDK_KEYCODE_PRIOR, "PRIOR", 0x21)                                          \
  X(LDK_KEYCODE_NEXT, "NEXT", 0x22)                                            \
  X(LDK_KEYCODE_END, "END", 0x23)                                              \
  X(LDK_KEYCODE_HOME, "HOME", 0x24)                                            \
  X(LDK_KEYCODE_LEFT, "LEFT", 0x25)                                            \
  X(LDK_KEYCODE_UP, "UP", 0x26)                                                \
  X(LDK_KEYCODE_RIGHT, "RIGHT", 0x27)                                          \
  X(LDK_KEYCODE_DOWN, "DOWN", 0x28)                                            \
  X(LDK_KEYCODE_SELECT, "SELECT", 0x29)                                        \
  X(LDK_KEYCODE_PRINT, "PRINT", 0x2A)                                          \
  X(LDK_KEYCODE_EXECUTE, "EXECUTE", 0x2B)                                      \
  X(LDK_KEYCODE_SNAPSHOT, "SNAPSHOT", 0x2C)                                    \
  X(LDK_KEYCODE_INSERT, "INSERT", 0x2D)                                        \
  X(LDK_KEYCODE_DELETE, "DELETE", 0x2E)                                        \
  X(LDK_KEYCODE_HELP, "HELP", 0x2F)                                            \
  X(LDK_KEYCODE_0, "0", 0x30)                                                  \
  X(LDK_KEYCODE_1, "1", 0x31)                                                  \
  X(LDK_KEYCODE_2, "2", 0x32)                                                  \
  X(LDK_KEYCODE_3, "3", 0x33)                                                  \
  X(LDK_KEYCODE_4, "4", 0x34)                                                  \
  X(LDK_KEYCODE_5, "5", 0x35)                                                  \
  X(LDK_KEYCODE_6, "6", 0x36)                                                  \
  X(LDK_KEYCODE_7, "7", 0x37)                                                  \
  X(LDK_KEYCODE_8, "8", 0x38)                                                  \
  X(LDK_KEYCODE_9, "9", 0x39)                                                  \
  X(LDK_KEYCODE_A, "A", 0x41)                                                  \
  X(LDK_KEYCODE_B, "B", 0x42)                                                  \
  X(LDK_KEYCODE_C, "C", 0x43)                                                  \
  X(LDK_KEYCODE_D, "D", 0x44)                                                  \
  X(LDK_KEYCODE_E, "E", 0x45)                                                  \
  X(LDK_KEYCODE_F, "F", 0x46)                                                  \
  X(LDK_KEYCODE_G, "G", 0x47)                                                  \
  X(LDK_KEYCODE_H, "H", 0x48)                                                  \
  X(LDK_KEYCODE_I, "I", 0x49)                                                  \
  X(LDK_KEYCODE_J, "J", 0x4A)                                                  \
  X(LDK_KEYCODE_K, "K", 0x4B)                                                  \
  X(LDK_KEYCODE_L, "L", 0x4C)                                                  \
  X(LDK_KEYCODE_M, "M", 0x4D)                                                  \
  X(LDK_KEYCODE_N, "N", 0x4E)                                                  \
  X(LDK_KEYCODE_O, "O", 0x4F)                                                  \
  X(LDK_KEYCODE_P, "P", 0x50)                                                  \
  X(LDK_KEYCODE_Q, "Q", 0x51)                                                  \
  X(LDK_KEYCODE_R, "R", 0x52)                                                  \
  X(LDK_KEYCODE_S, "S", 0x53)                                                  \
  X(LDK_KEYCODE_T, "T", 0x54)                                                  \
  X(LDK_KEYCODE_U, "U", 0x55)                                                  \
  X(LDK_KEYCODE_V, "V", 0x56)                                                  \
  X(LDK_KEYCODE_W, "W", 0x57)                                                  \
  X(LDK_KEYCODE_X, "X", 0x58)                                                  \
  X(LDK_KEYCODE_Y, "Y", 0x59)                                                  \
  X(LDK_KEYCODE_Z, "Z", 0x5A)                                                  \
  X(LDK_KEYCODE_NUMPAD0, "NUMPAD0", 0x60)                                      \
  X(LDK_KEYCODE_NUMPAD1, "NUMPAD1", 0x61)                                      \
  X(LDK_KEYCODE_NUMPAD2, "NUMPAD2", 0x62)                                      \
  X(LDK_KEYCODE_NUMPAD3, "NUMPAD3", 0x63)                                      \
  X(LDK_KEYCODE_NUMPAD4, "NUMPAD4", 0x64)                                      \
  X(LDK_KEYCODE_NUMPAD5, "NUMPAD5", 0x65)                                      \
  X(LDK_KEYCODE_NUMPAD6, "NUMPAD6", 0x66)                                      \
  X(LDK_KEYCODE_NUMPAD7, "NUMPAD7", 0x67)                                      \
  X(LDK_KEYCODE_NUMPAD8, "NUMPAD8", 0x68)                                      \
  X(LDK_KEYCODE_NUMPAD9, "NUMPAD9", 0x69)                                      \
  X(LDK_KEYCODE_MULTIPLY, "MULTIPLY", 0x6A)                                    \
  X(LDK_KEYCODE_ADD, "ADD", 0x6B)                                              \
  X(LDK_KEYCODE_SEPARATOR, "SEPARATOR", 0x6C)                                  \
  X(LDK_KEYCODE_SUBTRACT, "SUBTRACT", 0x6D)                                    \
  X(LDK_KEYCODE_DECIMAL, "DECIMAL", 0x6E)                                      \
  X(LDK_KEYCODE_DIVIDE, "DIVIDE", 0x6F)                                        \
  X(LDK_KEYCODE_F1, "F1", 0x70)                                                \
  X(LDK_KEYCODE_F2, "F2", 0x71)                                                \
  X(LDK_KEYCODE_F3, "F3", 0x72)                                                \
  X(LDK_KEYCODE_F4, "F4", 0x73)                                                \
  X(LDK_KEYCODE_F5, "F5", 0x74)                                                \
  X(LDK_KEYCODE_F6, "F6", 0x75)                                                \
  X(LDK_KEYCODE_F7, "F7", 0x76)                                                \
  X(LDK_KEYCODE_F8, "F8", 0x77)                                                \
  X(LDK_KEYCODE_F9, "F9", 0x78)                                                \
  X(LDK_KEYCODE_F10, "F10", 0x79)                                              \
  X(LDK_KEYCODE_F11, "F11", 0x7A)                                              \
  X(LDK_KEYCODE_F12, "F12", 0x7B)                                              \
  X(LDK_KEYCODE_F13, "F13", 0x7C)                                              \
  X(LDK_KEYCODE_F14, "F14", 0x7D)                                              \
  X(LDK_KEYCODE_F15, "F15", 0x7E)                                              \
  X(LDK_KEYCODE_F16, "F16", 0x7F)                                              \
  X(LDK_KEYCODE_F17, "F17", 0x80)                                              \
  X(LDK_KEYCODE_F18, "F18", 0x81)                                              \
  X(LDK_KEYCODE_F19, "F19", 0x82)                                              \
  X(LDK_KEYCODE_F20, "F20", 0x83)                                              \
  X(LDK_KEYCODE_F21, "F21", 0x84)                                              \
  X(LDK_KEYCODE_F22, "F22", 0x85)                                              \
  X(LDK_KEYCODE_F23, "F23", 0x86)                                              \
  X(LDK_KEYCODE_F24, "F24", 0x87)                                              \
  X(LDK_KEYCODE_LEFT_CONTROL, "LEFT_CONTROL", 0xA2)                          \
  X(LDK_KEYCODE_RIGHT_CONTROL, "RIGHT_CONTROL", 0xA3)                        \
  X(LDK_KEYCODE_OEM1, "OEM1", 0xBA)                                            \
  X(LDK_KEYCODE_OEM2, "OEM2", 0xBF)                                            \
  X(LDK_KEYCODE_OEM3, "OEM3", 0xC0)                                            \
  X(LDK_KEYCODE_OEM4, "OEM4", 0xDB)                                            \
  X(LDK_KEYCODE_OEM5, "OEM5", 0xDC)                                            \
  X(LDK_KEYCODE_OEM6, "OEM6", 0xDD)                                            \
  X(LDK_KEYCODE_OEM7, "OEM7", 0xDE)                                            \
  X(LDK_KEYCODE_OEM8, "OEM8", 0xDF)                                            \
  X(LDK_NUM_KEYCODES, "", 0xE0)

#define X(keycode, name, value) keycode = value,
  /** @group ldk.os */
  typedef enum
  {
    LDK_KEYBOARD_KEYCODES
  } LDKKeycode;

  enum
  {
    LDK_KEYBOARD_PRESSED_BIT = 1,
    LDK_KEYBOARD_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_KEYBOARD_MAX_KEYS = 256
  };

  // LDKKeyboardState
  /** @group ldk.os */
  typedef struct
  {
    unsigned char key[LDK_KEYBOARD_MAX_KEYS];
  } LDKKeyboardState;

#undef X

  /**
   * @brief Get the OS keyboard state.
   * @param outState Receives the requested input state snapshot.
   * @group ldk.os
   */
  LDK_API void ldk_os_keyboard_state_get(LDKKeyboardState *outState);

  /**
   * @brief Test whether the key is currently held.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_keyboard_key_is_pressed(LDKKeyboardState *state,
      LDKKeycode keycode); // True while the key is pressed
  /**
   * @brief Test whether the key was pressed during the current input update.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_keyboard_key_down(LDKKeyboardState *state,
      LDKKeycode keycode); // True in the frame the key was pressed
  /**
   * @brief Test whether the key was released during the current input update.
   * @param state Input snapshot or state object to query.
   * @param keycode Keyboard key to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_keyboard_key_up(LDKKeyboardState *state,
      LDKKeycode keycode); // True in the frame the key was released

  // ---------------------------------------------------------------------------
  // Joystick
  // ---------------------------------------------------------------------------
/** @group ldk.os */
#define LDK_JOYSTICK_BUTTONS                                                   \
  X(LDK_JOYSTICK_BUTTON_DPAD_UP, "LDK_JOYSTICK_BUTTON_DPAD_UP", 0x00)          \
  X(LDK_JOYSTICK_BUTTON_DPAD_DOWN, "LDK_JOYSTICK_BUTTON_DPAD_DOWN", 0x01)      \
  X(LDK_JOYSTICK_BUTTON_DPAD_LEFT, "LDK_JOYSTICK_BUTTON_DPAD_LEFT", 0x02)      \
  X(LDK_JOYSTICK_BUTTON_DPAD_RIGHT, "LDK_JOYSTICK_BUTTON_DPAD_RIGHT", 0x03)    \
  X(LDK_JOYSTICK_BUTTON_START, "LDK_JOYSTICK_BUTTON_START", 0x04)              \
  X(LDK_JOYSTICK_BUTTON_FN1, "LDK_JOYSTICK_BUTTON_FN1", 0x04)                  \
  X(LDK_JOYSTICK_BUTTON_BACK, "LDK_JOYSTICK_BUTTON_BACK", 0x05)                \
  X(LDK_JOYSTICK_BUTTON_FN2, "LDK_JOYSTICK_BUTTON_FN2", 0x05)                  \
  X(LDK_JOYSTICK_BUTTON_LEFT_THUMB, "LDK_JOYSTICK_BUTTON_LEFT_THUMB", 0x06)    \
  X(LDK_JOYSTICK_BUTTON_RIGHT_THUMB, "LDK_JOYSTICK_BUTTON_RIGHT_THUMB", 0x07)  \
  X(LDK_JOYSTICK_BUTTON_LEFT_SHOULDER, "LDK_JOYSTICK_BUTTON_LEFT_SHOULDER",    \
      0x08)                                                                    \
  X(LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER, "LDK_JOYSTICK_BUTTON_RIGHT_SHOULDER",  \
      0x09)                                                                    \
  X(LDK_JOYSTICK_BUTTON_A, "LDK_JOYSTICK_BUTTON_A", 0x0A)                      \
  X(LDK_JOYSTICK_BUTTON_B, "LDK_JOYSTICK_BUTTON_B", 0x0B)                      \
  X(LDK_JOYSTICK_BUTTON_X, "LDK_JOYSTICK_BUTTON_X", 0x0C)                      \
  X(LDK_JOYSTICK_BUTTON_Y, "LDK_JOYSTICK_BUTTON_Y", 0x0D)                      \
  X(LDK_JOYSTICK_BUTTON_BTN1, "LDK_JOYSTICK_BUTTON1", 0x0A)                    \
  X(LDK_JOYSTICK_BUTTON_BTN2, "LDK_JOYSTICK_BUTTON2", 0x0B)                    \
  X(LDK_JOYSTICK_BUTTON_BTN3, "LDK_JOYSTICK_BUTTON3", 0x0C)                    \
  X(LDK_JOYSTICK_BUTTON_BTN4, "LDK_JOYSTICK_BUTTON4", 0x0D)

#define X(keycode, name, value) keycode = value,
  /** @group ldk.os */
  typedef enum
  {
    LDK_JOYSTICK_CHANGED_THIS_FRAME_BIT = 1 << 1,
    LDK_JOYSTICK_PRESSED_BIT = 1,
    LDK_JOYSTICK_NUM_BUTTONS = 14, // NOTICE that some entry values are repeated
                                   // expand X macro ...
    LDK_JOYSTICK_BUTTONS
  } LDKJoystickButton;
#undef X

/** @group ldk.os */
#define LDK_JOYSTICK_AXIS                                                      \
  X(LDK_JOYSTICK_AXIS_LX, "LDK_JOYSTICK_AXIS_LX", 0x00)                        \
  X(LDK_JOYSTICK_AXIS_LY, "LDK_JOYSTICK_AXIS_LY", 0x01)                        \
  X(LDK_JOYSTICK_AXIS_RX, "LDK_JOYSTICK_AXIS_RX", 0x02)                        \
  X(LDK_JOYSTICK_AXIS_RY, "LDK_JOYSTICK_AXIS_RY", 0x03)                        \
  X(LDK_JOYSTICK_AXIS_LTRIGGER, "LDK_JOYSTICK_AXIS_LTRIGGER", 0x04)            \
  X(LDK_JOYSTICK_AXIS_RTRIGGER, "LDK_JOYSTICK_AXIS_RTRIGGER", 0x05)

/** @group ldk.os */
#define LDK_JOYSTICK_MAX 4

  /** @group ldk.os */
  typedef enum
  {
    LDK_JOYSTICK_0,
    LDK_JOYSTICK_1,
    LDK_JOYSTICK_2,
    LDK_JOYSTICK_3,
  } LDKJoystickID;

#define X(keycode, name, value) keycode = value,
  /** @group ldk.os */
  typedef enum
  {
    LDK_JOYSTICK_NUM_AXIS = 6,
    LDK_JOYSTICK_AXIS
  } LDKJoystickAxis;
#undef X

  /** @group ldk.os */
  typedef struct
  {
    u32 button[LDK_JOYSTICK_NUM_BUTTONS];
    float axis[LDK_JOYSTICK_NUM_AXIS];
    bool connected;
    float vibration_left;
    float vibration_right;
  } LDKJoystickState;

  /**
   * @brief Get the OS joystick state.
   * @param outState Receives the requested input state snapshot.
   * @param id Identifier of the target object.
   * @group ldk.os
   */
  LDK_API void ldk_os_joystick_state_get(
      LDKJoystickState *outState, LDKJoystickID id);
  /**
   * @brief Test whether the button is currently held.
   * @param state Input snapshot or state object to query.
   * @param key Key or button identifier to query.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_joystick_button_is_pressed(
      LDKJoystickState *state, LDKJoystickButton key);
  /**
   * @brief Test whether the button was pressed during the current input update.
   * @param state Input snapshot or state object to query.
   * @param key Key or button identifier to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_joystick_button_down(
      LDKJoystickState *state, LDKJoystickButton key);
  /**
   * @brief Test whether the button was released during the current input update.
   * @param state Input snapshot or state object to query.
   * @param key Key or button identifier to query.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_joystick_button_up(
      LDKJoystickState *state, LDKJoystickButton key);
  /**
   * @brief Get the OS joystick axis.
   * @param state Input snapshot or state object to query.
   * @param key Key or button identifier to query.
   * @return The resulting value.
   * @group ldk.os
   */
  LDK_API float ldk_os_joystick_axis_get(
      LDKJoystickState *state, LDKJoystickAxis key);
  /**
   * @brief Count the OS joystick.
   * @return Number of matching entries.
   * @group ldk.os
   */
  LDK_API u32 ldk_os_joystick_count(void);
  /**
   * @brief Check whether the indicated joystick is connected.
   * @param id Identifier of the target object.
   * @return The resulting numeric value.
   * @group ldk.os
   */
  LDK_API u32 ldk_os_joystick_is_connected(LDKJoystickID id);

  /**
   * @brief Set the OS joystick vibration left.
   * @param id Identifier of the target object.
   * @param speed Speed used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_joystick_vibration_left_set(
      LDKJoystickID id, float speed);
  /**
   * @brief Set the OS joystick vibration right.
   * @param id Identifier of the target object.
   * @param speed Speed used by this operation.
   * @group ldk.os
   */
  LDK_API void ldk_os_joystick_vibration_right_set(
      LDKJoystickID id, float speed);

  /**
   * @brief Get the OS joystick vibration left.
   * @param id Identifier of the target object.
   * @return The resulting value.
   * @group ldk.os
   */
  LDK_API float ldk_os_joystick_vibration_left_get(LDKJoystickID id);
  /**
   * @brief Get the OS joystick vibration right.
   * @param id Identifier of the target object.
   * @return The resulting value.
   * @group ldk.os
   */
  LDK_API float ldk_os_joystick_vibration_right_get(LDKJoystickID id);

  // ---------------------------------------------------------------------------
  // Dialogs
  // ---------------------------------------------------------------------------
  /** @group ldk.os */
  typedef enum
  {
    LDK_OS_DIALOG_RESULT_NONE = 0,
    LDK_OS_DIALOG_RESULT_OK,
    LDK_OS_DIALOG_RESULT_CANCEL,
    LDK_OS_DIALOG_RESULT_YES,
    LDK_OS_DIALOG_RESULT_NO,
  } LDKOSDialogResult;

  /**
   * @brief Open a native file selection dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param filter File type filter passed to the native dialog.
   * @param out_path Receives the resulting path.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_open_file(LDKWindow owner, const char *title,
      const char *filter, XFSPath *out_path);
  /* Edit RGB in a native dialog, preserving the alpha byte of 0xRRGGBBAA.
   * Returns false on cancel/error and leaves *color unchanged. */
  /**
   * @brief Open the native color selection dialog.
   * @param owner Parent window for the dialog.
   * @param color Color value used by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_color_picker_show(
      LDKWindow owner, rgba32 *color);
  /**
   * @brief Open a native folder selection dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param filter File type filter passed to the native dialog.
   * @param out_path Receives the resulting path.
   * @param out_path_size Receives the resulting path size.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_open_folder(LDKWindow owner,
      const char *title, const char *filter, char *out_path,
      size_t out_path_size);
  /**
   * @brief Open a native save-file dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param filter File type filter passed to the native dialog.
   * @param out_path Receives the resulting path.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_save_file(LDKWindow owner, const char *title,
      const char *filter, XFSPath *out_path);
  /**
   * @brief Show a Yes/No message dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param message Message displayed to the user.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_yes_no(
      LDKWindow owner, const char *title, const char *message);
  /**
   * @brief Show an OK/Cancel message dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param message Message displayed to the user.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_ok_cancel(
      LDKWindow owner, const char *title, const char *message);
  /**
   * @brief Show an OK message dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param message Message displayed to the user.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_ok(
      LDKWindow owner, const char *title, const char *message);
  /**
   * @brief Show an error message dialog.
   * @param owner Parent window for the dialog.
   * @param title Title shown by the window or dialog.
   * @param message Message displayed to the user.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.os
   */
  LDK_API bool ldk_os_dialog_show_error(
      LDKWindow owner, const char *title, const char *message);

#ifdef __cplusplus
}
#endif

#endif // LDK_OS_H
