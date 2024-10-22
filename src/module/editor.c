#include "common.h"
#include "entity/staticobject.h"
#include "maths.h"
#include "module/entity.h"
#include "os.h"
#include <ldk.h>
#include <math.h>

#ifdef LDK_EDITOR

extern void drawCircle(float x, float y, float z, float radius, float thickness, float angle);

extern void drawLine(float x1, float y1, float z1, float x2, float y2, float z2, float thickness, LDKRGB color);

extern void drawFrustum(float nearWidth, float nearHeight, float farWidth, float farHeight, float nearDist, float farDist, LDKRGB color);

enum
{
  EDITOR_FLAG_ENTITY_PLACEHOLDER = 1,       // The entity was used as a placeholder for another entity
};

typedef enum
{
  // Tools
  EDITOR_TOOL_MOVE    = 0,
  EDITOR_TOOL_SCALE   = 1,
  EDITOR_TOOL_ROTATE  = 2,
  EDITOR_TOOL_COUNT
} EditorToolType;

typedef enum
{
  EDITOR_TOOL_MODE_NONE     = -1,
  EDITOR_TOOL_AXIS_Y        = 0,
  EDITOR_TOOL_AXIS_X        = 1,
  EDITOR_TOOL_AXIS_Z        = 2,
  EDITOR_TOOL_CENTER        = 3,
  EDITOR_TOOL_PLANE_XZ      = 3,
  EDITOR_TOOL_PLANE_XZ_BACK = 4,
  EDITOR_TOOL_PLANE_ZY      = 5,
  EDITOR_TOOL_PLANE_ZY_BACK = 6,
  EDITOR_TOOL_PLANE_XY      = 7,
  EDITOR_TOOL_PLANE_XY_BACK = 8,
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
  bool localSpace;
  Vec3 originalPosition;
  Vec3 originalScale;
  Quat originalRotation;

  // A ray displayed when
  Vec3 cursorOffset;    // where the cursor is, relative to the target. For example along one of the axis of a move gizmo.
  Vec3 originalCursorPosition; // where the cursor is, relative to the target. For example along one of the axis of a move gizmo.
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
} s_editor = {0};

static bool s_entitySelectionInfoEquals(LDKEntitySelectionInfo* a, LDKEntitySelectionInfo* b)
{
  bool equals = ldkHandleEquals(a->handle, b->handle) &&
    a->instanceIndex == b->instanceIndex &&
    a->surfaceIndex == b->surfaceIndex;
  return equals;
}

//
// Reset gizmo state
//
static void s_gizmoReset()
{
  s_editor.tool.selectionIsGizmo = false;
  s_editor.selectedEntity.handle = LDK_HENTITY_INVALID;
  s_editor.previousEntity.handle = LDK_HENTITY_INVALID;
  s_editor.tool.isHot = false;
  s_editor.tool.showGizmo = false;
}

