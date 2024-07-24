#include "asset/material.h"
#include "entity/staticobject.h"
#include "array.h"
#include "asset/mesh.h"
#include "hlist.h"
#include "ldk/os.h"
#include "ldk/eventqueue.h"
#include "ldk/module/editor.h"
#include "ldk/module/renderer.h"
#include "ldk/entity/camera.h"
#include "ldk/entity/pointlight.h"
#include "ldk/entity/spotlight.h"
#include "ldk/entity/directionallight.h"
#include "ldk/common.h"
#include "ldk/asset/mesh.h"
#include "ldk/asset/shader.h"
#include "ldk/engine.h"
#include "maths.h"
#include "module/rendererbackend.h"
#include "module/asset.h"
#include "module/entity.h"
#include <math.h>

enum
{
  EDITOR_FLAG_ENTITY_PLACEHOLDER = 1, // The entity was used as a placeholder for another entity
};


typedef struct
{
  LDKHEntity placeholder;
  LDKHEntity entity;
} PlaceholderEntity;

static struct 
{
  LDKHEntity editorCamera;
  LDKArray* editorEntities;
  LDKMesh* quadMesh;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  bool enabled;
  LDKMesh* quad;
  LDKHAsset editorIconMaterial;
  LDKHAsset* overridenMaterialArray;
  LDKHEntity selectedEntity;
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

// Called every frame if the editor is enable
static bool onUpdate(const LDKEvent* evt, void* data)
{
  LDKHEntity selected = ldkRendererSelectedEntity();
  if (!ldkHandleEquals(internalEditor.selectedEntity, selected))
  {
    LDKEntityInfo* e = ldkEntityManagerFind(selected);
    if (!ldkHandleIsValid(selected))
    {
      ldkLogInfo("Nothing selected");
    }
    else if (e && e->typeId == typeid(LDKStaticObject) && e->flags == EDITOR_FLAG_ENTITY_PLACEHOLDER) 
    {
      LDKHEntity* h = (LDKHEntity*) ldkArrayGetData(internalEditor.editorEntities);
      uint32 numEntities = ldkArrayCount(internalEditor.editorEntities);
      for (uint32 i = 0; i < numEntities; i++)
      {
        if(ldkHandleEquals(h[i], selected))
        {
          e = ldkEntityManagerFind(e->editorPlaceholder);
          break;
        }
      }
    }
    ldkLogInfo("-- Editor: Entity selected %llx %s", 
        (e ? e->handle : LDK_HENTITY_INVALID),
        (e ? e->name.str : "Nothing"));
  }
  internalEditor.selectedEntity = selected;

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
    if (e->entity.handle.value == internalEditor.editorCamera.value)
      continue;
    Mat4 world = mat4World(e->position, vec3One(), quatInverse(mat4ToQuat(ldkCameraViewMatrix(e))));
    ldkShaderParamSetMat4(shader, "mModel", world);
    drawFrustum(0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1.5f, ldkRGB(255, 255, 255));
  }
}

static void internalAddPlaceholderEntity(Vec3 position, LDKHAsset* materialOverrideList, LDKHEntity targetEntity)
{
  LDKStaticObject* icon = ldkEntityCreate(LDKStaticObject);
  icon->mesh = ldkAssetGet(LDKMesh, "assets/box.mesh")->asset.handle;
  icon->materials = materialOverrideList;
  icon->position = position;
  icon->scale = vec3(0.2f, 0.2f, 0.2f);
  icon->entity.editorPlaceholder = targetEntity;
  icon->entity.flags = EDITOR_FLAG_ENTITY_PLACEHOLDER;
  ldkSmallStringFormat(&icon->entity.name, "editor-%llx", targetEntity);
  ldkArrayAdd(internalEditor.editorEntities, &icon->entity.handle);
  ldkRendererAddStaticObject(icon); 
}

// Called right after enabling/disabling the editor
void ldkEditorEnable(bool enabled)
{
  internalEditor.enabled = enabled;
  if (internalEditor.enabled)
  {

    ldkLogInfo("EDITOR mode");
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_FRAME_BEFORE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_KEYBOARD);

    // =====================================================
    // Add 3D icons for 'invisible' entities
    // =====================================================
    LDKHListIterator it;

    it = ldkEntityManagerGetIterator(LDKDirectionalLight);
    while(ldkHListIteratorNext(&it)) {
      LDKDirectionalLight* e = (LDKDirectionalLight*) ldkHListIteratorCurrent(&it);
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKPointLight);
    while(ldkHListIteratorNext(&it)) 
    {
      LDKPointLight* e = (LDKPointLight*) ldkHListIteratorCurrent(&it);
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKSpotLight);
    while(ldkHListIteratorNext(&it)) {
      LDKSpotLight* e = (LDKSpotLight*) ldkHListIteratorCurrent(&it);
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKCamera);
    while(ldkHListIteratorNext(&it)) {
      LDKCamera* e = (LDKCamera*) ldkHListIteratorCurrent(&it);
      // Skip the editor camera
      if (e->entity.handle.value == internalEditor.editorCamera.value)
        continue;
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }
  }
  else
  {
    ldkLogInfo("GAME mode");
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_NONE);

    // When disabling the editor, whe remove any entity created by the editor
    LDKHEntity* h = (LDKHEntity*) ldkArrayGetData(internalEditor.editorEntities);
    uint32 numEntities = ldkArrayCount(internalEditor.editorEntities);
    for (uint32 i = 0; i < numEntities; i++)
    {
      ldkEntityLookup(LDKStaticObject, *h)->materials = NULL;
      ldkEntityDestroy(*h);
      h++;
    }
    ldkArrayClear(internalEditor.editorEntities);

    internalEditor.selectedEntity = LDK_HENTITY_INVALID;
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
  internalEditor.editorEntities = ldkArrayCreate(sizeof(LDKHandle), 64);

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  ldkSmallString(&camera->entity.name, "Editor Camera");
  camera->position = vec3(0.0f, 5.0f, 10.0f);
  camera->target = vec3Zero();
  internalEditor.editorCamera = camera->entity.handle;

  // Creates an overriden material list for 3D icons
  internalEditor.editorIconMaterial = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle)->asset.handle;
  internalEditor.overridenMaterialArray = ldkOsMemoryAlloc(sizeof(LDKHAsset));
  internalEditor.overridenMaterialArray[0] = internalEditor.editorIconMaterial;

  LDKMaterial* material = ldkAssetLookup(LDKMaterial, internalEditor.editorIconMaterial);
  ldkMaterialParamSetTexture(material, "texture_diffuse1", ldkAssetGet(LDKTexture, "assets/editor/icons.texture"));

  return true;
}

void ldkEditorTerminate(void)
{
  ldkEventHandlerRemove(onUpdate);
  ldkEventHandlerRemove(onKeyboard);
  internalEditor.enabled = false;
}

