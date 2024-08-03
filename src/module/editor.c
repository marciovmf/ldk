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
  EDITOR_FLAG_ENTITY_PLACEHOLDER = 1,       // The entity was used as a placeholder for another entity
};

typedef enum
{
  // Tools
  EDITOR_TOOL_CAMERA  = 100,
  EDITOR_TOOL_MOVE    = 101,
  EDITOR_TOOL_SCALE   = 102,
  EDITOR_TOOL_ROTATE  = 103,
} EditorTool;

typedef enum
{
  EDITOR_TOOL_MOVE_AXIS_Y   = 0,
  EDITOR_TOOL_MOVE_AXIS_X   = 1,
  EDITOR_TOOL_MOVE_AXIS_Z   = 2,
  EDITOR_TOOL_MOVE_PLANE_XZ = 3,
  EDITOR_TOOL_MOVE_PLANE_ZY = 4,
  EDITOR_TOOL_MOVE_PLANE_XY = 5,
} MoveToolMode;

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
  // Entity Selection
  LDKEntitySelectionInfo selectedPlaceholder; // Might point to a placeholder entity
  LDKEntitySelectionInfo selectedEntity;      // Allways point to the acutal entity
                                              // Tools
  Vec3 toolPos;
  Vec3 toolScale;
  Quat toolRotation;
  EditorTool tool;
  LDKHEntity gizmoMove;
  MoveToolMode moveToolMode;
  bool toolIsHot;
} internalEditor = {0};

extern void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color);

extern void drawFrustum(float nearWidth, float nearHeight, float farWidth, float farHeight, float nearDist, float farDist, LDKRGB color);

static bool onKeyboard(const LDKEvent* evt, void* data)
{
  if (evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN )
  {
    // quit on SCAPE
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
    {
      ldkEngineStop(0);
    }

    // Change tool on TAB
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_TAB)
    {
      if (internalEditor.tool == EDITOR_TOOL_CAMERA)
      {
        ldkLogInfo("MOVE TOOL");
        internalEditor.tool = EDITOR_TOOL_MOVE;
      }
      else
      {
        ldkLogInfo("CAMERA TOOL");
        internalEditor.tool = EDITOR_TOOL_CAMERA;
      }
    }
  }

#if 0
  if (evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN || evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_HOLD)
  {
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
    {
      ldkEngineStop(0);
    }

    // moving handle ?
    if (ldkHandleIsValid(internalEditor.selectedEntity.handle) && internalEditor.tool == EDITOR_TOOL_MOVE)
    {
      Vec3 pos;
      Vec3 scale;
      Quat rot;
      bool updated = false;

      if (evt->keyboardEvent.keyCode == LDK_KEYCODE_UP)
      {
        updated = true;
        ldkEntityEditorGetTransform(&internalEditor.selectedEntity, &pos, &scale, &rot);
        pos.y += 0.3f;
        ldkEntityEditorSetTransform(&internalEditor.selectedEntity, pos, scale, rot);
      }
      else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_DOWN)
      {
        updated = true;
        ldkEntityEditorGetTransform(&internalEditor.selectedEntity, &pos, &scale, &rot);
        pos.y -= 0.3f;
        ldkEntityEditorSetTransform(&internalEditor.selectedEntity, pos, scale, rot);
      }
      else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_LEFT)
      {
        updated = true;
        ldkEntityEditorGetTransform(&internalEditor.selectedEntity, &pos, &scale, &rot);
        pos.x -= 0.3f;
        ldkEntityEditorSetTransform(&internalEditor.selectedEntity, pos, scale, rot);
      }
      else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_RIGHT)
      {
        updated = true;
        ldkEntityEditorGetTransform(&internalEditor.selectedEntity, &pos, &scale, &rot);
        pos.x += 0.3f;
        ldkEntityEditorSetTransform(&internalEditor.selectedEntity, pos, scale, rot);
      }

      if (updated && !ldkHandleEquals(internalEditor.selectedPlaceholder.handle, internalEditor.selectedEntity.handle))
      {
        ldkEntityEditorGetTransform(&internalEditor.selectedPlaceholder, NULL, &scale, &rot);
        ldkEntityEditorSetTransform(&internalEditor.selectedPlaceholder, pos, scale, rot);
      }
    }
  }
#endif
  return true;
}

