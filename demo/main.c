#include <ldk.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct
{
  LDKSize size;
  char* board;
  LDKHandle player;
  float movementTime;
  Vec3 cameraPos;
  Vec3 cameraTarget;
  LDKPoint playerCoord;
} Sokoban;

typedef struct
{
  LDKMaterial material;
  LDKHandle hCamera;
  uint64 ticksStart;
  uint64 ticksEnd;
  float deltaTime;
  float cameraLookSpeed;
  float cameraMoveSpeed;
  bool flyCamera;
  bool animating;
  Vec3 animationStart;
  Vec3 animationTarget;
  float animationTime;
  Sokoban sokoban;
  float animationSpeed;
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
  int32 x = sokoban->playerCoord.x;
  int32 y = sokoban->playerCoord.y;

  if (x < 0 || x >= sokoban->size.height)
    return false;

  if (y < 0 || y >= sokoban->size.width)
    return false;

  int32 dstIndex = 0;
  int32 srcIndex = x * sokoban->size.width + y;

  if (direction == SOKOBAN_UP)
  {
    x -= 1; 
  }
  else if (direction == SOKOBAN_DOWN)
  {
    x += 1;
  }
  else if (direction == SOKOBAN_LEFT)
  {
    y -= 1;
  }
  else if (direction == SOKOBAN_RIGHT)
  {
    y += 1;
  }

  dstIndex = (x * sokoban->size.width) + y;

  bool valid = 
    (dstIndex >= 0 &&
     dstIndex < sokoban->size.width * sokoban->size.height &&
     sokoban->board[dstIndex] == '.');

  if (valid)
  {
    char pieceAtSrc = sokoban->board[srcIndex];
    char pieceAtDst = sokoban->board[dstIndex];
    sokoban->board[srcIndex] = pieceAtDst;
    sokoban->board[dstIndex] = pieceAtSrc;

    sokoban->playerCoord.x = x;
    sokoban->playerCoord.y = y;
  }

  return valid;
} 

bool onKeyboardEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  if (event->keyboardEvent.type == LDK_KEYBOARD_EVENT_KEY_DOWN)
  {
#ifdef LDK_DEBUG
    // Toggle fly camera
    if (event->keyboardEvent.keyCode == LDK_KEYCODE_F3)
    {
      LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);
      state->flyCamera = !state->flyCamera;
      if (!state->flyCamera)
      {
        camera->position = state->sokoban.cameraPos;
        camera->target = state->sokoban.cameraTarget;
      }
    }
