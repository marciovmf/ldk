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

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#define X_IMPL_LOG
#define X_IMPL_HASHTABLE
#define X_IMPL_HPOOL
#endif // LDK_SHAREDLIB
     
      
#include <ldk_argparse.h>
#include <ldk_os.h>
#include <ldk_gl.h>
#include <ldk_entity.h>
#include <ldk_component.h>
#include <ldk_eventqueue.h>
#include <stdx/stdx_log.h>

#endif // LDK_GAME

#include <ldk.h>

#endif //LDK_GAME_H

