#include "ldk/ldk.h"
#include "ldk/common.h"

// Mixed approach
int mixedModeApplication()
{
  ldkEngineInitialize();
  ldkGraphicsViewportTitleSet("LDK");
  ldkGraphicsViewportIconSet("ldk.ico");
  glClearColor(0, 0, 255, 0);

  bool running = true;
  while (running)
  {
    LDKEvent e;
    while(ldkOsEventsPoll(&e))
    {
      if (e.type == LDK_EVENT_TYPE_KEYBOARD)
      {
        if (e.keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN
            && e.keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
        {
          ldkLogDebug("ESC Pressed!");
          running = false;
          break;
        }
      }
    }
    // Some manual opengl stuff...
    glClear(GL_COLOR_BUFFER_BIT);
    ldkGraphicsSwapBuffers();
  }
  ldkEngineTerminate();
  return 0;
}

int main()
{
  return mixedModeApplication();
}

