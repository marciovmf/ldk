#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/common.h"

// Pure engine approach
void terminate()
{
  ldkEngineStop(0);
  ldkEngineTerminate();
}

bool onKeyboardEvent(const LDKEvent* event, void* unused)
{
  if (event->type != LDK_EVENT_TYPE_KEYBOARD)
  {
    ldkLogError("Unexpected event type %d", event->type);
    return false;
  }

  const char* eventName = 
    event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_UP ? "KEY_UP" :
    event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN ? "KEY_DOWN" :
    event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_HOLD ? "KEY_HOLD" :
    "UNKNOWN";

  ldkLogDebug("Keyboard event: %s %d", eventName, event->keyboardEvent.keyCode);

  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN && event->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
  {
    terminate();
  }

  return true;
}

bool onMouseEvent(const LDKEvent* event, void* unused)
{
  const char* eventName = "";
  uint32 value = 0;

  if (event->type == LDK_EVENT_TYPE_MOUSE_WHEEL)
  {
    eventName = 
      event->mouseEvent.type == LDK_MOUSE_EVENT_WHEEL_FORWARD ? "WHEEL_FORWARD" :
      event->mouseEvent.type == LDK_MOUSE_EVENT_WHEEL_BACKWARD ? "WHEEL_BACKWARD" :
      "UNKNOWN";
    value = event->mouseEvent.wheelDelta;
  }
  else if (event->type == LDK_EVENT_TYPE_MOUSE_BUTTON)
  {
    eventName = 
      event->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_UP ? "BUTTON_UP" :
      event->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_DOWN ? "BUTTON_DOWN" :
      "UNKNOWN";
    value = event->mouseEvent.mouseButton;
  }
  else
  {
    ldkLogError("Unexpected event type %d", event->type);
    return false;
  }

  ldkLogDebug("Mouse event: %s %d", eventName, value);
  return true;
}

bool onFrameEvent(const LDKEvent* event, void* unused)
{
  if (event->frameEvent.type == LDK_FRAME_EVENT_BEFORE_RENDER)
  {
    glClear(GL_COLOR_BUFFER_BIT);
  }
  return true;
}

bool onWindowEvent(const LDKEvent* event, void* unused)
{
  if (event->windowEvent.type == LDK_WINDOW_EVENT_CLOSE)
  {
    terminate();
  }
  return true;
}

// Engine driven approach
int pureEngineApplication()
{
  ldkEngineInitialize();
  ldkGraphicsViewportTitleSet("LDK: Event test");
  ldkGraphicsViewportIconSet("../ldk.ico");
  ldkGraphicsVsyncSet(true);
  ldkGraphicsMultisamplesSet(true);
  ldkEventHandlerAdd(onKeyboardEvent, LDK_EVENT_TYPE_KEYBOARD, 0);
  ldkEventHandlerAdd(onMouseEvent,    LDK_EVENT_TYPE_MOUSE_BUTTON | LDK_EVENT_TYPE_MOUSE_WHEEL, 0);
  ldkEventHandlerAdd(onWindowEvent,   LDK_EVENT_TYPE_WINDOW, 0);
  ldkEventHandlerAdd(onFrameEvent,    LDK_EVENT_TYPE_FRAME, 0);

  LDKHShader vs = ldkAssetGet("../runtree/default.vs");
  LDKHShader fs = ldkAssetGet("../runtree/default.fs");
  LDKHMaterial material = ldkAssetGet("../runtree/default.material");
  //LDKHShaderProgram shader = ldkShaderProgramCreate(vs, fs, LDK_HANDLE_INVALID);
  //ldkShaderBind(shader);

  glClearColor(0.3f, 0.5f, 0.5f, 0.0f);
  return ldkEngineRun();
}

// Pure OS approach
bool pureOsApplication()
{
  ldkLogInitialize(0);
  ldkOsCwdSetFromExecutablePath();

  LDKGCtx context = ldkOsGraphicsContextOpenglCreate(3, 3, 24, 8);
  LDKWindow window = ldkOsWindowCreate("Window 1", 800, 600);
  ldkOsWindowTitleSet(window, "Hello, Sailor!");
  ldkOsWindowIconSet(window, "../ldk.ico");

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
    glClear(GL_COLOR_BUFFER_BIT);
    ldkOsWindowBuffersSwap(window);

    ldkOsGraphicsContextCurrent(window2, context);
    glClearColor(0, 127, 255, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    ldkOsWindowBuffersSwap(window2);
  }

  ldkOsWindowDestroy(window);
  ldkLogTerminate();
  return 0;
}

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

int testPathFunctions()
{
  LDKPath path;
  ldkOsExecutablePathFileNameGet(&path);
  ldkLogInfo("Exectuable name: %s", path.path);
  ldkOsExecutablePathGet(&path);
  ldkLogInfo("Exectuable path : %s", path.path);

  ldkOsExecutablePathFileNameGet(&path);
  LDKSubStr fileNameSubstr = ldkPathFileNameGetSubstring(path.path);
  ldkLogInfo("Exectuable path (substring) : %.*s", fileNameSubstr.length, fileNameSubstr.ptr);
  LDKSubStr fileExtentionSubstr = ldkPathFileExtentionGetSubstring(path.path);
  ldkLogInfo("Exectuable extention (substring) : %.*s", fileExtentionSubstr.length, fileExtentionSubstr.ptr);

  LDKSmallStr smallStr;

  ldkPathFileNameGet(path.path, smallStr.str, LDK_SMALL_STRING_MAX_LENGTH);
  ldkLogInfo("Exectuable path (external buffer) : %s", smallStr.str);
  ldkPathFileExtentionGet(path.path, smallStr.str, LDK_SMALL_STRING_MAX_LENGTH);
  ldkLogInfo("Exectuable extention (external buffer) : %s", smallStr.str);
  return 0;
}

int main()
{
  //return testPathFunctions();
  //return pureOsApplication();
  //return mixedModeApplication();
  return pureEngineApplication();
}