static bool s_onKeyboard(const LDKEvent* evt, void* data)
{
  if (evt->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN )
  {
    // quit on SCAPE
    if (evt->keyboardEvent.keyCode == LDK_KEYCODE_ESCAPE)
    {
      ldkEngineStop(0);
    }

    if (s_editor.tool.isHot == false)
    {
      if (evt->keyboardEvent.keyCode == LDK_KEYCODE_G)
      {
        s_editor.tool.localSpace = !s_editor.tool.localSpace;
        ldkLogInfo("%s space", (const char*) (s_editor.tool.localSpace ? "Local" : "Global"));
      }
      else
      if (evt->keyboardEvent.keyCode == LDK_KEYCODE_T)
      {
        // Disable current gizmo entity
        LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        if (gizmo != NULL) gizmo->entity.active = false;

        // enable next gizmo entity
        ldkLogInfo("MOVE TOOL");
        s_editor.tool.type = EDITOR_TOOL_MOVE;
        gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        gizmo->entity.active = true;
      }
      else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_E)
      {
        // Disable current gizmo entity
        LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        if (gizmo != NULL) gizmo->entity.active = false;

        // enable next gizmo entity
        ldkLogInfo("SCALE TOOL");
        s_editor.tool.type = EDITOR_TOOL_SCALE;
        gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        gizmo->entity.active = true;
      }
      else if (evt->keyboardEvent.keyCode == LDK_KEYCODE_R)
      {
        // Disable current gizmo entity
        LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        if (gizmo != NULL) gizmo->entity.active = false;

        // enable next gizmo entity
        ldkLogInfo("ROTATE TOOL");
        s_editor.tool.type = EDITOR_TOOL_ROTATE;
        gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
        gizmo->entity.active = true;
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

static bool s_moveToolUpdate(LDKCamera* camera, LDKMouseState* mouseState, EditorTool* tool)
{
  LDKPoint cursor = ldkOsMouseCursor(mouseState);
  LDKPoint cursorRel = ldkOsMouseCursorRelative(mouseState);
  bool cursorMoved = (cursorRel.x != 0 || cursorRel.y != 0);

  if (ldkOsMouseButtonDown(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &tool->position, &tool->scale, &tool->rotation);
    tool->originalPosition = tool->position;
    tool->cursorOffset = vec3Sub(screenToWorldPos(camera, cursor.x, cursor.y, tool->position), tool->position);
    tool->isHot = true;
    return true;
  }
  else if (ldkOsMouseButtonUp(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    tool->isHot = false;
    tool->originalPosition = vec3Zero();
    return true;
  }
  else if (cursorMoved && tool->isHot)
  {
    Vec3 worldPos = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);
    worldPos = vec3Sub(worldPos, tool->cursorOffset);
    switch(tool->mode)
    {
      case EDITOR_TOOL_AXIS_X:
        {
          Vec3 direction = tool->localSpace ? vec3Normalize(quatGetRight(tool->rotation)) : vec3Right();
          Vec3 increment = vec3Mul(direction, vec3Dot(vec3Sub(worldPos, tool->originalPosition), direction));
          tool->position = vec3Add(tool->originalPosition, increment);
          tool->moveRayColor = ldkRGB(255, 0, 0);
          break;
        }
      case EDITOR_TOOL_AXIS_Y:
        {
          Vec3 direction = tool->localSpace ? vec3Normalize(quatGetUp(tool->rotation)) : vec3Up();
          Vec3 increment = vec3Mul(direction, vec3Dot(vec3Sub(worldPos, tool->originalPosition), direction));
          tool->position = vec3Add(tool->originalPosition, increment);
          tool->moveRayColor = ldkRGB(0, 255, 0);
          break;
        }
      case EDITOR_TOOL_AXIS_Z:
        {
          Vec3 direction = tool->localSpace ? vec3Normalize(quatGetForward(tool->rotation)) : vec3Forward();
          Vec3 increment = vec3Mul(direction, vec3Dot(vec3Sub(worldPos, tool->originalPosition), direction));
          tool->position = vec3Add(tool->originalPosition, increment);
          tool->moveRayColor = ldkRGB(0, 0, 255);
          break;
        }
      case EDITOR_TOOL_PLANE_XY:
      case EDITOR_TOOL_PLANE_XY_BACK:
        {
          Vec3 dx = tool->localSpace ? vec3Normalize(quatGetRight(tool->rotation)) : vec3Right();
          Vec3 ix = vec3Mul(dx, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dx));
          Vec3 dy = tool->localSpace ? vec3Normalize(quatGetUp(tool->rotation)) : vec3Up();
          Vec3 iy = vec3Mul(dy, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dy));
          tool->position = vec3Add(vec3Add(tool->originalPosition, ix), iy);
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        }
      case EDITOR_TOOL_PLANE_XZ:
      case EDITOR_TOOL_PLANE_XZ_BACK:
        {
          Vec3 dx = tool->localSpace ? vec3Normalize(quatGetRight(tool->rotation)) : vec3Right();
          Vec3 ix = vec3Mul(dx, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dx));
          Vec3 dy = tool->localSpace ? vec3Normalize(quatGetForward(tool->rotation)) : vec3Forward();
          Vec3 iy = vec3Mul(dy, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dy));
          tool->position = vec3Add(vec3Add(tool->originalPosition, ix), iy);
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        }
      case EDITOR_TOOL_PLANE_ZY:
      case EDITOR_TOOL_PLANE_ZY_BACK:
        {
          Vec3 dx = tool->localSpace ? vec3Normalize(quatGetForward(tool->rotation)) : vec3Forward();
          Vec3 ix = vec3Mul(dx, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dx));
          Vec3 dy = tool->localSpace ? vec3Normalize(quatGetUp(tool->rotation)) : vec3Up();
          Vec3 iy = vec3Mul(dy, vec3Dot(vec3Sub(worldPos, tool->originalPosition), dy));
          tool->position = vec3Add(vec3Add(tool->originalPosition, ix), iy);
          tool->moveRayColor = ldkRGB(0, 255, 255);
          break;
        }
      default:
        break;
    }

    LDKEntityInfo* target = ldkEntityManagerFind(s_editor.previousEntity.handle);
    LDK_ASSERT(target != NULL);

    if (ldkHandleIsValid(target->editorPlaceholder))
    {
      // Update placeholder position ONLY
      Vec3 pos, scale;
      Quat rotation;
      ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, tool->position, scale, rotation);

      // Update the actual target transform
      LDKEntitySelectionInfo selection = {0};
      selection.handle = target->editorPlaceholder;

      ldkEntityGetTransform(selection.handle, selection.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(selection.handle, selection.instanceIndex, tool->position, scale, rotation);
    }
    else
    {
      // Update the target transform
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, tool->position, tool->scale, tool->rotation);
    }
  }
  return true;
}

