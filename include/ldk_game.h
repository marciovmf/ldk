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

typedef struct LDKGame
{
  void* user_data;
  bool (*initialize)(struct LDKGame* game);
  bool (*start)(struct LDKGame* game);
  void (*update)(struct LDKGame* game, float delta_time);
  void (*stop)(struct LDKGame* game);
  void (*terminate)(struct LDKGame* game);
} LDKGame;

#endif //LDK_GAME_H