#endif

    // Move player
    if (!state->flyCamera && !state->animating)
    {
      if (event->keyboardEvent.keyCode == LDK_KEYCODE_W && sokobanMove(SOKOBAN_UP, &state->sokoban))
      {
        LDKStaticObject* entity = ldkEntityLookup(LDKStaticObject, state->sokoban.player);
        state->animationStart = entity->position;
        state->animationTarget = vec3(entity->position.x, entity->position.y, entity->position.z - 1);
        state->animating = true;
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_S && sokobanMove(SOKOBAN_DOWN, &state->sokoban))
      {
        LDKStaticObject* entity = ldkEntityLookup(LDKStaticObject, state->sokoban.player);
        state->animationStart = entity->position;
        state->animationTarget = vec3(entity->position.x, entity->position.y, entity->position.z + 1);
        state->animating = true;
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_A && sokobanMove(SOKOBAN_LEFT, &state->sokoban))
      {
        LDKStaticObject* entity = ldkEntityLookup(LDKStaticObject, state->sokoban.player);
        state->animationStart = entity->position;
        state->animationTarget = vec3(entity->position.x - 1, entity->position.y, entity->position.z);
        state->animating = true;
      }
      else if (event->keyboardEvent.keyCode == LDK_KEYCODE_D && sokobanMove(SOKOBAN_RIGHT, &state->sokoban))
      {
        LDKStaticObject* entity = ldkEntityLookup(LDKStaticObject, state->sokoban.player);
        state->animationStart = entity->position;
        state->animationTarget = vec3(entity->position.x + 1, entity->position.y, entity->position.z);
        state->animating = true;
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

bool onFrameEvent(const LDKEvent* event, void* data)
{
  GameState* state = (GameState*) data;
  LDKCamera* camera = ldkEntityLookup(LDKCamera, state->hCamera);

  if (event->frameEvent.type == LDK_FRAME_EVENT_BEFORE_RENDER)
  {
    double delta = (float) ldkOsTimeTicksIntervalGetMilliseconds(state->ticksStart, state->ticksEnd);
    state->deltaTime = (float) delta / 1000;
    state->ticksStart = ldkOsTimeTicksGet();

#ifdef LDK_DEBUG
    if (state->flyCamera)
    {
      ldkCameraUpdateFreeCamera(camera, state->deltaTime, state->cameraLookSpeed, state->cameraMoveSpeed);
    }
#endif

    if (state->animating)
    {
      LDKStaticObject* player = ldkEntityLookup(LDKStaticObject, state->sokoban.player);
      player->position = vec3Lerp(state->animationStart, state->animationTarget, state->animationTime);
      state->animationTime += state->deltaTime * state->animationSpeed;

      if (state->animationTime >= 1.0f)
      {
        state->animating = false;
        state->animationTime = 0.0f;
      }
    }

    LDKHListIterator it = ldkEntityManagerGetIterator(LDKStaticObject);
    while(ldkHListIteratorNext(&it))
    {
      ldkRendererAddStaticObject(it.ptr);
    }

    it = ldkEntityManagerGetIterator(LDKInstancedObject);
    while(ldkHListIteratorNext(&it))
    {
      ldkRendererAddInstancedObject(it.ptr);
    }


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
  camera->position = vec3Zero();
  camera->target = vec3(0.0f, 0.0f, -1.0f);

#if 1
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

  state.sokoban.cameraPos = vec3(2.229180f, 11.014329f, 8.020866f);
  state.sokoban.cameraTarget = vec3(2.333776f, 10.095286f, 7.640840f);
  state.sokoban.size.width = 10;
  state.sokoban.size.height = 7;
  state.sokoban.playerCoord = ldkPoint(1, 6);
  state.sokoban.board =
    "---####---"
    "###...@##-"
    "#.......#-"
    "#........#"
    "#.......#-"
    "#.......#-"
    "#########-";

  camera->position = state.sokoban.cameraPos;
  camera->target = state.sokoban.cameraTarget;

  //
  // Board
  //
  Vec3 initialPlayerPos = {0};
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

      if (piece == '@')
      {
        initialPlayerPos.x = x;
        initialPlayerPos.z = z;
        piece = '.'; //lets put a floor under the player position
      }

      if (piece == '#' || piece == '.')
      {
        float x = (float) j;
        float z = (float) i;
        float y = piece == '.' ? -1.0f : 0.0f;
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
  ldkMaterialParamSetVec3(material, "color", vec3(1.0f, 0.0f, 0.0f));
  ldkMaterialParamSetFloat(material, "colorIntensity", 1.0f);

  playerObj->materials[0] = material->asset.handle;
  playerObj->position = initialPlayerPos;
  playerObj->scale    = vec3(.8f, 1.0f, .7f);

  state.sokoban.player = playerObj->entity.handle;
#endif

  LDKConfig* cfg = ldkAssetGet(LDKConfig, "ldk.cfg");
  state.animationSpeed  = ldkConfigGetFloat(cfg, "sokoban.animation.speed");
  state.cameraMoveSpeed = ldkConfigGetFloat(cfg, "game.camera-move-speed");
  state.cameraLookSpeed = ldkConfigGetFloat(cfg, "game.camera-look-speed");

  // We should never keep pointers to entities. Intead, we keep their Handle
  state.hCamera = camera->entity.handle;
  ldkRendererCameraSet(camera);

  return ldkEngineRun();
}
