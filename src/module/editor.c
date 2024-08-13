#include "common.h"
#include "entity/staticobject.h"
#include "maths.h"
#include "module/entity.h"
#include "os.h"
#include <ldk.h>
#include <math.h>

extern void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color);

extern void drawFrustum(float nearWidth, float nearHeight, float farWidth, float farHeight, float nearDist, float farDist, LDKRGB color);

enum
{
  EDITOR_FLAG_ENTITY_PLACEHOLDER = 1,       // The entity was used as a placeholder for another entity
};

typedef enum
{
  // Tools
  EDITOR_TOOL_CAMERA  = 0,
  EDITOR_TOOL_MOVE    = 1,
  EDITOR_TOOL_SCALE   = 2,
  EDITOR_TOOL_ROTATE  = 3,
  EDITOR_TOOL_COUNT
} EditorToolType;

typedef enum
{
  EDITOR_TOOL_MODE_NONE     = -1,
  EDITOR_TOOL_MOVE_PLANE_XZ = 0,
  EDITOR_TOOL_MOVE_PLANE_ZY = 1,
  EDITOR_TOOL_MOVE_PLANE_XY = 2,
  EDITOR_TOOL_MOVE_AXIS_Y   = 3,
  EDITOR_TOOL_MOVE_AXIS_X   = 4,
  EDITOR_TOOL_MOVE_AXIS_Z   = 5,
  EDITOR_TOOL_MOVE_CENTER   = 6,

} EditorToolMode;

typedef struct
{
  EditorToolType type;  // Current tool type
  EditorToolMode mode;  // Current tool mode
  LDKHEntity gizmo[EDITOR_TOOL_COUNT]; // Current tool gizmo

  Vec3 position;        // target entity position
  Vec3 scale;           // target entity scale
  Quat rotation;        // target entity rotation
  bool isHot;           // is this gizmo being interacted with
  bool showGizmo;       // When the current tool gizmo should be drawn
  bool selectionIsGizmo;// When the current selected entity is a gizmo
  Vec3 originalMovePosition;

  // A ray displayed when
  Vec3 cursorOffset;    // where the cursor is, relative to the target. For example along one of the axis of a move gizmo.
  LDKRGB moveRayColor;
} EditorTool;

static struct 
{
  LDKHEntity editorCamera;
  LDKHEntity gizmoMove;       // Gizmo for the move tool
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
  EditorTool tool;
  LDKEntitySelectionInfo selectedEntity;
  LDKEntitySelectionInfo previousEntity;
} internalEditor = {0};

static bool internalEntitySelectionInfoEquals(LDKEntitySelectionInfo* a, LDKEntitySelectionInfo* b)
{
  bool equals = ldkHandleEquals(a->handle, b->handle) &&
    a->instanceIndex == b->instanceIndex &&
    a->surfaceIndex == b->surfaceIndex;
  return equals;
}

//
// Reset gizmo state
//
static void gizmoReset()
{
  internalEditor.tool.selectionIsGizmo = false;
  internalEditor.selectedEntity.handle = LDK_HENTITY_INVALID;
  internalEditor.previousEntity.handle = LDK_HENTITY_INVALID;
  internalEditor.tool.isHot = false;
  internalEditor.tool.showGizmo = false;
}

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
      // disable current tool gizmo
      LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[internalEditor.tool.type]);
      if (gizmo != NULL)
        gizmo->entity.active = false;

      if (internalEditor.tool.type == EDITOR_TOOL_CAMERA)
      {
        ldkLogInfo("MOVE TOOL");
        internalEditor.tool.type = EDITOR_TOOL_MOVE;
        gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[internalEditor.tool.type]);
        gizmo->entity.active = true;
      }
      else
      {
        ldkLogInfo("CAMERA TOOL");
        internalEditor.tool.type = EDITOR_TOOL_CAMERA;
      }
    }
  }
  return true;
}

