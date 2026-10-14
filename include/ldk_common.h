/**
 * # Common engine definitions
 *
 * Common definitions and utilities
 *
 * Shared types, macros, and helpers used across the LDK engine.
 *
 * Important defines:
 * LDK_ENGINE       :should be set when building the engine
 * LDK_SHAREDLIB    :should be set when building LDK as a dll.
 */

#ifndef LDK_COMMON_H
#define LDK_COMMON_H

#include <stddef.h>
#include <stdx/stdx_common.h>

/** @group ldk */
#define LDK_VERSION_MAJOR 0
/** @group ldk */
#define LDK_VERSION_PATCH 0
/** @group ldk */
#define LDK_VERSION_MINOR 1

#ifndef LDK_BUILD_TYPE
/** @group ldk */
#define LDK_BUILD_TYPE ""
#endif

#ifndef LDK_ENGINE
/** @group ldk */
#define LDK_GAME
#endif

#ifdef _DEBUG
#ifndef LDK_DEBUG
/** @group ldk */
#define LDK_DEBUG
#endif
#endif

#ifdef LDK_SHAREDLIB
#ifdef X_COMPILER_MSVC
#ifdef LDK_ENGINE
#define LDK_API X_PLAT_EXPORT
#else
#define LDK_API X_PLAT_IMPORT
#endif
#endif
#else
#define LDK_API
#endif

#if defined(LDK_MONOLITHIC)
/** @group ldk */
#define LDK_GAME_API
#else
/** @group ldk */
#define LDK_GAME_API X_PLAT_EXPORT
#endif

#if defined(X_OS_WINDOWS)
/** @group ldk */
#define LDK_OS_WINDOWS
#elif defined(X_OS_LINUX)
/** @group ldk */
#define LDK_OS_LINUX
#elif defined(X_OS_OSX)
/** @group ldk */
#define LDK_OS_OSX
#endif

/** @group ldk.engine */
#define LDK_ASSERT(expr) X_ASSERT(expr)
/** @group ldk.engine */
#define LDK_STATIC_ASSERT(expr, name) X_STATIC_ASSERT((expr), name)

typedef u32 rgba32;

#ifdef X_LITTLE_ENDIAN
/** @group ldk.engine */
#define LDK_RGBA32(c)                                                          \
  ((u32)(((c) & 0x000000FFu) << 24) | (((c) & 0x0000FF00u) << 8) |             \
      (((c) & 0x00FF0000u) >> 8) | (((c) & 0xFF000000u) >> 24))
#else
/** @group ldk.engine */
#define LDK_RGBA(c) ((u32)c)
#endif

/** @group ldk */
typedef void *LDKWindow;

#endif // LDK_COMMON_H
