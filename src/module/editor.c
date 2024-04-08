#include "ldk/os.h"
#include "ldk/eventqueue.h"
#include "ldk/module/editor.h"
#include "ldk/module/renderer.h"
#include "ldk/entity/staticobject.h"
#include "ldk/entity/camera.h"
#include "ldk/asset/material.h"
#include "ldk/asset/mesh.h"
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



bool ldkEditorInitialize(void)
{
  ldkEventHandlerAdd(onUpdate, LDK_EVENT_TYPE_NONE, NULL); 
  ldkEventHandlerAdd(onKeyboard, LDK_EVENT_TYPE_NONE, NULL);

  internalEditor.enabled = false;

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  ldkSmallString(&camera->entity.name, "Editor Camera");
  camera->position = vec3Zero();
  camera->target = vec3(0.0f, 0.0f, -1.0f);
  internalEditor.editorCamera = camera;

  //
  // Default materials
  //
  LDKMaterial* defaultMaterial = ldkAssetGet(LDKMaterial, "assets/default.material");
  LDKMaterial* materialRed = ldkMaterialClone(defaultMaterial->asset.handle);
  materialRed->enableDepthTest = false;
  ldkMaterialParamSetVec3(materialRed, "color", vec3(1.0f, 0.0f, 0.0f));
  ldkMaterialParamSetFloat(materialRed, "colorIntensity", 1.0f);

  LDKMaterial* materialGreen = ldkMaterialClone(defaultMaterial->asset.handle);
  materialGreen->enableDepthTest = false;
  ldkMaterialParamSetVec3(materialGreen, "color", vec3(0.0f, 1.0f, 0.0f));
  ldkMaterialParamSetFloat(materialGreen, "colorIntensity", 1.0f);

  LDKMaterial* materialBlue = ldkMaterialClone(defaultMaterial->asset.handle);
  materialBlue->enableDepthTest = false;
  ldkMaterialParamSetVec3(materialBlue, "color", vec3(0.0f, 0.0f, 1.0f));
  ldkMaterialParamSetFloat(materialBlue, "colorIntensity", 1.0f);


  return true;
}

void ldkEditorTerminate(void)
{
  ldkEventHandlerRemove(onUpdate);
  ldkEventHandlerRemove(onKeyboard);
  internalEditor.enabled = false;
}