static bool onMouse(const LDKEvent* evt, void* data)
{
  if (internalEditor.tool == EDITOR_TOOL_MOVE && ldkHandleIsValid(internalEditor.selectedEntity.handle))
  {
    if (evt->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_DOWN && evt->mouseEvent.mouseButton == LDK_MOUSE_BUTTON_LEFT)
    {
      internalEditor.toolIsHot = true;
    }
    else if (evt->mouseEvent.type == LDK_MOUSE_EVENT_BUTTON_UP && evt->mouseEvent.mouseButton == LDK_MOUSE_BUTTON_LEFT)
    {
      internalEditor.toolIsHot = false;
    }
    else if (evt->mouseEvent.type == LDK_MOUSE_EVENT_MOVE && internalEditor.toolIsHot)
    {
      const float speed = 2.0f;
      float xRel = evt->mouseEvent.xRel * speed * internalEditor.deltaTime;
      float yRel = -evt->mouseEvent.yRel * speed * internalEditor.deltaTime;

      if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_AXIS_X) { internalEditor.toolPos.x += xRel; }
      else if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_AXIS_Y) { internalEditor.toolPos.y += yRel; }
      else if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_AXIS_Z) { internalEditor.toolPos.z += xRel; }
      else if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_PLANE_XY) { internalEditor.toolPos.x += xRel; internalEditor.toolPos.y += yRel;}
      else if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_PLANE_XZ) { internalEditor.toolPos.x += xRel; internalEditor.toolPos.z += xRel;}
      else if (internalEditor.moveToolMode == EDITOR_TOOL_MOVE_PLANE_ZY) { internalEditor.toolPos.z += -xRel; internalEditor.toolPos.y += yRel;}
      ldkEntityEditorSetTransform(&internalEditor.selectedEntity, internalEditor.toolPos, internalEditor.toolScale, internalEditor.toolRotation);

      // update the placeholder
      if (!ldkHandleEquals(internalEditor.selectedPlaceholder.handle, internalEditor.selectedEntity.handle))
      {
        Vec3 scale;
        Quat rot;
        ldkEntityEditorGetTransform(&internalEditor.selectedPlaceholder, NULL, &scale, &rot);
        ldkEntityEditorSetTransform(&internalEditor.selectedPlaceholder, internalEditor.toolPos, scale, rot);
      }
    }
  }
  //if (internalEditor.tool == EDITOR_TOOL_SCALE) { internalEditor.toolScale = vec3Zero(); }
  //if (internalEditor.tool == EDITOR_TOOL_ROTATE) { internalEditor.toolRotation = vec3Zero(); }

  return true;
}

static bool internalEntitySelectionInfoEquals(LDKEntitySelectionInfo* a, LDKEntitySelectionInfo* b)
{
  bool equals = ldkHandleEquals(a->handle, b->handle) &&
    a->instanceIndex == b->instanceIndex &&
    a->surfaceIndex == b->surfaceIndex;
  return equals;
}

