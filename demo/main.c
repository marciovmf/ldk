#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/maths.h"
#include "ldk/common.h"
#include "ldk/maths.h"
#include <math.h>

typedef struct
{
  LDKHMaterial material;
  LDKHMesh mesh;
  LDKCamera* camera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool looking;
  bool strafing;
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
  const float speed = 0.001f * state->deltaTime;
  const float lookSpeed = 0.0001f * state->deltaTime;

  if (event->type == LDK_EVENT_TYPE_MOUSE_BUTTON)
  {
    if (event->mouseEvent.mouseButton == LDK_MOUSE_BUTTON_RIGHT)
    {
      if (event->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_UP)
        state->looking = false;
      else
        state->looking = true;
    }

    if (event->mouseEvent.mouseButton == LDK_MOUSE_BUTTON_MIDDLE)
    {
      if (event->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_UP)
        state->strafing = false;
      else
        state->strafing = true;
    }
  }

  Vec3 cam_dir = ldkCameraDirectionNormalized(state->camera);
  Vec3 side_dir = vec3Normalize(vec3Cross(cam_dir, vec3Up()));
  Vec3 cam_up = vec3Normalize(vec3Cross(cam_dir, side_dir));

  if (state->looking)
  {
    cam_dir.y += -(event->mouseEvent.yRel * speed);

    cam_dir = vec3Add(cam_dir, vec3Mul(side_dir, (event->mouseEvent.xRel * lookSpeed)));
    state->camera->target = vec3Add(state->camera->position, vec3Normalize(cam_dir));
  }


  if (state->strafing)
  {
    if (abs(event->mouseEvent.yRel) > abs(event->mouseEvent.xRel))
    {
      if (event->mouseEvent.yRel < 0)
      {
        state->camera->position = vec3Add(state->camera->position, vec3Mul(cam_up, speed));
        state->camera->target = vec3Add(state->camera->target, vec3Mul(cam_up, speed));
      }
      else if (event->mouseEvent.yRel > 0)
      {
        state->camera->position = vec3Sub(state->camera->position, vec3Mul(cam_up, speed));
        state->camera->target = vec3Sub(state->camera->target, vec3Mul(cam_up, speed));
      }
    }
    else
    {
      if (event->mouseEvent.xRel < 0)
      {
        state->camera->position = vec3Add(state->camera->position, vec3Mul(side_dir, speed));
        state->camera->target = vec3Add(state->camera->target, vec3Mul(side_dir, speed));
      }
      else if (event->mouseEvent.xRel > 0)
      {
        state->camera->position = vec3Sub(state->camera->position, vec3Mul(side_dir, speed));
        state->camera->target = vec3Sub(state->camera->target, vec3Mul(side_dir, speed));
      }
    }
  }

  if (event->type == LDK_EVENT_TYPE_MOUSE_WHEEL)
  {
    if (event->mouseEvent.wheelDelta > 0)
    {
      state->camera->position = vec3Add(state->camera->position, vec3Mul(cam_dir, 3 * speed));
      state->camera->target = vec3Add(state->camera->target, vec3Mul(cam_dir, 3 * speed));
    }
    else
    {
      state->camera->position = vec3Sub(state->camera->position, vec3Mul(cam_dir, 3 * speed));
      state->camera->target = vec3Sub(state->camera->target, vec3Mul(cam_dir, 3 * speed));
    }
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

int main()
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