static bool s_scaleToolUpdate(LDKCamera* camera, LDKMouseState* mouseState, EditorTool* tool)
{
  LDKPoint cursor = ldkOsMouseCursor(mouseState);
  LDKPoint cursorRel = ldkOsMouseCursorRelative(mouseState);
  bool cursorMoved = (cursorRel.x != 0 || cursorRel.y != 0);

  if (ldkOsMouseButtonDown(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &tool->position, &tool->scale, &tool->rotation);
    tool->originalCursorPosition = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);
    //tool->cursorOffset = vec3Sub(screenToWorldPos(camera, cursor.x, cursor.y, tool->position), tool->position);
    tool->originalScale = tool->scale;
    tool->isHot = true;
    return true;
  }
  else if (ldkOsMouseButtonUp(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    tool->isHot = false;
    return true;
  }
  else if (cursorMoved && tool->isHot)
  {
    Vec3 worldPos = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);

    switch(tool->mode)
    {
      case EDITOR_TOOL_AXIS_X:
        tool->scale.x = tool->originalScale.x * (worldPos.x / tool->originalCursorPosition.x);
        break;

      case EDITOR_TOOL_AXIS_Y:
        tool->scale.y = tool->originalScale.y * (worldPos.y / tool->originalCursorPosition.y);
        break;

      case EDITOR_TOOL_AXIS_Z:
        tool->scale.z = tool->originalScale.z * (worldPos.z / tool->originalCursorPosition.z);
        break;

      case EDITOR_TOOL_CENTER:
        float amount = worldPos.x + worldPos.y / tool->originalCursorPosition.x;
        tool->scale.x = tool->originalScale.x * amount;
        tool->scale.y = tool->originalScale.y * amount;
        tool->scale.z = tool->originalScale.z * amount;
        break;

      default:
        break;
    }

    LDKEntityInfo* target = ldkEntityManagerFind(s_editor.previousEntity.handle);
    LDK_ASSERT(target != NULL);

    if (ldkHandleIsValid(target->editorPlaceholder))
    {
      // Update placeholder position ONLY
      Vec3 pos, scale;
      Quat rotation;
      ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, pos, tool->scale, rotation);

      // Update the actual target transform
      LDKEntitySelectionInfo selection = {0};
      selection.handle = target->editorPlaceholder;

      ldkEntityGetTransform(selection.handle, selection.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(selection.handle, selection.instanceIndex, pos, tool->scale, rotation);
    }
    else
    {
      // Update the target transform
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, tool->position, tool->scale, tool->rotation);
    }
  }
  return true;
}

