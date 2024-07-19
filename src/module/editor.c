#include "hlist.h"
#include "ldk/os.h"
#include "ldk/eventqueue.h"
#include "ldk/module/editor.h"
#include "ldk/module/renderer.h"
#include "ldk/entity/camera.h"
#include "ldk/common.h"
#include "ldk/engine.h"
#include "maths.h"
#include "module/entity.h"
#include <math.h>

static struct 
{
  LDKHandle editorCamera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool enabled;

} internalEditor = {0};

extern void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color);

extern void drawFrustum(float nearWidth, float nearHeight, float farWidth, float farHeight, float nearDist, float farDist, LDKRGB color);

static bool onKeyboard(const LDKEvent* evt, void* data)
{
  if (evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
    {
      ldkEngineStop(0);
    }
    else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_X)
    {
    }
  }
  return true;
}

static bool onUpdate(const LDKEvent* evt, void* data)
{
  LDKCamera* camera = ldkEntityLookup(LDKCamera, internalEditor.editorCamera);
  float deltaTime = evt->frameEvent.deltaTime;
  ldkCameraUpdateFreeCamera(camera, deltaTime, 30, 10);
  ldkRendererSetCamera(camera);
  return true;
}

void  ldkEditorImmediateDraw(float deltaTime)
{
  Mat4 identity = mat4Id();
  LDKCamera* camera = ldkEntityLookup(LDKCamera, internalEditor.editorCamera);
  Mat4 cameraViewMatrix = ldkCameraViewMatrix(camera);
  Mat4 cameraProjMatrix = ldkCameraProjectMatrix(camera);
  LDKShader* shader = ldkAssetGet(LDKShader, "assets/default_vertex_color.shader");

  //
  // Draw Axis lines
  //

  ldkShaderProgramBind(shader);
  ldkShaderParamSetMat4(shader, "mModel", identity);
  ldkShaderParamSetMat4(shader, "mView", cameraViewMatrix);
  ldkShaderParamSetMat4(shader, "mProj", cameraProjMatrix);

  const float cameraX = camera->target.x;
  const float cameraY = camera->target.y;
  const float cameraZ = camera->target.z;
  const float lineLength = camera->farPlane * 100;
  const float thickness = 1.4f;

  drawLine(cameraX - lineLength, 0.0f, 0.0f, cameraX + lineLength, 0.0f, 0.0f, thickness, ldkRGB(255, 0, 0));
  drawLine(0.0f, cameraY - lineLength, 0.0f, 0.0f, cameraY + lineLength, 0.0f, thickness, ldkRGB(0, 255, 0));
  drawLine(0.0f, 0.0f, cameraZ - lineLength, 0.0f, 0.0f, cameraZ + lineLength, thickness, ldkRGB(0, 0, 255));

  //
  // Draw Camera Gizmos
  //
  ldkShaderParamSetVec3(shader, "color", vec3(1.0f, 1.0f, 1.0f));
  LDKHListIterator it = ldkEntityManagerGetIterator(LDKCamera);
  while (ldkHListIteratorNext(&it))
  {
    LDKCamera* e = (LDKCamera*) ldkHListIteratorCurrent(&it);
    if ((e->entity.flags & LDK_ENTITY_FLAG_INTERNAL) == LDK_ENTITY_FLAG_INTERNAL)
      continue;
    Mat4 world = mat4World(e->position, vec3One(), quatInverse(mat4ToQuat(ldkCameraViewMatrix(e))));
    ldkShaderParamSetMat4(shader, "mModel", world);
    drawFrustum(0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1.5f, ldkRGB(255, 255, 255));
  }
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
  internalEditor.editorCamera = camera->entity.handle;

  return true;
}

void ldkEditorTerminate(void)
{
  ldkEventHandlerRemove(onUpdate);
  ldkEventHandlerRemove(onKeyboard);
  internalEditor.enabled = false;
}

