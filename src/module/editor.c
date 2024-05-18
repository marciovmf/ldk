#include "ldk/os.h"
#include "ldk/eventqueue.h"
#include "ldk/module/editor.h"
#include "ldk/module/renderer.h"
#include "ldk/entity/camera.h"
#include "ldk/common.h"
#include "ldk/engine.h"
#include <math.h>

static struct 
{
  LDKCamera* editorCamera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool enabled;

} internalEditor = {0};

static bool onKeyboard(const LDKEvent* evt, void* data)
{
  if (evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
    {
      ldkEngineStop(0);
    }
  }
  return true;
}

static bool onUpdate(const LDKEvent* evt, void* data)
{
  float deltaTime = evt->frameEvent.deltaTime;
  ldkCameraUpdateFreeCamera(internalEditor.editorCamera, deltaTime, 30, 10);
  ldkRendererCameraSet(internalEditor.editorCamera);
  return true;
}

void ldkEditorEnable(bool enabled)
{
  internalEditor.enabled = enabled;

  if (internalEditor.enabled)
  {
    ldkLogInfo("EDITOR mode");
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_FRAME_BEFORE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_KEYBOARD);
  }
  else
  {
    ldkLogInfo("GAME mode");
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_NONE);
  }
}

bool ldkEditorIsEnabled(void)
{
  return internalEditor.enabled;
}

bool ldkEditorInitialize(void)
{
  ldkEventHandlerAdd(onUpdate, LDK_EVENT_TYPE_NONE, NULL); 
  ldkEventHandlerAdd(onKeyboard, LDK_EVENT_TYPE_NONE, NULL);

  internalEditor.enabled = false;

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  ldkSmallString(&camera->entity.name, "Editor Camera");
  camera->position = vec3(0.0f, 5.0f, 10.0f);
  camera->target = vec3Zero();
  camera->entity.flags = LDK_ENTITY_FLAG_INTERNAL;
  internalEditor.editorCamera = camera;

  return true;
}

void ldkEditorTerminate(void)
{
  ldkEventHandlerRemove(onUpdate);
  ldkEventHandlerRemove(onKeyboard);
  internalEditor.enabled = false;
}

