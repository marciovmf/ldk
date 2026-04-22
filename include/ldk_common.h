/**
 * @file   ldk_common.h
 * @brief  Common definitions and utilities
 *
 * Shared types, macros, and helpers used across the LDK engine.
 *
 * Important defines:
 * LDK_ENGINE       :should be set when building the engine
 * LDK_SHAREDLIB    :should be set when building LDK as a dll.
 */

#ifndef LDK_COMMON_H
#define LDK_COMMON_H

#include <stdx/stdx_common.h>

#define LDK_VERSION_MAJOR 0
#define LDK_VERSION_PATCH 0
#define LDK_VERSION_MINOR 1

#ifndef LDK_BUILD_TYPE
#define LDK_BUILD_TYPE ""
#endif

#ifndef LDK_ENGINE
#define LDK_GAME
#endif

#ifdef _DEBUG
#ifndef LDK_DEBUG
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

#if defined(X_OS_WINDOWS)
#define  LDK_OS_WINDOWS
#elif defined(X_OS_LINUX)
#define  LDK_OS_LINUX
#elif defined(X_OS_OSX)
#define  LDK_OS_OSX
#endif

#define LDK_ASSERT(expr) X_ASSERT(expr)

typedef void* LDKWindow;

#endif //LDK_COMMON_H
