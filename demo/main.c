#include <ldk.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

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
  LDKHandle hCamera;
  float cameraLookSpeed;
  float cameraMoveSpeed;
  Sokoban sokoban;
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
  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
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
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_S && sokobanMove(SOKOBAN_DOWN, &state->sokoban))
      {
        zOffset = 1;
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_A && sokobanMove(SOKOBAN_LEFT, &state->sokoban))
      {
        xOffset = -1;
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_D && sokobanMove(SOKOBAN_RIGHT, &state->sokoban))
      {
        xOffset = 1;
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

  //ldkLogInfo("Camera Position %f, %f, %f; Target %f, %f, %f",
  //    camera->position.x, camera->position.y, camera->position.z,
  //    camera->target.x, camera->target.y, camera->target.z);

  ldkRendererCameraSet(camera);

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

  state.sokoban.cameraPos = vec3(4.666242f, 7.928266f, 5.953767f);
  state.sokoban.cameraTarget = vec3(4.649732f, 6.982578f, 5.629108f);
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
  "----------"
    "---...@---"
    "-.......--"
    "-..A#B...-"
    "-.......--"
    "-..#....--"
    "----------";
#endif

  camera->position = state.sokoban.cameraPos;
  camera->target = state.sokoban.cameraTarget;

  //
  // Board
  //
  LDKInstancedObject* io = ldkEntityCreate(LDKInstancedObject);
  LDKMesh* cube = ldkAssetGet(LDKMesh, "assets/cube.mesh");

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
        ldkInstancedObjectAddInstance(io, vec3(x, y, z), vec3One(), quatId());
      }
    }
  }

  ldkInstancedObjectUpdate(io);

  //
  // Player
  //
  LDKStaticObject* playerObj = ldkEntityCreate(LDKStaticObject);
  ldkStaticObjectSetMesh(playerObj, cube->asset.handle);
  LDKMaterial* material = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle);
  ldkMaterialParamSetVec3(material, "color", vec3(.6f, .6f, .6f));
  ldkMaterialParamSetFloat(material, "colorIntensity", 1.0f);

  playerObj->materials[0] = material->asset.handle;
  playerObj->position = vec3((float) state.sokoban.player.coord.x, 0.0f, (float) state.sokoban.player.coord.y);
  playerObj->scale    = vec3(.7f, 1.0f, .7f);

  state.sokoban.player.staticObject = playerObj;


  // Boxes
  for (uint32 i = 0; i < state.sokoban.boxCount; i++)
  {
    LDKStaticObject* box = ldkEntityCreate(LDKStaticObject);
    ldkStaticObjectSetMesh(box, cube->asset.handle);
    LDKMaterial* material = ldkMaterialClone(ldkAssetGet(LDKMaterial, "assets/default.material")->asset.handle);
    ldkMaterialParamSetVec3(material, "color", vec3(1.0f, 1.0f, 1.0f));
    ldkMaterialParamSetFloat(material, "colorIntensity", 1.0f);

    box->materials[0] = material->asset.handle;
    box->position = vec3((float) state.sokoban.box[i].coord.x, 0.0f, (float) state.sokoban.box[i].coord.y);
    box->scale    = vec3One();
    state.sokoban.box[i].staticObject = box;
  }

#endif

  LDKConfig* cfg = ldkAssetGet(LDKConfig, "ldk.cfg");
  state.sokoban.animationSpeed  = ldkConfigGetFloat(cfg, "sokoban.animation.speed");
  state.cameraMoveSpeed = ldkConfigGetFloat(cfg, "game.camera-move-speed");
  state.cameraLookSpeed = ldkConfigGetFloat(cfg, "game.camera-look-speed");

  // We should never keep pointers to entities. Intead, we keep their Handle
  state.hCamera = camera->entity.handle;
  ldkRendererCameraSet(camera);

  return ldkEngineRun();
}
