#include "asset/config.h"
#include "asset/material.h"
#include "asset/mesh.h"
#include "common.h"
#include "entity/camera.h"
#include "entity/directionallight.h"
#include "entity/pointlight.h"
#include "entity/spotlight.h"
#include "entity/staticobject.h"
#include "maths.h"
#include "module/asset.h"
#include "module/entity.h"
#include "module/renderer.h"
#include "os.h"
#include <ldk.h>
#include <math.h>
#include <stdbool.h>

#define SOKOBAN_MAX_BOX_COUNT 5
#define sokobanBoxId(c) ((uint32)((c) - 'A'))
#define sokobanInBound(i, w, h) ((i) >= 0 && (i) <= w * h)
#define sokobanPieceIndex(sokoban, x, y) ((y) * sokoban->size.width + (x))

typedef enum
{
  SOKOBAN_PIECE_PLAYER    = '@',
  SOKOBAN_PIECE_WALL      = '#',
  SOKOBAN_PIECE_FLOOR     = '.',
  SOKOBAN_PIECE_EMTPTY    = '-',
  // boxes
  SOKOBAN_PIECE_BOXA      = 'A',
  SOKOBAN_PIECE_BOXB      = 'B',
  SOKOBAN_PIECE_BOXC      = 'C',
  SOKOBAN_PIECE_BOXD      = 'D',
  SOKOBAN_PIECE_BOXE      = 'E',
  // Slots
  SOKOBAN_PIECE_SLOTA     = 'a',
  SOKOBAN_PIECE_SLOTB     = 'b',
  SOKOBAN_PIECE_SLOTC     = 'c',
  SOKOBAN_PIECE_SLOTD     = 'd',
  SOKOBAN_PIECE_SLOTE     = 'e',

} SokobanEnum;

typedef struct 
{
  bool changed;
  Vec3 animationStart;
  Vec3 animationTarget;
  char pieceId;
  LDKPoint coord;
  LDKStaticObject* staticObject;
} SokobanBox;

typedef struct
{
  bool changed;
  LDKPoint coord;
  Vec3 animationStart;
  Vec3 animationTarget;
  LDKStaticObject* staticObject;
} SokobanPlayer;

typedef struct
{
  LDKSize size;
  char* board;
  float movementTime;
  Vec3 cameraPos;
  Vec3 cameraTarget;
  SokobanBox box[SOKOBAN_MAX_BOX_COUNT];
  SokobanPlayer player;
  float animationSpeed;
  float animationTime;
  bool animating;
  uint32 boxCount;
} Sokoban;

typedef struct
{
  LDKMaterial material;
  LDKHEntity hCamera;
  float cameraLookSpeed;
  float cameraMoveSpeed;
  Sokoban sokoban;
  LDKDirectionalLight* directionalLight;
  uint32 numPointLights;
  LDKSpotLight* spotLight;
  LDKPointLight* pointLight[32];
} GameState;

typedef enum 
{
  SOKOBAN_UP = 0,
  SOKOBAN_DOWN,
  SOKOBAN_LEFT,
  SOKOBAN_RIGHT,
} SOKOBANDirection;

