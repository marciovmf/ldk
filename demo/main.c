#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/module/entity.h"
#include "ldk/entity/staticobject.h"
#include "ldk/common.h"
#include "ldk/maths.h"
#include <math.h>
#include <stdlib.h>

typedef struct
{
  LDKMaterial material;
  LDKHandle hCamera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool looking;
  bool strafing;
} GameState;

bool onKeyboardEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  if (event->type != LDK_EVENT_TYPE_KEYBOARD)
  {
    ldkLogError("Unexpected event type %d", event->type);
    return false;
  }

  if (event->keyboardEvent.type != LDK_KEYBOARD_EVENT_KEY_UP)
  {
    LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);

    //Vec3 cam_dir = vec3Normalize(vec3Sub(state->camera->target, state->camera->position));
    Vec3 cam_dir = ldkCameraDirectionNormalized(camera);
    Vec3 side_dir = vec3Normalize(vec3Cross(cam_dir, vec3Up()));
    const float speed = 5.0f * state->deltaTime;

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_W)
    {
      camera->position = vec3Add(camera->position, vec3Mul(cam_dir, speed));
      camera->target = vec3Add(camera->target, vec3Mul(cam_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_S)
    {
      camera->position = vec3Sub(camera->position, vec3Mul(cam_dir, speed));
      camera->target = vec3Sub(camera->target, vec3Mul(cam_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_D)
    {
      camera->position = vec3Add(camera->position, vec3Mul(side_dir, speed));
      camera->target = vec3Add(camera->target, vec3Mul(side_dir, speed));
    }

    if (event->keyboardEvent.keyCode == LDK_KEYCODE_A)
    {
      camera->position = vec3Sub(camera->position, vec3Mul(side_dir, speed));
      camera->target = vec3Sub(camera->target, vec3Mul(side_dir, speed));
    }
  }

  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_UP
      && event->keyboardEvent.keyCode == LDK_KEYCODE_X)
  {
    LDKHandle targetEntity = ldkRendererSelectedEntity();
    if (targetEntity != LDK_HANDLE_INVALID)
    {
      ldkLogInfo("Destroying entity %llx", targetEntity);
      ldkEntityDestroy(targetEntity);
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
  const float speed = 5.0f * state->deltaTime;

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

  LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);

  Vec3 cam_dir = ldkCameraDirectionNormalized(camera);
  Vec3 side_dir = vec3Normalize(vec3Cross(cam_dir, vec3Up()));
  Vec3 cam_up = vec3Normalize(vec3Cross(cam_dir, side_dir));

  if (state->looking)
  {
    cam_dir.y += -(event->mouseEvent.yRel * state->deltaTime * speed);

    cam_dir = vec3Add(cam_dir, vec3Mul(side_dir, (event->mouseEvent.xRel* state->deltaTime * speed)));
    camera->target = vec3Add(camera->position, vec3Normalize(cam_dir));
  }

  if (state->strafing)
  {
    if (abs(event->mouseEvent.yRel) > abs(event->mouseEvent.xRel))
    {
      if (event->mouseEvent.yRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(cam_up, speed));
        camera->target = vec3Add(camera->target, vec3Mul(cam_up, speed));
      }
      else if (event->mouseEvent.yRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(cam_up, speed));
        camera->target = vec3Sub(camera->target, vec3Mul(cam_up, speed));
      }
    }
    else
    {
      if (event->mouseEvent.xRel < 0)
      {
        camera->position = vec3Add(camera->position, vec3Mul(side_dir, speed));
        camera->target = vec3Add(camera->target, vec3Mul(side_dir, speed));
      }
      else if (event->mouseEvent.xRel > 0)
      {
        camera->position = vec3Sub(camera->position, vec3Mul(side_dir, speed));
        camera->target = vec3Sub(camera->target, vec3Mul(side_dir, speed));
      }
    }
  }

  if (event->type == LDK_EVENT_TYPE_MOUSE_WHEEL)
  {
    if (event->mouseEvent.wheelDelta > 0)
    {
      camera->position = vec3Add(camera->position, vec3Mul(cam_dir, 3 * speed));
      camera->target = vec3Add(camera->target, vec3Mul(cam_dir, 3 * speed));
    }
    else
    {
      camera->position = vec3Sub(camera->position, vec3Mul(cam_dir, 3 * speed));
      camera->target = vec3Sub(camera->target, vec3Mul(cam_dir, 3 * speed));
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

  LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);

  if (event->frameEvent.type == LDK_FRAME_EVENT_BEFORE_RENDER)
  {
    double delta = (float) ldkOsTimeTicksIntervalGetMilliseconds(state->ticksStart, state->ticksEnd);
    state->deltaTime = (float) delta / 1000;
    state->ticksStart = ldkOsTimeTicksGet();

    ldkRendererCameraSet(camera);

    uint32 numObjects = 0;
    LDKStaticObject* allStaticObjects = ldkEntityGetAll(LDKStaticObject, &numObjects);
    for (uint32 i = 0; i < numObjects; i++)
      ldkRendererAddStaticObject(&allStaticObjects[i]);

    ldkRendererRender(state->deltaTime);
  }
  else if (event->frameEvent.type == LDK_FRAME_EVENT_AFTER_RENDER)
  {
    state->ticksEnd = ldkOsTimeTicksGet();
#if 0
    if (camera)
      ldkLogInfo("Camera position = %f, %f, %f", camera->position.x, camera->position.y, camera->position.z);
#endif
  }
  return true;
}

int main(void)
{
  // Initialize stuff
  GameState state = {0};
  ldkEngineInitialize();
  ldkGraphicsViewportTitleSet("LDK demo game");
  ldkGraphicsViewportIconSet("../ldk.ico");
  ldkGraphicsVsyncSet(true);
  ldkGraphicsMultisamplesSet(true);

  // Bind events
  ldkEventHandlerAdd(onKeyboardEvent, LDK_EVENT_TYPE_KEYBOARD, (void*) &state);
  ldkEventHandlerAdd(onMouseEvent,    LDK_EVENT_TYPE_MOUSE_WHEEL | LDK_EVENT_TYPE_MOUSE_BUTTON | LDK_EVENT_TYPE_MOUSE_MOVE, (void*) &state);
  ldkEventHandlerAdd(onWindowEvent,   LDK_EVENT_TYPE_WINDOW, 0);
  ldkEventHandlerAdd(onFrameEvent,    LDK_EVENT_TYPE_FRAME, (void*) &state);

  //
  // Create some entities
  //

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  camera->position = vec3(-19.0f, 7.0f, -1.0f);


  for(uint32 i = 0; i < 10; i++)
  {
    LDKStaticObject* obj = ldkEntityCreate(LDKStaticObject);
    LDKMesh* mesh = ldkAssetGet(LDKMesh, "assets/dock.mesh");
    obj->mesh = mesh->asset.handle;

    float f = 3.0f + (0.5f * i);
    obj->scale = vec3(f, f, f);
    obj->position.z = -25.0f + (i * 5);
    obj->position.z = -25.0f + i * f;
  }

  // We should never keep pointers to entities. Intead, we keep their Handle
  state.hCamera = camera->entity.handle;

  return ldkEngineRun();
}