Vec3 screenToWorldPos(LDKCamera* camera, int32 x, int32 y, Vec3 referencePosition)
{
  // Normalized direction vector from camera to mouse cursor in world space.
  Vec3 mouseVector = ldkCameraScreenCoordToWorldRay(camera, x, y);

  // Vector from the camera to the target entity
  Vec3 entityVector = vec3Sub(referencePosition, camera->position);
  float entityVectorLength = vec3Length(entityVector);

  // Angle between both vectors. Note that mouseVector length is 1
  float theta = fmaxf(-1.0f, fminf(1.0f, vec3Dot(entityVector, mouseVector) / entityVectorLength));
  float angle = acosf(theta);

  LDK_ASSERT(angle == angle); // Assert for NaN

  float distanceToMousePos = vec3Length(entityVector) / cosf(angle);
  Vec3 worldPos = vec3Add(vec3Mul(mouseVector, distanceToMousePos), camera->position);
  return worldPos;
}

static bool postUpdate(const LDKEvent* evt, void* data)
{
  // As entity selection depends on render phase, we need to handle it on postUpdate
  EditorTool* tool = &internalEditor.tool;
  LDKMouseState mouseState;
  ldkOsMouseStateGet(&mouseState);

  //
  // Handle entity selection change
  //
  if (ldkOsMouseButtonDown(&mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKEntitySelectionInfo previousEntity = internalEditor.selectedEntity;
    LDKEntitySelectionInfo selectedEntity = ldkRendererSelectedEntity();

    bool changed = !internalEntitySelectionInfoEquals(&selectedEntity, &previousEntity);
    if (changed)
    {
      if (ldkHandleIsValid(selectedEntity.handle) && tool->isHot == false)
      {
        LDKEntityInfo* targetEntity = ldkEntityManagerFind(selectedEntity.handle);
        internalEditor.tool.showGizmo = true;
        LDK_ASSERT(targetEntity != NULL);
        if (targetEntity->isEditorGizmo)
        {
          // It is a gizmo entity. For a gizmo to be visible it must be clicked
          // AFTER a regular entity (or another gizmo). So previousEntity is guaranteed to be valid.
          tool->selectionIsGizmo = true;
          ldkEntityEditorGetTransform(&selectedEntity, &tool->position, &tool->scale, &tool->rotation);
          tool->mode = selectedEntity.surfaceIndex;
          // selectedEntity and previousEntity should never point both to a
          // gizmo, otherwise we won't know which entity is being targeted by
          // the gizmo tool.
          internalEditor.selectedEntity = selectedEntity;
          if (selectedEntity.handle.value != previousEntity.handle.value)
          {
            internalEditor.previousEntity = previousEntity;
          }
        }
        else
        {
          tool->selectionIsGizmo = false;
          ldkEntityEditorGetTransform(&selectedEntity, &tool->position, &tool->scale, &tool->rotation);
          internalEditor.previousEntity = previousEntity;
          internalEditor.selectedEntity = selectedEntity;
        }
      }
      else
      {
        tool->selectionIsGizmo = false;
        tool->showGizmo = false;
      }

      // Move the gizmo to the selected entity position
      LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[EDITOR_TOOL_MOVE]);
      if (gizmo) gizmo->position = tool->position;

      //ldkLogInfo("SELECTION CHANGED: PREVIOUS = %llx, CURRENT = %llx", internalEditor.previousEntity.handle, internalEditor.selectedEntity.handle);
    }
  }

  if (ldkHandleIsValid(internalEditor.selectedEntity.handle) == false
      || ldkHandleIsValid(internalEditor.previousEntity.handle) == false
      || tool->selectionIsGizmo == false)
    return false;

  LDKCamera* camera = ldkEntityLookup(LDKCamera, internalEditor.editorCamera);

  // Remember, The selectedEntity is always a gizmo and the previousEntity is the actual entity
  if (tool->type == EDITOR_TOOL_MOVE)
  {
    LDKPoint cursorRel = ldkOsMouseCursorRelative(&mouseState);
    LDKPoint cursor = ldkOsMouseCursor(&mouseState);
    bool cursorMoved = (cursorRel.x != 0 || cursorRel.y != 0);

    if (ldkOsMouseButtonDown(&mouseState, LDK_MOUSE_BUTTON_LEFT))
    {
      ldkEntityEditorGetTransform(&internalEditor.previousEntity, &tool->position, &tool->scale, &tool->rotation);
      tool->originalMovePosition = tool->position;
      tool->cursorOffset = vec3Sub(screenToWorldPos(camera, cursor.x, cursor.y, tool->position), tool->position);
      tool->isHot = true;
      return true;
    }
    else if (ldkOsMouseButtonUp(&mouseState, LDK_MOUSE_BUTTON_LEFT))
    {
      tool->isHot = false;
      tool->originalMovePosition = vec3Zero();
      return true;
    }
    else if (cursorMoved && tool->isHot)
    {
      Vec3 worldPos = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);
      switch(tool->mode)
      {
        case EDITOR_TOOL_MOVE_AXIS_X:
          tool->position.x = worldPos.x - tool->cursorOffset.x;
          tool->moveRayColor = ldkRGB(255, 0, 0);
          break;
        case EDITOR_TOOL_MOVE_AXIS_Y:
          tool->position.y = worldPos.y - tool->cursorOffset.y;
          tool->moveRayColor = ldkRGB(0, 0, 255);
          break;
        case EDITOR_TOOL_MOVE_AXIS_Z:
          tool->position.z = worldPos.z - tool->cursorOffset.z;
          tool->moveRayColor = ldkRGB(0, 255, 0);
          break;
        case EDITOR_TOOL_MOVE_PLANE_XY:
          tool->position.x = worldPos.x - tool->cursorOffset.x;
          tool->position.y = worldPos.y - tool->cursorOffset.y;
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        case EDITOR_TOOL_MOVE_PLANE_XZ:
          tool->position.x = worldPos.x - tool->cursorOffset.x;
          tool->position.z = worldPos.z - tool->cursorOffset.z;
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        case EDITOR_TOOL_MOVE_PLANE_ZY:
          tool->position.z = worldPos.z - tool->cursorOffset.z;
          tool->position.y = worldPos.y - tool->cursorOffset.y;
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        case EDITOR_TOOL_MOVE_CENTER:
          tool->position = worldPos;
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        default:
          break;
      }

      // Update gizmo position
      LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[EDITOR_TOOL_MOVE]);
      gizmo->position = tool->position;

      LDKEntityInfo* target = ldkEntityManagerFind(internalEditor.previousEntity.handle);
      LDK_ASSERT(target != NULL);

      if (ldkHandleIsValid(target->editorPlaceholder))
      {
        // Update placeholder position ONLY
        Vec3 pos, scale;
        Quat rotation;
        ldkEntityEditorGetTransform(&internalEditor.previousEntity, &pos, &scale, &rotation);
        ldkEntityEditorSetTransform(&internalEditor.previousEntity, tool->position, scale, rotation);

        // Update the actual target transform
        LDKEntitySelectionInfo selection = {0};
        selection.handle = target->editorPlaceholder;

        ldkEntityEditorGetTransform(&selection, &pos, &scale, &rotation);
        ldkEntityEditorSetTransform(&selection, tool->position, scale, rotation);
      }
      else
      {
        // Update the target transform
        ldkEntityEditorSetTransform(&internalEditor.previousEntity, tool->position, tool->scale, tool->rotation);
      }
    }
  }
  //if (internalEditor.tool == EDITOR_TOOL_SCALE) { internalEditor.toolScale = vec3Zero(); }
  //if (internalEditor.tool == EDITOR_TOOL_ROTATE) { internalEditor.toolRotation = vec3Zero(); }
  return true;
}