bool sokobanMove(SOKOBANDirection direction, Sokoban* sokoban)
{
  int32 playerX = sokoban->player.coord.x;
  int32 playerY = sokoban->player.coord.y;

  sokoban->animating = false;
  sokoban->player.changed = false;
  for(uint32 i = 0; i < sokoban->boxCount; i++)
    sokoban->box[i].changed = false;

  int32 xOffset = 0;
  int32 yOffset = 0;

  if (playerX < 0 || playerX >= sokoban->size.width)
    return false;

  if (playerY < 0 || playerY >= sokoban->size.height)
    return false;

  if (direction == SOKOBAN_UP)
  {
    yOffset = -1;
  }
  else if (direction == SOKOBAN_DOWN)
  {
    yOffset = 1;
  }
  else if (direction == SOKOBAN_LEFT)
  {
    xOffset = -1;
  }
  else if (direction == SOKOBAN_RIGHT)
  {
    xOffset = 1;
  }

  int32 playerSrcIndex = sokobanPieceIndex(sokoban, playerX, playerY);
  int32 playerDstIndex = sokobanPieceIndex(sokoban, (playerX + xOffset), (playerY + yOffset));
  char playerDstPiece = sokoban->board[playerDstIndex];

  // Is the player moving out of the bounds ?
  if (!sokobanInBound(playerDstIndex, sokoban->size.width, sokoban->size.height))
    return false;

  // is the player moving to an empty space ?
  if (playerDstPiece == '.')
  {
    sokoban->board[playerSrcIndex]   = '.';
    sokoban->board[playerDstIndex]  = '@';
    sokoban->player.coord.x += xOffset;
    sokoban->player.coord.y += yOffset;
    sokoban->player.changed = true;
    return true;
  }

  // is the player moving to a target ?
  //if (playerDstPiece == '.')
  //{
  //}

  // Is the player pushing a box ?
  if (playerDstPiece == 'A' || playerDstPiece == 'B' || playerDstPiece == 'C' || playerDstPiece == 'D' || playerDstPiece == 'E')
  {
    int32 boxDstIndex = sokobanPieceIndex(sokoban, playerX + (2 * xOffset), playerY + (2 * yOffset));

    // is the player trying to push a box out of bounds 
    if (!sokobanInBound(boxDstIndex, sokoban->size.width, sokoban->size.height))
      return false;

    // bump box ahead
    char pieceAtBoxDst = sokoban->board[boxDstIndex];

    // moving towards a slot ?
    if (pieceAtBoxDst == 'a' && pieceAtBoxDst == 'b' && pieceAtBoxDst == 'c' && pieceAtBoxDst == 'd')
    {
      // Not implemented yet
      return false;
    }
    else
    {
      // is the player trying to push a box to an occupied space ?
      if (pieceAtBoxDst != '.' )
        return false;

      sokoban->board[playerSrcIndex] = '.';
      sokoban->board[playerDstIndex] = '@';
      sokoban->board[boxDstIndex] = playerDstPiece;
    }

    int32 boxId = sokobanBoxId(playerDstPiece);
    LDK_ASSERT(boxId >= 0 && boxId <= (int32) sokoban->boxCount);
    SokobanBox* box = &sokoban->box[boxId];
    box->coord.x += xOffset;
    box->coord.y += yOffset;
    box->changed = true;

    sokoban->player.coord.x += xOffset;
    sokoban->player.coord.y += yOffset;
    sokoban->player.changed = true;

    return true;
  }

  return false;
} 

#ifdef LDK_DEBUG
void sokobanPrintBoard(Sokoban* sokoban)
{
  printf("\n");
  char* p = sokoban->board;
  for (int i = 0; i < sokoban->size.height; i++)
  {
    printf("%.*s\n", sokoban->size.width, p);
    p += sokoban->size.width;
  }
}
#else
#define void sokobanPrintBoard(sokoban)
#endif

bool onKeyboardEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN
      || event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_HOLD)
  {
    if (state->sokoban.animating)
      return false;

    // Move player
    if (!state->sokoban.animating)
    {
      int32 xOffset = 0;
      int32 zOffset = 0;

      LDKStaticObject* entity =  state->sokoban.player.staticObject;
      if (event->keyboardEvent.keyCode == LDK_KEYCODE_W && sokobanMove(SOKOBAN_UP, &state->sokoban))
      {
        zOffset = -1;
        state->sokoban.player.staticObject->rotation = quatRotationY((float) degToRadian(180.0));
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_S && sokobanMove(SOKOBAN_DOWN, &state->sokoban))
      {
        zOffset = 1;
        state->sokoban.player.staticObject->rotation = quatRotationY((float) degToRadian(0.0));
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_A && sokobanMove(SOKOBAN_LEFT, &state->sokoban))
      {
        xOffset = -1;
        state->sokoban.player.staticObject->rotation = quatRotationY((float) degToRadian(-90.0));
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_D && sokobanMove(SOKOBAN_RIGHT, &state->sokoban))
      {
        xOffset = 1;
        state->sokoban.player.staticObject->rotation = quatRotationY((float) degToRadian(90.0));
      }

      if (xOffset != 0 || zOffset != 0)
      {
        sokobanPrintBoard(&state->sokoban);
        state->sokoban.animating = true;
        state->sokoban.player.animationStart = entity->position;
        state->sokoban.player.animationTarget = vec3(entity->position.x + xOffset, entity->position.y, entity->position.z + zOffset);

        for(uint32 i = 0; i < state->sokoban.boxCount; i++)
        {
          if (!state->sokoban.box[i].changed)
            continue;

          LDKStaticObject* entity =  state->sokoban.box[i].staticObject;
          state->sokoban.box[i].animationStart = entity->position;
          state->sokoban.box[i].animationTarget = vec3(entity->position.x + xOffset, entity->position.y, entity->position.z + zOffset);
        }
      }
    }
  }