static bool s_rotateToolUpdate(LDKCamera* camera, LDKMouseState* mouseState, EditorTool* tool)
{
  LDKPoint cursor = ldkOsMouseCursor(mouseState);
  LDKPoint cursorRel = ldkOsMouseCursorRelative(mouseState);
  bool cursorMoved = (cursorRel.x != 0 || cursorRel.y != 0);

  if (ldkOsMouseButtonDown(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &tool->position, &tool->scale, &tool->rotation);
    tool->originalPosition = tool->position;
    tool->originalRotation = tool->rotation;
    tool->cursorOffset = vec3Sub(screenToWorldPos(camera, cursor.x, cursor.y, tool->position), tool->position);
    tool->originalCursorPosition = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);
    tool->isHot = true;
    return true;
  }
  else if (ldkOsMouseButtonUp(mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    tool->isHot = false;
    return true;
  }
  else if (cursorMoved && tool->isHot)
  {
    // Find the angle between initial cursor position and current cursor position relative to the current rotation axis
    const Vec3 worldPos = screenToWorldPos(camera, cursor.x, cursor.y, tool->position);
    const Vec3 v1 = vec3Sub(s_editor.tool.originalCursorPosition, s_editor.tool.originalPosition);
    const Vec3 v2 = vec3Sub(worldPos, s_editor.tool.originalPosition);
    const float magnitudeV1 = vec3Length(v1);
    const float magnitudeV2 = vec3Length(v2);
    const Vec3 cross = vec3Cross(v1, v2);

    if(floatIsZero(magnitudeV1) || floatIsZero(magnitudeV2))
      return true;

    const float cosTheta = vec3Dot(v1, v2) / (magnitudeV1 * magnitudeV2);
    float angle = acosf(fmaxf(-1.0f, fminf(1.0f, cosTheta)));

    switch (tool->mode)
    {
      case EDITOR_TOOL_AXIS_X:
        if (s_editor.tool.localSpace)
        {
          angle = vec3Dot(cross, quatGetRight(tool->originalRotation)) < 0.0f ? -angle : angle;
          tool->rotation = quatMulQuat(tool->originalRotation, quatRotationX(angle));
        }
        else
        {
          angle = vec3Dot(cross, vec3Right()) < 0.0f ? -angle : angle;
          tool->rotation = quatMulQuat(quatRotationX(angle), tool->originalRotation);
        }
        break;

      case EDITOR_TOOL_AXIS_Y:
        if (s_editor.tool.localSpace)
        {
          angle = vec3Dot(cross, quatGetUp(tool->originalRotation)) < 0.0f ? -angle : angle;
          tool->rotation = quatMulQuat(tool->originalRotation, quatRotationY(angle));
        }
        else
        {
          angle = vec3Dot(cross, vec3Up()) < 0.0f ? -angle : angle;
          tool->rotation = quatMulQuat(quatRotationY(angle), tool->originalRotation);
        }
        break;

      case EDITOR_TOOL_AXIS_Z:
        if (s_editor.tool.localSpace)
        {
          angle = vec3Dot(cross, quatGetForward(tool->originalRotation)) ? -angle : angle;
          tool->rotation = quatMulQuat(tool->originalRotation, quatRotationZ(angle));
        }
        else
        {
          angle = vec3Dot(cross, vec3Forward()) < 0.0f ? -angle : angle;
          tool->rotation = quatMulQuat(quatRotationZ(angle), tool->originalRotation);
        }
        break;

      case EDITOR_TOOL_CENTER:
        break;

      default:
        break;
    }

    LDKEntityInfo* target = ldkEntityManagerFind(s_editor.previousEntity.handle);
    LDK_ASSERT(target != NULL);

    if (ldkHandleIsValid(target->editorPlaceholder))
    {
      // Update placeholder position ONLY
      Vec3 pos, scale;
      Quat rotation;
      ldkEntityGetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, pos, scale, tool->rotation);

      // Update the actual target transform
      LDKEntitySelectionInfo selection = {0};
      selection.handle = target->editorPlaceholder;

      ldkEntityGetTransform(selection.handle, selection.instanceIndex, &pos, &scale, &rotation);
      ldkEntitySetTransform(selection.handle, selection.instanceIndex, pos, scale, tool->rotation);
    }
    else
    {
      // Update the target transform
      ldkEntitySetTransform(s_editor.previousEntity.handle, s_editor.previousEntity.instanceIndex, tool->position, tool->scale, tool->rotation);
    }
  }
  return true;
}