// Called evuery frame if the editor is enable
static bool onUpdate(const LDKEvent* evt, void* data)
{
  internalEditor.deltaTime = evt->frameEvent.deltaTime;

  // Update editor camera
  LDKCamera* camera = ldkEntityLookup(LDKCamera, internalEditor.editorCamera);
  float deltaTime = evt->frameEvent.deltaTime;
  ldkCameraUpdateFreeCamera(camera, deltaTime, 30, 10);
  ldkRendererSetCamera(camera);

  //
  // Update current gizmo size
  //
  LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[internalEditor.tool.type]);
  if (internalEditor.tool.showGizmo && gizmo != NULL)
  {
    // Update scale factor for gizmos size
    float distance = vec3Dist(camera->position, gizmo->position);
    const float scaleFactor = distance * 0.008f;
    gizmo->scale = vec3Mul(vec3One(), scaleFactor);
    gizmo->entity.active = true;
  }
  else
  {
    LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, internalEditor.tool.gizmo[internalEditor.tool.type]);
    if (gizmo != NULL) gizmo->entity.active = false;
  }

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
  drawLine(0.0f, cameraY - lineLength, 0.0f, 0.0f, cameraY + lineLength, 0.0f, thickness, ldkRGB(0, 0, 255));
  drawLine(0.0f, 0.0f, cameraZ - lineLength, 0.0f, 0.0f, cameraZ + lineLength, thickness, ldkRGB(0, 255, 0));

  // While moving with the MOVE tool, draw a ray from the initial position to the current position.
  if (internalEditor.tool.isHot && internalEditor.tool.type == EDITOR_TOOL_MOVE)
  {
    Vec3 start = internalEditor.tool.originalMovePosition;
    Vec3 end = internalEditor.tool.position;
    drawLine(start.x, start.y, start.z, end.x,
        end.y, end.z, thickness * 3.0f, internalEditor.tool.moveRayColor);
  }

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
    ldkLogInfo("Editor mode"); 

    // Signup for keyboard and frame events
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_FRAME_BEFORE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_KEYBOARD);
    ldkEventHandlerMaskSet(postUpdate, LDK_EVENT_TYPE_FRAME_AFTER);

    // Cleanup editor selection
    internalEditor.selectedEntity.handle.value = 0;
    internalEditor.selectedEntity.instanceIndex = 0;
    internalEditor.selectedEntity.surfaceIndex = 0;

    // Add Move Tool Gizmo
    LDKStaticObject *moveGizmo = ldkEntityCreate(LDKStaticObject);
    moveGizmo->mesh = ldkAssetGet(LDKMesh, "assets/editor/gizmo-move.mesh")->asset.handle;
    float s = 0.08f;
    moveGizmo->scale = vec3(s, s, s);
    moveGizmo->entity.active = false;
    moveGizmo->entity.isEditorGizmo = true;
    internalEditor.tool.gizmo[EDITOR_TOOL_MOVE] = moveGizmo->entity.handle;
    ldkArrayAdd(internalEditor.editorEntities, &moveGizmo->entity.handle);
    ldkRendererAddStaticObject(moveGizmo); 

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
      if (e->entity.handle.value == internalEditor.editorCamera.value)  // Skip the editor camera
        continue;
      internalAddPlaceholderEntity(e->position, internalEditor.overridenMaterialArray, e->entity.handle);
    }
  }
  else
  {
    gizmoReset();
    ldkLogInfo("Game mode"); 
    // Stop receiving events
    ldkEventHandlerMaskSet(onUpdate, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(onKeyboard, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(postUpdate, LDK_EVENT_TYPE_NONE);

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
  ldkEventHandlerAdd(postUpdate, LDK_EVENT_TYPE_NONE, NULL);

  internalEditor.enabled = false;
  internalEditor.tool.type = EDITOR_TOOL_CAMERA;
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
  ldkEventHandlerRemove(postUpdate);
  internalEditor.enabled = false;
}

