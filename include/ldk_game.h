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
#include <ldk_os.h>
#include <ldk_gl.h>
#include <ldk_entity.h>
#include <ldk_component.h>
#include <ldk_system.h>
#include <ldk_eventqueue.h>

#include <stdx/stdx_log.h>
#include <stdx/stdx_filesystem.h>

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