// Called every frame if the editor is enable
static bool onUpdate(const LDKEvent* evt, void* data)
{

  internalEditor.deltaTime = evt->frameEvent.deltaTime;

  LDKEntitySelectionInfo selected = ldkRendererSelectedEntity();
  bool changed = !internalEntitySelectionInfoEquals(&internalEditor.selectedPlaceholder, &selected);
  if (changed)
  {
    // A tool gizmo is only selected AFTER an actual entity or placeholder.
    // So we never update internalEditor.selectedPlaceholder nor
    // internalEditor.selectedEntity when a tool gizmo is selected.
    // is it a tool gizmo ?
    if (ldkHandleEquals(selected.handle, internalEditor.gizmoMove))
    {
      LDK_ASSERT(selected.surfaceIndex == EDITOR_TOOL_MOVE_AXIS_X
          || selected.surfaceIndex == EDITOR_TOOL_MOVE_AXIS_Y
          || selected.surfaceIndex == EDITOR_TOOL_MOVE_AXIS_Z
          || selected.surfaceIndex == EDITOR_TOOL_MOVE_PLANE_XZ
          || selected.surfaceIndex == EDITOR_TOOL_MOVE_PLANE_ZY
          || selected.surfaceIndex == EDITOR_TOOL_MOVE_PLANE_XY);

      internalEditor.moveToolMode = selected.surfaceIndex;
    }
    //else if (ldkHandleEquals(selected.handle, internalEditor.gizmoScale))
    //else if (ldkHandleEquals(selected.handle, internalEditor.gizmoRotate))
    else
    {
      internalEditor.selectedPlaceholder = selected;
      internalEditor.selectedEntity = internalEditor.selectedPlaceholder;
      internalEditor.toolIsHot = false;

      LDKEntityInfo* e = ldkEntityManagerFind(selected.handle);
      if (e && e->typeId == typeid(LDKStaticObject) && e->flags == EDITOR_FLAG_ENTITY_PLACEHOLDER)
      {
        LDKHEntity* h = (LDKHEntity*) ldkArrayGetData(internalEditor.editorEntities);
        uint32 numEntities = ldkArrayCount(internalEditor.editorEntities);
        for (uint32 i = 0; i < numEntities; i++)
        {
          if(ldkHandleEquals(h[i], selected.handle))
          {
            e = ldkEntityManagerFind(e->editorPlaceholder);
            internalEditor.selectedEntity.handle = e->handle;
            break;
          }
        }
      }
    }
    //ldkLogInfo("Entity %llx %d %d", internalEditor.selectedEntity.handle, internalEditor.selectedEntity.surfaceIndex, internalEditor.selectedEntity.instanceIndex);
  }

  LDKStaticObject* moveToolGizmo = ldkEntityLookup(LDKStaticObject, internalEditor.gizmoMove);
  LDKEntityInfo* targetEntity = ldkEntityManagerFind(internalEditor.selectedEntity.handle);
  if (targetEntity != NULL)
  {
    if (internalEditor.tool == EDITOR_TOOL_MOVE)
    {
      ldkEntityEditorGetTransform(&internalEditor.selectedEntity, &internalEditor.toolPos, &internalEditor.toolScale, &internalEditor.toolRotation);
      moveToolGizmo->position = internalEditor.toolPos;
      moveToolGizmo->entity.active = true;
    }
  }
  else
  {
    moveToolGizmo->entity.active = false;
  }

  // Update editor camera
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
    Mat4 world = mat4World(e->position, vec3One(), mat4ToQuat(ldkCameraViewMatrix(e)));
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

    // Signup for keyboard and frame events
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_FRAME_BEFORE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_KEYBOARD);
    ldkEventHandlerMaskSet(onMouse, LDK_EVENT_TYPE_MOUSE_MOVE | LDK_EVENT_TYPE_MOUSE_BUTTON);

    // Move tool gizmo
    LDKStaticObject *gizmo = ldkEntityCreate(LDKStaticObject);
    gizmo->mesh = ldkAssetGet(LDKMesh, "assets/editor/gizmo-move.mesh")->asset.handle;
    float s = 0.08f;
    gizmo->scale = vec3(s, s, s);
    gizmo->entity.active = false;
    gizmo->entity.isEditorGizmo = true;
    ldkArrayAdd(internalEditor.editorEntities, &gizmo->entity.handle);
    ldkRendererAddStaticObject(gizmo); 
    internalEditor.gizmoMove = gizmo->entity.handle;

    // Add 3D icons for 'invisible' entities
    LDKHListIterator it;
    it = ldkEntityManagerGetIterator(LDKDirectionalLight);
    while(ldkHListIteratorNext(&it))
    {
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
    while(ldkHListIteratorNext(&it))
    {
      LDKSpotLight* e = (LDKSpotLight*) ldkHListIteratorCurrent(&it);
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKCamera);
    while(ldkHListIteratorNext(&it))
    {
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

    // Stop receiving events
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(onMouse, LDK_EVENT_TYPE_NONE);

    // Cleanup entity selection
    internalEditor.selectedEntity.handle = LDK_HENTITY_INVALID;
    internalEditor.selectedEntity.instanceIndex = 0;
    internalEditor.selectedEntity.surfaceIndex = 0;

    // Remove any entity created by the editor
    LDKHEntity* h = (LDKHEntity*) ldkArrayGetData(internalEditor.editorEntities);
    uint32 numEntities = ldkArrayCount(internalEditor.editorEntities);
    for (uint32 i = 0; i < numEntities; i++)
    {
      ldkEntityLookup(LDKStaticObject, *h)->materials = NULL;
      ldkEntityDestroy(*h);
      h++;
    }
    ldkArrayClear(internalEditor.editorEntities);
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
  ldkEventHandlerAdd(onMouse, LDK_EVENT_TYPE_NONE, NULL);

  internalEditor.enabled = false;
  internalEditor.tool = EDITOR_TOOL_CAMERA;
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

