/**
 * @file   ldk_system.h
 * @brief  System Registry module.
 *
 * Systems are registered as descriptors and owned by the registry. They are
 * identified by unique, compile-time known u64 ids.
 *
 * A system has at most one scheduled update callback. The bucket stored in the
 * descriptor selects where that callback executes. initialize and terminate
 * are optional lifecycle callbacks and do not participate in scheduling.
 *
 * System instance data is not owned by the registry. Scene integration binds a
 * data pointer before starting a stateful system and keeps that memory alive
 * until the system is stopped and unbound.
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
#include <module/ldk_entity.h>
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

/**
 * Execution bucket for a system update callback.
 *
 * UPDATE is zero intentionally so a zero-initialized descriptor gets the
 * normal/default gameplay bucket.
 */
typedef enum LDKSystemBucket
{
  LDK_SYSTEM_BUCKET_UPDATE = 0,
  LDK_SYSTEM_BUCKET_PRE_UPDATE,
  LDK_SYSTEM_BUCKET_POST_UPDATE,
  LDK_SYSTEM_BUCKET_RENDER,
  LDK_SYSTEM_BUCKET_COUNT
} LDKSystemBucket;

typedef enum LDKSystemFlags
{
  LDK_SYSTEM_FLAG_NONE = 0,
  LDK_SYSTEM_FLAG_ENABLED = 1 << 0,
  LDK_SYSTEM_FLAG_ENGINE_NATIVE = 1 << 1,
  LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED = 1 << 2
} LDKSystemFlags;

/**
 * Stable read-only view of the entity membership bound to a system.
 *
 * The view is valid for the duration of a system update callback. Structural
 * ECS changes performed by a callback are reflected before the next system
 * callback executes.
 * A grouping_id of 0 identifies the empty, unbound group.
 */
typedef struct LDKEntityGroup
{
  u64 grouping_id;
  const LDKEntity *entities;
  u32 count;
} LDKEntityGroup;

struct LDKRoot;

typedef bool (*LDKSystemCallbackSyncFn)(void *user);

typedef int (*LDKSystemInitializeFn)(void *data);
typedef void (*LDKSystemUpdateFn)(
    void *data, const LDKEntityGroup *group, float dt);
typedef void (*LDKSystemTerminateFn)(void *data);

typedef struct LDKSystemDesc
{
  u64 id;
  const char *name;
  u32 flags;
  LDKSystemBucket bucket;
  i32 order;
  u32 data_size;
  LDKSystemInitializeFn initialize;
  LDKSystemUpdateFn update;
  LDKSystemTerminateFn terminate;
} LDKSystemDesc;

typedef struct LDKSystemRegistry
{
  void *internal;
  struct LDKRoot *root;
  u8 is_initialized;
  u8 is_started; /* Bucket lists are prepared. */
  u8 is_paused;  /* Execution is suspended, not individual state. */
} LDKSystemRegistry;

LDK_API bool ldk_system_registry_initialize(LDKSystemRegistry *registry);
LDK_API void ldk_system_registry_terminate(LDKSystemRegistry *registry);
LDK_API bool ldk_system_registry_register(
    LDKSystemRegistry *registry, const LDKSystemDesc *desc);
LDK_API bool ldk_system_registry_unregister(
    LDKSystemRegistry *registry, u64 id);
LDK_API bool ldk_system_registry_find_by_id(
    LDKSystemRegistry *registry, u64 id, LDKSystemDesc *out);
LDK_API u32 ldk_system_registry_count(const LDKSystemRegistry *registry);
LDK_API bool ldk_system_registry_at(
    const LDKSystemRegistry *registry, u32 index, LDKSystemDesc *out);
LDK_API bool ldk_system_registry_clear(LDKSystemRegistry *registry);

/** Prepare the bucket lists without initializing individual systems. */
LDK_API bool ldk_system_registry_start(LDKSystemRegistry *registry);

/** Terminate all active systems in reverse registration order and unprepare. */
LDK_API bool ldk_system_registry_stop(LDKSystemRegistry *registry);

/**
 * Start/stop a registered system without changing the bucket lists.
 * Repeated start/stop calls succeed without repeating callbacks. A failed
 * initialize callback is followed by terminate so partially initialized state
 * can be released. A failed start leaves other systems unchanged.
 *
 * Stateful systems (data_size > 0) must have data bound before start.
 */
LDK_API bool ldk_system_registry_system_start(
    LDKSystemRegistry *registry, u64 id);
LDK_API bool ldk_system_registry_system_stop(
    LDKSystemRegistry *registry, u64 id);
LDK_API bool ldk_system_registry_system_is_started(
    const LDKSystemRegistry *registry, u64 id);

/**
 * Bind scene-owned system instance data. The registry never allocates or frees
 * this pointer. Data cannot be replaced while the system is initialized.
 * Passing NULL unbinds the current instance.
 */
LDK_API bool ldk_system_registry_system_data_set(
    LDKSystemRegistry *registry, u64 id, void *data);
LDK_API void *ldk_system_registry_system_data_get(
    const LDKSystemRegistry *registry, u64 id);

/**
 * Change the entity group delivered to a registered system's update callback.
 * This is scene binding state, not part of LDKSystemDesc or system lifecycle.
 * Passing NULL binds the system to the empty group.
 */
LDK_API bool ldk_system_registry_system_group_set(LDKSystemRegistry *registry,
    u64 id, const LDKEntityGroup *group);
LDK_API const LDKEntityGroup *ldk_system_registry_system_group_get(
    const LDKSystemRegistry *registry, u64 id);

/**
 * Install an engine-side synchronization point invoked after each executed
 * update callback. The system registry does not interpret the callback; the
 * ECS uses it to commit deferred grouping membership changes safely between
 * systems. Passing NULL clears the handler.
 */
LDK_API bool ldk_system_registry_callback_sync_set(LDKSystemRegistry *registry,
    LDKSystemCallbackSyncFn fn, void *user);

/** Suspend/resume execution without terminating individual systems. */
LDK_API bool ldk_system_registry_pause(LDKSystemRegistry *registry);
LDK_API bool ldk_system_registry_resume(LDKSystemRegistry *registry);
LDK_API bool ldk_system_registry_is_paused(
    const LDKSystemRegistry *registry);

/** True while a bucket or a system lifecycle callback is executing. */
LDK_API bool ldk_system_registry_is_busy(const LDKSystemRegistry *registry);

/**
 * Run initialized, enabled systems whose single update callback belongs to the
 * requested bucket. While paused, only systems with RUN_WHEN_PAUSED execute.
 * Structural and lifecycle changes are forbidden during update and lifecycle
 * callbacks.
 */
LDK_API bool ldk_system_registry_run_bucket(
    LDKSystemRegistry *registry, LDKSystemBucket bucket, float dt);
LDK_API bool ldk_system_registry_has(
    const LDKSystemRegistry *registry, u64 id);

#ifdef __cplusplus
}
#endif

#endif // LDK_SYSTEM_H
