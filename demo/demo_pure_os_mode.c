#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#define X_IMPL_LOG
#define X_IMPL_HASHTABLE
#define X_IMPL_HPOOL
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#define X_IMPL_FILESYSTEM
#include <ldk_game.h>

bool pureOsApplication()
{
  x_fs_cwd_set_from_executable_path();

  ldk_engine_initialize();

  LDKGCtx context = ldk_os_graphics_context_opengl_create(3, 3, 24, 8);

  LDKWindow window = ldk_os_window_create("Window 1", 800, 600);
  ldk_os_window_title_set(window, "Hello, Sailor!");
  ldk_os_window_icon_set(window, "assets/ldk.ico");

  ldk_os_graphics_vsync_set(true);

  LDKWindow window2 = ldk_os_window_create("Window 2", 800, 600);
  ldk_os_window_title_set(window2, "Hello, Friend!");

  bool running = true;

  while (running)
  {
    LDKEvent e;
    while(ldk_os_events_poll(&e))
    {
      if (e.type == LDK_EVENT_TYPE_KEYBOARD)
      {
        if (e.keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN &&
            e.keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
        {
          ldk_log_debug("ESC Pressed!");
          running = false;
          break;
        }
      }
    }

    ldk_os_graphics_context_current(window, context);
    glClearColor(0, 0, 255, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ldk_os_window_buffers_swap(window);

    ldk_os_graphics_context_current(window2, context);
    glClearColor(0, 127, 255, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    ldk_os_window_buffers_swap(window2);
  }

  ldk_os_window_destroy(window);
  ldk_engine_terminate();
  return 0;
}

int main()
{
  return pureOsApplication();
}

