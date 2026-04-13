/**
 * @file   ldk_system.h
 * @brief  System Registry module.
 * 
 * This module manages registration and execution of engine systems. Systems are
 * units of executable logic that operate on engine entities and components and are
 * scheduled into execution buckets.
 *
 * System execution order is determined per bucket using order values,
 * with registration order as a stable tie-breaker.
 * 
 * Systems are not meant to be passed around or referenced by pointer.
 * They are registered as descriptors and owned by the registry.
 *
 * Systems are identified by a unique, compile-time known u64 id.
 * Registration, unregistration, and clear are only allowed while stopped.
 *
 * This module has a very explicit lifecycle, and some actions are only possible
 * depending on its current state:
 * 
 *   initialize  -> prepares the registry context
 *   register    -> adds system descriptors (only allowed while stopped)
 *   start       -> builds execution lists and initializes systems
 *   run_bucket  -> executes systems in a given bucket (only while started)
 *   stop        -> terminates systems and clears runtime state
 *   clear       -> removes all registered systems (only while stopped)
 *   terminate   -> releases all resources
 * 
 * The registry is owned by the engine root and should not be instantiated or
 * managed independently.
 */

#ifndef LDK_SYSTEM_H
#define LDK_SYSTEM_H

#include <ldk_common.h>
#include <stdx/stdx_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LDKSystemBucket
{
  LDK_SYSTEM_BUCKET_PRE_UPDATE = 0,
  LDK_SYSTEM_BUCKET_UPDATE,
  LDK_SYSTEM_BUCKET_POST_UPDATE,
  LDK_SYSTEM_BUCKET_RENDER,
  LDK_SYSTEM_BUCKET_COUNT
} LDKSystemBucket;

typedef enum LDKSystemFlags
{
  LDK_SYSTEM_FLAG_NONE             = 0,
  LDK_SYSTEM_FLAG_ENABLED          = 1 << 0,
  LDK_SYSTEM_FLAG_ENGINE_NATIVE    = 1 << 1,
  LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED  = 1 << 2
} LDKSystemFlags;

struct LDKRoot;

typedef int  (*LDKSystemInitializeFn)(void** userdata);
typedef void (*LDKSystemTerminateFn)(void* userdata);
typedef void (*LDKSystemPreUpdateFn)(void* userdata, float dt);
typedef void (*LDKSystemUpdateFn)(void* userdata, float dt);
typedef void (*LDKSystemPostUpdateFn)(void* userdata, float dt);
typedef void (*LDKSystemRenderFn)(void* userdata, float dt);

typedef struct LDKSystemCallbacks
{
  LDKSystemInitializeFn initialize;
  LDKSystemTerminateFn terminate;

  LDKSystemPreUpdateFn pre_update;
  LDKSystemUpdateFn update;
  LDKSystemPostUpdateFn post_update;
  LDKSystemRenderFn render;
} LDKSystemCallbacks;

typedef struct LDKSystemDesc
{
  u64 id;
  const char* name;
  u32 flags;

  i32 pre_update_order;
  i32 update_order;
  i32 post_update_order;
  i32 render_order;

  LDKSystemCallbacks callbacks;
} LDKSystemDesc;

typedef struct LDKSystemRegistry
{
  void* internal;
  struct LDKRoot* root;
  u8 is_initialized;
  u8 is_started;
} LDKSystemRegistry;

LDK_API bool ldk_system_registry_initialize(LDKSystemRegistry* registry);
LDK_API void ldk_system_registry_terminate(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_register(LDKSystemRegistry* registry, const LDKSystemDesc* desc);
LDK_API bool ldk_system_registry_unregister(LDKSystemRegistry* registry, u64 id);
LDK_API bool ldk_system_registry_find_by_id(LDKSystemRegistry* registry, u64 id, LDKSystemDesc* out);
LDK_API bool ldk_system_registry_clear(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_start(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_stop(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_run_bucket(LDKSystemRegistry* registry, LDKSystemBucket bucket, float dt);
LDK_API bool ldk_system_registry_has(const LDKSystemRegistry* registry, u64 id);

#ifdef __cplusplus
}
#endif

#endif // LDK_SYSTEM_H