static bool s_postUpdate(const LDKEvent* evt, void* data)
{
  // As entity selection depends on render phase, we need to handle it on postUpdate
  EditorTool* tool = &s_editor.tool;
  LDKMouseState mouseState;
  ldkOsMouseStateGet(&mouseState);
  bool changed = false;

  // Move the gizmo to the selected entity position
  LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
  if (gizmo)
  {
    gizmo->position = tool->position;
    gizmo->rotation = tool->localSpace ? tool->rotation : quatId();
  }

  //
  // Handle entity selection change
  //
  if (ldkOsMouseButtonDown(&mouseState, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKEntitySelectionInfo previousEntity = s_editor.selectedEntity;
    LDKEntitySelectionInfo selectedEntity = ldkRendererSelectedEntity();

    changed = !s_entitySelectionInfoEquals(&selectedEntity, &previousEntity);
    if (changed)
    {
      if (ldkHandleIsValid(selectedEntity.handle) && tool->isHot == false)
      {
        LDKEntityInfo* targetEntity = ldkEntityManagerFind(selectedEntity.handle);
        s_editor.tool.showGizmo = true;
        LDK_ASSERT(targetEntity != NULL);
        if (targetEntity->isEditorGizmo)
        {
          // It is a gizmo entity. For a gizmo to be visible it must be clicked
          // AFTER a regular entity (or another gizmo). So previousEntity is guaranteed to be valid.
          bool selectionWasGizmo = tool->selectionIsGizmo;
          tool->selectionIsGizmo = true;
          ldkEntityGetTransform(selectedEntity.handle, selectedEntity.instanceIndex, &tool->position, &tool->scale, &tool->rotation);
          tool->originalRotation = tool->rotation;
          tool->mode = selectedEntity.surfaceIndex;
          // selectedEntity and previousEntity should never point both to a
          // gizmo, otherwise we won't know which entity is being targeted by
          // the gizmo tool.
          s_editor.selectedEntity = selectedEntity;
          if (selectedEntity.handle.value != previousEntity.handle.value && selectionWasGizmo == false)
          {
            s_editor.previousEntity = previousEntity;
          }
        }
        else
        {
          tool->selectionIsGizmo = false;
          ldkEntityGetTransform(selectedEntity.handle, selectedEntity.instanceIndex, &tool->position, &tool->scale, &tool->rotation);
          s_editor.previousEntity = previousEntity;
          s_editor.selectedEntity = selectedEntity;
        }
      }
      else
      {
        tool->selectionIsGizmo = false;
        tool->showGizmo = false;
      }
    }
  }

  if (ldkHandleIsValid(s_editor.selectedEntity.handle) == false
      || ldkHandleIsValid(s_editor.previousEntity.handle) == false
      || tool->selectionIsGizmo == false)
    return false;

  LDKCamera* camera = ldkEntityLookup(LDKCamera, s_editor.editorCamera);

  // Remember, The selectedEntity is always a gizmo and the previousEntity is the actual entity
  if (tool->type == EDITOR_TOOL_MOVE)
  {
    s_moveToolUpdate(camera, &mouseState, tool);
  }
  else if (tool->type == EDITOR_TOOL_SCALE)
  {
    s_scaleToolUpdate(camera, &mouseState, tool);
  }
  else if (tool->type == EDITOR_TOOL_ROTATE)
  {
    s_rotateToolUpdate(camera, &mouseState, tool);
  }
  return true;
}

// Called evuery frame if the editor is enable
static bool s_onUpdate(const LDKEvent* evt, void* data)
{
  s_editor.deltaTime = evt->frameEvent.deltaTime;

  // Update editor camera
  LDKCamera* camera = ldkEntityLookup(LDKCamera, s_editor.editorCamera);
  float deltaTime = evt->frameEvent.deltaTime;
  ldkCameraUpdateFreeCamera(camera, deltaTime, 30, 10);
  ldkRendererSetCamera(camera);

  //
  // Update current gizmo size
  //
  LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
  if (s_editor.tool.showGizmo && gizmo != NULL)
  {
    // Update scale factor for gizmos size
    float distance = vec3Dist(camera->position, gizmo->position);
    const float scaleFactor = distance * 0.008f;
    gizmo->scale = vec3Mul(vec3One(), scaleFactor);
    gizmo->entity.active = true;
  }
  else
  {
    LDKStaticObject* gizmo = ldkEntityLookup(LDKStaticObject, s_editor.tool.gizmo[s_editor.tool.type]);
    if (gizmo != NULL) gizmo->entity.active = false;
  }

  return true;
}

void  ldkEditorImmediateDraw(float deltaTime)
{
  Mat4 identity = mat4Id();
  LDKCamera* camera = ldkEntityLookup(LDKCamera, s_editor.editorCamera);
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

  // While moving with the MOVE tool, draw a ray from the initial position to the current position.
  if (s_editor.tool.isHot && s_editor.tool.type == EDITOR_TOOL_MOVE)
  {
    Vec3 start = s_editor.tool.originalPosition;
    Vec3 end = s_editor.tool.position;
    drawLine(start.x, start.y, start.z, end.x,
        end.y, end.z, thickness * 3.0f, s_editor.tool.moveRayColor);
  }

  //
  // Draw Camera Gizmos
  //
  ldkShaderParamSetVec3(shader, "color", vec3(1.0f, 1.0f, 1.0f));
  LDKHListIterator it = ldkEntityManagerGetIterator(LDKCamera);
  while (ldkHListIteratorNext(&it))
  {
    LDKCamera* e = (LDKCamera*) ldkHListIteratorCurrent(&it);
    if (e->entity.handle.value == s_editor.editorCamera.value)
      continue;
    Mat4 world = mat4World(e->position, vec3One(), quatInverse(mat4ToQuat(ldkCameraViewMatrix(e))));
    ldkShaderParamSetMat4(shader, "mModel", world);
    drawFrustum(0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1.5f, ldkRGB(255, 255, 255));
  }


  drawCircle(
      s_editor.tool.position.x,
      s_editor.tool.position.y,
      s_editor.tool.position.z,
      1.0f, 2.0f, 360.0f);
}

static void s_addPlaceholderEntity(Vec3 position, LDKHAsset* materialOverrideList, LDKHEntity targetEntity)
{
  LDKStaticObject* icon = ldkEntityCreate(LDKStaticObject);
  icon->mesh = ldkAssetGet(LDKMesh, "assets/box.mesh")->asset.handle;
  icon->materials = materialOverrideList;
  icon->position = position;
  icon->scale = vec3(0.2f, 0.2f, 0.2f);
  icon->entity.editorPlaceholder = targetEntity;
  icon->entity.flags = EDITOR_FLAG_ENTITY_PLACEHOLDER;
  ldkSmallStringFormat(&icon->entity.name, "editor-%llx", targetEntity);
  ldkArrayAdd(s_editor.editorEntities, &icon->entity.handle);
  ldkRendererAddStaticObject(icon); 
}

// Called right after enabling/disabling the editor
void ldkEditorEnable(bool enabled)
{
  s_editor.enabled = enabled;
  if (s_editor.enabled)
  {
    // Signup for keyboard and frame events
    ldkEventHandlerMaskSet(s_onUpdate, LDK_EVENT_TYPE_FRAME_BEFORE);
    ldkEventHandlerMaskSet(s_onKeyboard, LDK_EVENT_TYPE_KEYBOARD);
    ldkEventHandlerMaskSet(s_postUpdate, LDK_EVENT_TYPE_FRAME_AFTER);

    // Cleanup editor selection
    s_editor.selectedEntity.handle.value = 0;
    s_editor.selectedEntity.instanceIndex = 0;
    s_editor.selectedEntity.surfaceIndex = 0;

    // Add Move tool gizmo
    LDKStaticObject *gizmo = ldkEntityCreate(LDKStaticObject);
    gizmo->mesh = ldkAssetGet(LDKMesh, "assets/editor/gizmo-move.mesh")->asset.handle;
    float s = 0.08f;
    gizmo->scale = vec3(s, s, s);
    gizmo->entity.active = false;
    gizmo->entity.isEditorGizmo = true;
    s_editor.tool.gizmo[EDITOR_TOOL_MOVE] = gizmo->entity.handle;
    ldkArrayAdd(s_editor.editorEntities, &gizmo->entity.handle);
    ldkRendererAddStaticObject(gizmo); 

    // Add Scale tool gizmo
    gizmo = ldkEntityCreate(LDKStaticObject);
    gizmo->mesh = ldkAssetGet(LDKMesh, "assets/editor/gizmo-scale.mesh")->asset.handle;
    s = 0.08f;
    gizmo->scale = vec3(s, s, s);
    gizmo->entity.active = false;
    gizmo->entity.isEditorGizmo = true;
    s_editor.tool.gizmo[EDITOR_TOOL_SCALE] = gizmo->entity.handle;
    ldkArrayAdd(s_editor.editorEntities, &gizmo->entity.handle);
    ldkRendererAddStaticObject(gizmo); 

    // Add Rotation tool gizmo
    gizmo = ldkEntityCreate(LDKStaticObject);
    gizmo->mesh = ldkAssetGet(LDKMesh, "assets/editor/gizmo-rotate.mesh")->asset.handle;
    s = 0.08f;
    gizmo->scale = vec3(s, s, s);
    gizmo->entity.active = false;
    gizmo->entity.isEditorGizmo = true;
    s_editor.tool.gizmo[EDITOR_TOOL_ROTATE] = gizmo->entity.handle;
    ldkArrayAdd(s_editor.editorEntities, &gizmo->entity.handle);
    ldkRendererAddStaticObject(gizmo); 

    // Add 3D icons for 'invisible' entities
    LDKHListIterator it;
    it = ldkEntityManagerGetIterator(LDKDirectionalLight);
    while(ldkHListIteratorNext(&it))
    {
      LDKDirectionalLight* e = (LDKDirectionalLight*) ldkHListIteratorCurrent(&it);
      s_addPlaceholderEntity(e->position, s_editor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKPointLight);
    while(ldkHListIteratorNext(&it)) 
    {
      LDKPointLight* e = (LDKPointLight*) ldkHListIteratorCurrent(&it);
      s_addPlaceholderEntity(e->position, s_editor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKSpotLight);
    while(ldkHListIteratorNext(&it))
    {
      LDKSpotLight* e = (LDKSpotLight*) ldkHListIteratorCurrent(&it);
      s_addPlaceholderEntity(e->position, s_editor.overridenMaterialArray, e->entity.handle);
    }

    it = ldkEntityManagerGetIterator(LDKCamera);
    while(ldkHListIteratorNext(&it))
    {
      LDKCamera* e = (LDKCamera*) ldkHListIteratorCurrent(&it);
      if (e->entity.handle.value == s_editor.editorCamera.value)  // Skip the editor camera
        continue;
      s_addPlaceholderEntity(e->position, s_editor.overridenMaterialArray, e->entity.handle);
    }
  }
  else
  {
    s_gizmoReset();
    // Stop receiving events
    ldkEventHandlerMaskSet(s_onUpdate, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(s_onKeyboard, LDK_EVENT_TYPE_NONE);
    ldkEventHandlerMaskSet(s_postUpdate, LDK_EVENT_TYPE_NONE);

    // Remove any entity created by the editor
    LDKHEntity* h = (LDKHEntity*) ldkArrayGetData(s_editor.editorEntities);
    uint32 numEntities = ldkArrayCount(s_editor.editorEntities);
    for (uint32 i = 0; i < numEntities; i++)
    {
      ldkEntityLookup(LDKStaticObject, *h)->materials = NULL;
      ldkEntityDestroy(*h);
      h++;
    }
    ldkArrayClear(s_editor.editorEntities);
  }
}

bool ldkEditorIsEnabled(void)
{
  return s_editor.enabled;
}

bool ldkEditorInitialize(void)
{
  ldkEventHandlerAdd(s_onUpdate, LDK_EVENT_TYPE_NONE, NULL); 
  ldkEventHandlerAdd(s_onKeyboard, LDK_EVENT_TYPE_NONE, NULL);
  ldkEventHandlerAdd(s_postUpdate, LDK_EVENT_TYPE_NONE, NULL);

  s_editor.enabled = false;
  s_editor.tool.type = EDITOR_TOOL_MOVE;
  s_editor.editorEntities = ldkArrayCreate(sizeof(LDKHandle), 64);

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  ldkSmallString(&camera->entity.name, "Editor Camera");
  camera->position = vec3(0.0f, 5.0f, 10.0f);
  camera->target = vec3Zero();
  s_editor.editorCamera = camera->entity.handle;

  // Creates an overriden material list for 3D icons
  s_editor.editorIconMaterial = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle)->asset.handle;
  s_editor.overridenMaterialArray = ldkOsMemoryAlloc(sizeof(LDKHAsset));
  s_editor.overridenMaterialArray[0] = s_editor.editorIconMaterial;

  LDKMaterial* material = ldkAssetLookup(LDKMaterial, s_editor.editorIconMaterial);
  ldkMaterialParamSetTexture(material, "texture_diffuse1", ldkAssetGet(LDKTexture, "assets/editor/icons.texture"));

  return true;
}

void ldkEditorTerminate(void)
{
  ldkEventHandlerRemove(s_onUpdate);
  ldkEventHandlerRemove(s_onKeyboard);
  ldkEventHandlerRemove(s_postUpdate);
  s_editor.enabled = false;
}

#endif// LDK_EDITOR
