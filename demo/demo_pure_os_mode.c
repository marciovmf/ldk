#include "ldk/ldk.h"

bool pureOsApplication()
{
  ldkOsInitialize();
  ldkLogInitialize(0, "-- Start --");
  ldkOsCwdSetFromExecutablePath();

  LDKGCtx context = ldkOsGraphicsContextOpenglCreate(3, 3, 24, 8);

  LDKWindow window = ldkOsWindowCreate("Window 1", 800, 600);
  ldkOsWindowTitleSet(window, "Hello, Sailor!");
  ldkOsWindowIconSet(window, "assets/ldk.ico");

  ldkGraphicsVsyncSet(true);

  LDKWindow window2 = ldkOsWindowCreate("Window 2", 800, 600);
  ldkOsWindowTitleSet(window2, "Hello, Friend!");

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

    ldkOsGraphicsContextCurrent(window, context);
    glClearColor(0, 0, 255, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ldkOsWindowBuffersSwap(window);

    ldkOsGraphicsContextCurrent(window2, context);
    glClearColor(0, 127, 255, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    ldkOsWindowBuffersSwap(window2);
  }

  ldkOsWindowDestroy(window);
  ldkLogTerminate();
  ldkOsTerminate();
  return 0;
}

int main()
{
  return pureOsApplication();
}

