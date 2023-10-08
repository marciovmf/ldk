#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/common.h"

//TODO: Create Events for Joystick button and axis input

typedef struct
{
  LDKHMaterial material;
  LDKHMesh mesh;
} GameState;

// Pure engine approach
bool onKeyboardEvent(const LDKEvent* event, void* data)
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

  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN
      && event->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
  {
       ldkEngineStop(0);
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

bool onWindowEvent(const LDKEvent* event, void* state)
{
  if (event->windowEvent.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldkEngineStop(0);
  }
  else if (event->windowEvent.type == LDK_WINDOW_EVENT_RESIZED
  || event->windowEvent.type == LDK_WINDOW_EVENT_MAXIMIZED)
  {
    uint32 width = event->windowEvent.width;
    uint32 height = event->windowEvent.height;
    glViewport(0, 0, width, height);
  }
  return true;
}

bool onFrameEvent(const LDKEvent* event, void* data)
{
  if (event->frameEvent.type == LDK_FRAME_EVENT_BEFORE_RENDER)
  {
    GameState* state = (GameState*) data;
    glClear(GL_COLOR_BUFFER_BIT);
    LDKVertexBuffer vBuffer = ldkAssetMeshGetVertexBuffer(state->mesh);
    ldkMaterialBind(state->material);
    ldkVertexBufferBind(vBuffer);
    ldkRenderMesh(vBuffer, 6, 0);
    ldkMaterialBind(0);
    ldkVertexBufferBind(0);
  }
  return true;
}

// Engine driven approach
int pureEngineApplication()
{
  GameState state = {0};
  ldkEngineInitialize();
  ldkGraphicsViewportTitleSet("LDK: Event test");
  ldkGraphicsViewportIconSet("../ldk.ico");
  ldkGraphicsVsyncSet(true);
  ldkGraphicsMultisamplesSet(true);
  ldkEventHandlerAdd(onKeyboardEvent, LDK_EVENT_TYPE_KEYBOARD, 0);
  ldkEventHandlerAdd(onMouseEvent,    LDK_EVENT_TYPE_MOUSE_WHEEL | LDK_EVENT_TYPE_MOUSE_BUTTON, 0);
  ldkEventHandlerAdd(onWindowEvent,   LDK_EVENT_TYPE_WINDOW, 0);
  ldkEventHandlerAdd(onFrameEvent,    LDK_EVENT_TYPE_FRAME, (void*) &state);

  state.material = ldkAssetGet("../runtree/default.material");
  state.mesh = ldkAssetGet("../runtree/default.mesh");

  glClearColor(0.3f, 0.5f, 0.5f, 0.0f);
  return ldkEngineRun();
}

// Pure OS approach
bool pureOsApplication()
{
  ldkOsInitialize();
  ldkLogInitialize(0, "-- Start --");
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
  ldkOsTerminate();
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

