/**
 * @file   ldk_game.h
 * @brief  Umbrella header for LDK games
 *
 * This header centralizes all required includes and ensures proper integration
 * with the stdx libraries, whether the engine is used as a shared library or
 * statically linked into the game.
 */

#ifndef LDK_GAME_H
#define LDK_GAME_H

#include <ldk_common.h>
#include <ldk_argparse.h>
#include <ldk_event.h>
#include <ldk_mesh.h>
#include <ldk_os.h>

#include <component/ldk_camera.h>
#include <component/ldk_mesh_source.h>
#include <component/ldk_transform.h>

#include <module/ldk_asset_manager.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_entity.h>
#include <module/ldk_eventqueue.h>
#include <module/ldk_eventqueue.h>
#include <module/ldk_system.h>

#include <stdx/stdx_log.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_math.h>

#include <ldk.h>

#if defined(LDK_GAME_STATIC) && (LDK_EDITOR)
#error "Incompatible defines: LDK_GAME_STATIC and LDK_EDITOR"
#endif

#if defined(LDK_MONOLITHIC)
  #define LDK_GAME_API
#else
  #define LDK_GAME_API X_PLAT_EXPORT
#endif

LDK_GAME_API bool game_initialize(struct LDKGame* game);
LDK_GAME_API bool game_start(struct LDKGame* game);
LDK_GAME_API void game_update(struct LDKGame* game, float delta_time);
LDK_GAME_API void game_stop(struct LDKGame* game);
LDK_GAME_API void game_terminate(struct LDKGame* game);


typedef bool (*LDKGameInitializeFunc)(struct LDKGame* game);
typedef bool (*LDKGameStartFunc)(struct LDKGame* game);
typedef void (*LDKGameUpdateFunc)(struct LDKGame* game, float delta_time);
typedef void (*LDKGameStopFunc)(struct LDKGame* game);
typedef void (*LDKGameTerminateFunc)(struct LDKGame* game);


#ifndef LDK_GAME_INITIALIZE_FUNC_NAME
#define LDK_GAME_INITIALIZE_FUNC_NAME "game_initialize"
#endif

#ifndef LDK_GAME_START_FUNC_NAME
#define LDK_GAME_START_FUNC_NAME "game_start"
#endif

#ifndef LDK_GAME_UPDATE_FUNC_NAME
#define LDK_GAME_UPDATE_FUNC_NAME "game_update"
#endif

#ifndef LDK_GAME_STOP_FUNC_NAME
#define LDK_GAME_STOP_FUNC_NAME "game_stop"
#endif

#ifndef LDK_GAME_TERMINATE_FUNC_NAME
#define LDK_GAME_TERMINATE_FUNC_NAME "game_terminate"
#endif

typedef struct LDKGame
{
  bool initialized;
  void* user_data;
  LDKLibrary* lib;
  LDKGameInitializeFunc initialize;
  LDKGameStartFunc      start;
  LDKGameUpdateFunc     update;
  LDKGameStopFunc       stop;
  LDKGameTerminateFunc  terminate;
} LDKGame;

#endif //LDK_GAME_H

