/**
 * @file   ldk_system.h
 * @brief  System Registry module.
 *
 * Systems are registered as descriptors and owned by the registry. They are
 * identified by unique, compile-time known u64 ids.
 *
 * The registry has a structural lifecycle:
 *   initialize -> register -> start -> stop -> clear/terminate
 *
 * Registry start builds the ordered bucket lists, but does not initialize
 * individual systems. Individual systems are started and stopped by id while
 * the registry is prepared. Registration and structural changes are only
 * allowed while the registry is stopped.
 *
 * The registry is owned by the engine root and should not be instantiated or
 * managed independently by game code.
 */

#ifndef LDK_SYSTEM_H
#define LDK_SYSTEM_H

#include <ldk_common.h>
#include <stdx/stdx_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IDs of builtin systems. */
typedef enum LDKBuiltinSystemId
{
  LDK_SYSTEM_ID_SCENEGRAPH = 0x1,
  LDK_SYSTEM_ID_USER = 0x100
} LDKBuiltinSystemId;

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
  u8 is_started;  /* Bucket lists are prepared. */
  u8 is_paused;   /* Execution is suspended, not individual state. */
} LDKSystemRegistry;

LDK_API bool ldk_system_registry_initialize(LDKSystemRegistry* registry);
LDK_API void ldk_system_registry_terminate(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_register(LDKSystemRegistry* registry, const LDKSystemDesc* desc);
LDK_API bool ldk_system_registry_unregister(LDKSystemRegistry* registry, u64 id);
LDK_API bool ldk_system_registry_find_by_id(LDKSystemRegistry* registry, u64 id, LDKSystemDesc* out);
LDK_API u32 ldk_system_registry_count(const LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_at(const LDKSystemRegistry* registry, u32 index, LDKSystemDesc* out);
LDK_API bool ldk_system_registry_clear(LDKSystemRegistry* registry);

/** Prepare the bucket lists without initializing individual systems. */
LDK_API bool ldk_system_registry_start(LDKSystemRegistry* registry);

/** Terminate all active systems in reverse registration order and unprepare. */
LDK_API bool ldk_system_registry_stop(LDKSystemRegistry* registry);

/**
 * Start/stop a registered system without changing the bucket lists.
 * Repeated start/stop calls succeed without repeating callbacks. A failed
 * initialize callback is followed by terminate to release partial userdata.
 * A failed start leaves other systems unchanged.
 */
LDK_API bool ldk_system_registry_system_start(LDKSystemRegistry* registry, u64 id);
LDK_API bool ldk_system_registry_system_stop(LDKSystemRegistry* registry, u64 id);
LDK_API bool ldk_system_registry_system_is_started(const LDKSystemRegistry* registry, u64 id);

/** Suspend/resume execution without terminating individual systems. */
LDK_API bool ldk_system_registry_pause(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_resume(LDKSystemRegistry* registry);
LDK_API bool ldk_system_registry_is_paused(const LDKSystemRegistry* registry);

/** True while a bucket or a system lifecycle callback is executing. */
LDK_API bool ldk_system_registry_is_busy(const LDKSystemRegistry* registry);

/**
 * Run only initialized, enabled systems. While paused, only systems with
 * RUN_WHEN_PAUSED execute. Structural and lifecycle changes are forbidden
 * during bucket execution and system initialize/terminate callbacks.
 */
LDK_API bool ldk_system_registry_run_bucket(LDKSystemRegistry* registry, LDKSystemBucket bucket, float dt);
LDK_API bool ldk_system_registry_has(const LDKSystemRegistry* registry, u64 id);

#ifdef __cplusplus
}
#endif

#endif // LDK_SYSTEM_H