#if 1
  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_UP
      && event->keyboardEvent.keyCode == LDK_KEYCODE_L)
  {
    bool b = !state->directionalLight->entity.active;
    state->directionalLight->entity.active = b;
  }

  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_UP
      && event->keyboardEvent.keyCode == LDK_KEYCODE_P)
  {
    for (uint32 i = 0; i < state->numPointLights; i++)
    {
      bool b = !state->pointLight[i]->entity.active;
      state->pointLight[i]->entity.active = b;
    }
  }
#endif

  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_UP
      && event->keyboardEvent.keyCode == LDK_KEYCODE_X)
  {
    LDKHEntity targetEntity = ldkRendererSelectedEntity();
    if (ldkHandleIsValid(targetEntity))
    {
      ldkLogInfo("Destroying entity %llx", targetEntity);
      ldkEntityDestroy(targetEntity);
    }
  }

  return true;
}

bool onWindowEvent(const LDKEvent* event, void* state)
{
  if (event->windowEvent.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldkLogInfo("Fechando janela!");
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

bool onUpdate(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);


  // Animate player
  if (state->sokoban.animating)
  {
    state->sokoban.player.staticObject->position = vec3Lerp(
        state->sokoban.player.animationStart,
        state->sokoban.player.animationTarget,
        state->sokoban.animationTime);

    // Animate box
    for (uint32 i = 0; i < state->sokoban.boxCount; i++)
    {
      if (!state->sokoban.box[i].changed)
        continue;

      state->sokoban.box[i].staticObject->position = vec3Lerp(
          state->sokoban.box[i].animationStart,
          state->sokoban.box[i].animationTarget,
          state->sokoban.animationTime);
    }

    if (state->sokoban.animationTime >= 1.0f)
    {
      state->sokoban.animating = false;
      state->sokoban.animationTime = 0.0f;
    }

    state->sokoban.animationTime += event->frameEvent.deltaTime * state->sokoban.animationSpeed;
    state->sokoban.animationTime = clamp(state->sokoban.animationTime, 0.0f, 1.0f);
  }

  ldkRendererSetCamera(camera);

  state->pointLight[0]->position = state->sokoban.box[1].staticObject->position;
  state->pointLight[0]->position.y += 1.0f;
  state->pointLight[1]->position = state->sokoban.box[0].staticObject->position;
  state->pointLight[1]->position.y += 1.0f;

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
  ldkEventHandlerAdd(onUpdate,    LDK_EVENT_TYPE_FRAME_BEFORE, (void*) &state);

  //
  // Create some entities
  //

  LDKCamera* camera = ldkEntityCreate(LDKCamera);
  camera->position = vec3Zero();
  camera->target = vec3(0.0f, 0.0f, -1.0f);

#if 0
  for(uint32 i = 0; i < 10; i++)
  {
    LDKStaticObject* obj = ldkEntityCreate(LDKStaticObject);
    LDKMesh* mesh = ldkAssetGet(LDKMesh, "assets/dock.mesh");
    ldkStaticObjectSetMesh(obj, mesh->asset.handle);

    // Clone each material and customize it
    for (uint32 m = 0; m < mesh->numMaterials; m++)
    {
      LDKMaterial* material = ldkMaterialClone(mesh->materials[m]);
      ldkMaterialParamSetVec3(material, "color", vec3(1.0f, 0.0f, 0.0f));
      ldkMaterialParamSetFloat(material, "colorIntensity", 0.1f * i);
      obj->materials[m] = material->asset.handle;
    }

    float f = 3.0f + (0.5f * i);
    obj->scale = vec3(f, f, f);
    obj->position.x -= 4.0f;
    obj->position.z = -25.0f + i * f;
    obj->position.y = 5.0f;
  }
#endif

#define SOKOBAN 1
#if SOKOBAN

  state.sokoban.cameraPos = vec3(4.136592f, 8.118844f, 9.579421f);
  state.sokoban.cameraTarget = vec3(4.140860f, 7.318166f, 8.980342f);
  state.sokoban.size.width = 10;
  state.sokoban.size.height = 7;
  state.sokoban.boxCount = 0;
  state.sokoban.board =
#if 0
    "---####---"
    "###...@##-"
    "#.......#-"
    "#..A#B...#"
    "#.......#-"
    "#..#....#-"
    "#########-";
#else
  ".........."
    "......@..."
    ".........."
    "...A#B...."
    ".........."
    ".........."
    "..........";
#endif

  camera->position = state.sokoban.cameraPos;
  camera->target = state.sokoban.cameraTarget;

  //
  // Board
  //
  LDKInstancedObject* io = ldkEntityCreate(LDKInstancedObject);
  LDKMesh* cube = ldkAssetGet(LDKMesh, "assets/box.mesh");

  io->mesh = cube->asset.handle;

  for (int i = state.sokoban.size.height - 1; i >= 0; i--)
  {
    for (int j = 0 ; j < state.sokoban.size.width; j++)
    {
      char piece = state.sokoban.board[(i * state.sokoban.size.width) + j];
      float x = (float) j;
      float z = (float) i;

      if (piece == SOKOBAN_PIECE_PLAYER)
      {
        state.sokoban.player.coord.x = (uint32) x;
        state.sokoban.player.coord.y = (uint32) z;
        piece = SOKOBAN_PIECE_FLOOR;      //lets put a floor under the player position
      }
      else if (piece == SOKOBAN_PIECE_BOXA || piece == SOKOBAN_PIECE_BOXB ||
          piece == SOKOBAN_PIECE_BOXC || piece == SOKOBAN_PIECE_BOXD || piece == SOKOBAN_PIECE_BOXD)
      {
        uint32 index = sokobanBoxId(piece);
        state.sokoban.box[index].coord = ldkPoint((uint32)x, (uint32)z);
        state.sokoban.box[index].pieceId = piece;
        state.sokoban.boxCount++;
        piece = SOKOBAN_PIECE_FLOOR;      //lets put a floor under the box position
      }

      // -- 
      if (piece == SOKOBAN_PIECE_WALL || piece == SOKOBAN_PIECE_FLOOR)
      {
        float x = (float) j;
        float z = (float) i;
        float y = piece == SOKOBAN_PIECE_FLOOR ? -1.0f : 0.0f;
        ldkInstancedObjectAddInstance(io, vec3(x, y, z), vec3(0.5f, 0.5f, 0.5f), quatId());
      }
    }
  }

  ldkInstancedObjectUpdate(io);

  //
  // Room
  //
  //LDKStaticObject* room = ldkEntityCreate(LDKStaticObject);
  //ldkSmallString(&room->entity.name, "ROOM");
  //ldkStaticObjectSetMesh(room, cube->asset.handle);
  //room->scale = vec3(-10.0,-10.0,-10.0);
  //room->position = vec3(3.0f, 4.0f, 3.0f);
  //room->materials[0] = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle)->asset.handle;

  //
  // Player
  //

  //LDKStaticObject* playerObj = ldkEntityCreate(LDKStaticObject);

  //ldkSmallString(&playerObj->entity.name, "PLAYER");
  //ldkStaticObjectSetMesh(playerObj, cube->asset.handle);
  //LDKMaterial* material = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle);
  //ldkMaterialParamSetVec3(material, "color", vec3(1.0f, 1.0f, 1.0f));
  //ldkMaterialParamSetFloat(material, "colorIntensity", 1.0f);

  //playerObj->materials[0] = material->asset.handle;
  //playerObj->position = vec3((float) state.sokoban.player.coord.x, 0.0f, (float) state.sokoban.player.coord.y);
  //playerObj->scale    = vec3(.7f, 1.0f, .7f);
  //state.sokoban.player.staticObject = playerObj;

  // Boxes
  for (uint32 i = 0; i < state.sokoban.boxCount; i++)
  {
    LDKStaticObject* box = ldkEntityCreate(LDKStaticObject);
    box->mesh = cube->asset.handle;
    box->position = vec3((float) state.sokoban.box[i].coord.x, 0.0f, (float) state.sokoban.box[i].coord.y);
    ldkEntitySetNameFormat(box, "BOX-%d", i);
    box->scale    = vec3(.5f, .5f, .5f);
    state.sokoban.box[i].staticObject = box;
  }

  // Cyborg
  LDKStaticObject* cyborg = ldkEntityCreate(LDKStaticObject);
  ldkEntitySetName(cyborg, "Cyborg1");
  cyborg->mesh = ldkAssetGet(LDKMesh, "assets/cyborg/cyborg.mesh")->asset.handle;
  cyborg->position = vec3((float) state.sokoban.player.coord.x, -0.4f, (float) state.sokoban.player.coord.y);
  cyborg->scale = vec3(0.5f, 0.5f, 0.5f);
  state.sokoban.player.staticObject = cyborg;

  cyborg = ldkEntityCreate(LDKStaticObject);
  ldkSmallString(&cyborg->entity.name, "Cyborg2");
  cyborg->mesh = ldkAssetGet(LDKMesh, "assets/cyborg/cyborg.mesh")->asset.handle;
  cyborg->position = vec3(4, 0, 10);
  cyborg->scale = vec3One();


  //
  // Lights
  //

  // Directional Light
  LDKDirectionalLight* directionalLight = ldkEntityCreate(LDKDirectionalLight);
  directionalLight->colorSpecular = vec3(0.2f, 0.4f, 0.0f);
  directionalLight->colorDiffuse = vec3(1.0f, 1.0f, 1.0f);
  directionalLight->position = camera->position;
  directionalLight->direction = ldkCameraDirectionNormalized(camera);
  state.directionalLight = directionalLight;

  // Point Light
  uint32 i = 0;
  state.pointLight[i] = ldkEntityCreate(LDKPointLight);
  state.pointLight[i]->colorDiffuse = vec3One();
  state.pointLight[i]->colorSpecular = vec3One();
  ldkLightAttenuationForDistance(&state.pointLight[i]->attenuation, 360.0f);
  i++;

  state.pointLight[i] = ldkEntityCreate(LDKPointLight);
  state.pointLight[i]->colorDiffuse = vec3(.2f, .7f, .7f);
  state.pointLight[i]->colorSpecular = vec3One();
  ldkLightAttenuationForDistance(&state.pointLight[i]->attenuation, 360.0f);
  i++;

  state.numPointLights = i;
  // Spot Light
  state.spotLight = ldkEntityCreate(LDKSpotLight);
  state.spotLight->position = camera->position;
  state.spotLight->direction = ldkCameraDirectionNormalized(camera);

  LDKMaterial* m = ldkMaterialCreateFromShader("assets/editor/lightbox.shader");
  ldkQuadMeshCreate(m->asset.handle);

  LDKStaticObject* o = ldkEntityCreate(LDKStaticObject);
  o->mesh = m->asset.handle;
#endif

  LDKConfig* cfg = ldkAssetGet(LDKConfig, "ldk.cfg");
  state.sokoban.animationSpeed  = ldkConfigGetFloat(cfg, "sokoban.animation.speed");
  state.cameraMoveSpeed = ldkConfigGetFloat(cfg, "game.camera-move-speed");
  state.cameraLookSpeed = ldkConfigGetFloat(cfg, "game.camera-look-speed");
  Vec3 clearColor = ldkConfigGetVec3(cfg, "game.clear-color");
  state.hCamera = camera->entity.handle;
  ldkRendererSetCamera(camera);
  ldkRendererSetClearColorVec3(clearColor);
  ldkAmbientLightSetIntensity(ldkConfigGetFloat(cfg, "game.ambient-light-intensity"));

  return ldkEngineRun();
}

