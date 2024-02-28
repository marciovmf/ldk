#include "ldk/ldk.h"
#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/module/entity.h"
#include "ldk/entity/staticobject.h"
#include "ldk/common.h"
#include "ldk/asset/config.h"
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
  float cameraLookSpeed;
  float cameraMoveSpeed;
} GameState;

bool onKeyboardEvent(const LDKEvent* event, void* data)
{
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

    ldkCameraUpdateFreeCamera(camera, state->deltaTime, state->cameraLookSpeed, state->cameraMoveSpeed);
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
  }
  return true;
}

int main(void)
{
  // Initialize stuff
  GameState state = {0};
  ldkEngineInitialize();

  // Bind events
  ldkEventHandlerAdd(onKeyboardEvent, LDK_EVENT_TYPE_KEYBOARD, (void*) &state);
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

   LDKConfig* cfg = ldkAssetGet(LDKConfig, "ldk.cfg");
   state.cameraMoveSpeed = ldkConfigGetFloat(cfg, "game.camera-move-speed");
   state.cameraLookSpeed = ldkConfigGetFloat(cfg, "game.camera-look-speed");

  // We should never keep pointers to entities. Intead, we keep their Handle
  state.hCamera = camera->entity.handle;

  return ldkEngineRun();
}

