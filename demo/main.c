#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/maths.h"
#include "ldk/common.h"

//TODO: Create Events for Joystick button and axis input

typedef struct
{
  LDKHMaterial material;
  LDKHMesh mesh;
  LDKCamera* camera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool looking;
} GameState;

// Pure engine approach
bool onKeyboardEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  if (event->type != LDK_EVENT_TYPE_KEYBOARD)
  {
    ldkLogError("Unexpected event type %d", event->type);
    return false;
  }

  if ( event->keyboardEvent.type != LDK_KEYBOARD_EVENT_KEY_UP)
  {
    //Vec3 cam_dir = vec3Normalize(vec3Sub(state->camera->target, state->camera->position));
    Vec3 cam_dir = ldkCameraDirectionNormalized(state->camera);
    Vec3 side_dir = vec3Normalize(vec3Cross(cam_dir, vec3Up()));
    const float speed = 0.001f * state->deltaTime;

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_W)
    {
      state->camera->position = vec3Add(state->camera->position, vec3Mul(cam_dir, speed));
      state->camera->target = vec3Add(state->camera->target, vec3Mul(cam_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_S)
    {
      state->camera->position = vec3Sub(state->camera->position, vec3Mul(cam_dir, speed));
      state->camera->target = vec3Sub(state->camera->target, vec3Mul(cam_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_D)
    {
      state->camera->position = vec3Add(state->camera->position, vec3Mul(side_dir, speed));
      state->camera->target = vec3Add(state->camera->target, vec3Mul(side_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_A)
    {
      state->camera->position = vec3Sub(state->camera->position, vec3Mul(side_dir, speed));
      state->camera->target = vec3Sub(state->camera->target, vec3Mul(side_dir, speed));
    }

  }

  // Stop on ESC
  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN
      && event->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
  {
       ldkEngineStop(0);
  }

  return true;
}

bool onMouseEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;

  if (event->type == LDK_EVENT_TYPE_MOUSE_BUTTON && event->mouseEvent.mouseButton == LDK_MOUSE_BUTTON_LEFT)
  {
    if (event->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_UP)
      state->looking = false;
    else
      state->looking = true;
  }

  if (state->looking)
  {
    const float speed = 0.0001f * state->deltaTime;
    Vec3 cam_dir = ldkCameraDirectionNormalized(state->camera);
    cam_dir.y += -(event->mouseEvent.yRel * speed);

    Vec3 side_dir = vec3Cross(cam_dir, vec3Up());
    cam_dir = vec3Add(cam_dir, vec3Mul(side_dir, (event->mouseEvent.xRel * speed)));
    state->camera->target = vec3Add(state->camera->position, vec3Normalize(cam_dir));
  }

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
  GameState* state = (GameState*) data;

  if (event->frameEvent.type == LDK_FRAME_EVENT_BEFORE_RENDER)
  {
    double delta = (float) ldkOsTimeTicksIntervalGetMilliseconds(state->ticksStart, state->ticksEnd);
    state->deltaTime = (float) delta;
    state->ticksStart = ldkOsTimeTicksGet();

    ldkRendererCameraSet(state->camera);
    ldkRendererAddStaticMesh(state->mesh);
    ldkRendererRender();
  }
  else if (event->frameEvent.type == LDK_FRAME_EVENT_AFTER_RENDER)
  {
    state->ticksEnd = ldkOsTimeTicksGet();
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
  ldkEventHandlerAdd(onKeyboardEvent, LDK_EVENT_TYPE_KEYBOARD, (void*) &state);
  ldkEventHandlerAdd(onMouseEvent,    LDK_EVENT_TYPE_MOUSE_WHEEL | LDK_EVENT_TYPE_MOUSE_BUTTON | LDK_EVENT_TYPE_MOUSE_MOVE, (void*) &state);
  ldkEventHandlerAdd(onWindowEvent,   LDK_EVENT_TYPE_WINDOW, 0);
  ldkEventHandlerAdd(onFrameEvent,    LDK_EVENT_TYPE_FRAME, (void*) &state);

  state.mesh = ldkAssetGet("assets/dock.mesh");

  state.camera = ldkCameraCreate();
  state.camera->position = vec3(0.0f, 1.0f, 2.0f);

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

