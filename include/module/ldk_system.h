/**
 * # System registry and Comet annotations
 *
 * Comet generates system ids and LDKSystemDesc descriptors from annotations
 * placed before C declarations. Annotations are special single-line comments.
 * In the examples, replace `// @` with `//@` (remove the space) before use:
 * this avoids accidentally feeding these documentation examples to Comet.
 *
 * Stateless system (the annotated function is the update callback):
 *
 *   // @system name=HealthSystem components=LDK_COMPONENT_HealthComponent order=10
 *   void health_system_update(void *data, const LDKEntityGroup *group, float dt);
 *
 * Stateful system (the annotated struct contains scene-owned system data):
 *
 *   int damage_system_initialize(void *data);
 *   void damage_system_update(void *data, const LDKEntityGroup *group, float dt);
 *   void damage_system_terminate(void *data);
 *
 *   // @system name=DamageSystem components=LDK_COMPONENT_HealthComponent update=damage_system_update initialize=damage_system_initialize terminate=damage_system_terminate
 *   typedef struct DamageSystem
 *   {
 *     // @inspect min=0 label=Damage
 *     float damage;
 *     // @inspect runtime
 *     u32 hits;
 *   } DamageSystem;
 *
 * Stateful systems must be declared in a header (.h, .hh or .hpp). They
 * require at least one of initialize/init, update or terminate. Function
 * annotations must NOT specify update=: the annotated function is update.
 * They may still specify initialize/init and terminate. Callback declarations
 * must be available to the generated C code.
 *
 * All supported `@system` options (space-delimited key=value, one line):
 *
 *   name=ID         Public system name (must be a C identifier). Defaults
 *                   to the struct typedef name or annotated function name.
 *   components=A,B  Required ECS component-type identifiers; no spaces,
 *                   duplicates or expressions, maximum 16 identifiers.
 *                   If omitted the system has no component requirements.
 *   initialize=fn   Optional initialization callback (`init=fn` also works).
 *   update=fn       Update callback for a stateful system only.
 *   terminate=fn    Optional termination callback.
 *   flags=EXPR      Flags expression, default LDK_SYSTEM_FLAG_ENABLED.
 *   bucket=EXPR     Execution bucket, default LDK_SYSTEM_BUCKET_UPDATE.
 *   order=EXPR      Order within the bucket, default 0.
 *
 * Available flags: LDK_SYSTEM_FLAG_NONE, LDK_SYSTEM_FLAG_ENABLED,
 * LDK_SYSTEM_FLAG_ENGINE_NATIVE and LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED.
 * Combine flags without spaces using `|`. ENGINE_NATIVE indicates the
 * implementation comes from the engine; it is not needed for game systems.
 *
 * Available buckets: LDK_SYSTEM_BUCKET_PRE_UPDATE,
 * LDK_SYSTEM_BUCKET_UPDATE, LDK_SYSTEM_BUCKET_POST_UPDATE and
 * LDK_SYSTEM_BUCKET_RENDER. `order` specifies execution order in a bucket.
 * Options such as flags/bucket/order are emitted as C expressions, so their
 * identifiers must be defined when compiling the generated descriptors.
 *
 * Callback signatures:
 *   int  initialize(void *data);                              // optional
 *   void update(void *data, const LDKEntityGroup *group, float dt);
 *   void terminate(void *data);                               // optional
 *
 * `data` is the scene-owned instance for stateful systems; stateless systems
 * have data_size=0. `group` is the temporary view of entities matching the
 * component requirements and is valid during the callback. `dt` is elapsed
 * time in seconds. A system can omit update if it is lifecycle-only and
 * stateful. Systems are registered from generated descriptors, then selected
 * and started for a scene; creating a struct does not start the system.
 *
 * Comet generates LDK_SYSTEM_Name and LDK_SYSTEM_DESC_Name, accessed with
 * `ldk_system_id(Name)` and `ldk_system_desc(Name)`. For stateful systems,
 * Name in those symbols is the typedef name, even if name= changes the
 * display name; for stateless systems it is name=, or the function name.
 *
 * System struct fields use the same annotations as component fields:
 *   // @inspect readonly | runtime | hidden | slider | enum
 *   // @inspect min=N max=N widget=SUFFIX source=field label=Token
 *   // @begin_group "Group Name"     (nested groups are supported)
 *   // @end_group
 * See ldk_component.h for supported field types, widgets, flag behavior,
 * inspector metadata and grouping rules.
 *
 * All `@system` key=value options must appear on the annotation's own line.
 * Comet scans source text directly and does not ignore comment examples or
 * disabled blocks: avoid spelling a literal marker outside real declarations.
 * The registry belongs to the engine; game code should not own it directly.
 */

#ifndef LDK_SYSTEM_H
#define LDK_SYSTEM_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** IDs of builtin systems.
 * @group ldk.module.system
 */
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
 * @group ldk.module.system
 */
typedef enum LDKSystemBucket
{
  LDK_SYSTEM_BUCKET_UPDATE = 0,
  LDK_SYSTEM_BUCKET_PRE_UPDATE,
  LDK_SYSTEM_BUCKET_POST_UPDATE,
  LDK_SYSTEM_BUCKET_RENDER,
  LDK_SYSTEM_BUCKET_COUNT
} LDKSystemBucket;

/** @group ldk.module.system */
typedef enum LDKSystemFlags
{
  LDK_SYSTEM_FLAG_NONE = 0,
  LDK_SYSTEM_FLAG_ENABLED = 1 << 0,
  /* Implementation is provided by the engine rather than user code.
   * Registration and lifecycle remain game responsibilities. */
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
 * @group ldk.module.system
 */
typedef struct LDKEntityGroup
{
  u64 grouping_id;
  const LDKEntity *entities;
  u32 count;
} LDKEntityGroup;

struct LDKRoot;

/**
 * @brief Callback used to synchronize with system execution.
 * @param user Opaque caller-supplied data passed to the callback.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
typedef bool (*LDKSystemCallbackSyncFn)(void *user);

/**
 * @brief Callback called during ECS system initialization.
 * @param data Source data or system-owned context for this operation.
 * @return The resulting integer value.
 * @group ldk.module.system
 */
typedef int (*LDKSystemInitializeFn)(void *data);
/**
 * @brief Callback called when an ECS system updates a group.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group ldk.module.system
 */
typedef void (*LDKSystemUpdateFn)(
    void *data, const LDKEntityGroup *group, float dt);
/**
 * @brief Callback called during ECS system termination.
 * @param data Source data or system-owned context for this operation.
 * @group ldk.module.system
 */
typedef void (*LDKSystemTerminateFn)(void *data);

/** @group ldk.module.system */
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
  const u32 *component_types;
  u32 component_count;
} LDKSystemDesc;

/** @group ldk.module.system */
typedef struct LDKSystemRegistry
{
  void *internal;
  struct LDKRoot *root;
  u8 is_initialized;
  u8 is_started; /* Bucket lists are prepared. */
  u8 is_paused;  /* Execution is suspended, not individual state. */
} LDKSystemRegistry;

/** Descriptors for systems implemented by the engine. Games own registration.
 * @return Number of matching entries.
 * @group ldk.module.system
 */
LDK_API u32 ldk_engine_system_descriptor_count(void);
/**
 * @brief Get the engine system descriptor.
 * @param index Zero-based index of the selected item.
 * @param out Output object or buffer receiving the result.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_engine_system_descriptor_get(
    u32 index, LDKSystemDesc *out);

/**
 * @brief Initialize the system registry.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_initialize(LDKSystemRegistry *registry);
/**
 * @brief Shut down the system registry.
 * @param registry Registry on which the operation is performed.
 * @group ldk.module.system
 */
LDK_API void ldk_system_registry_terminate(LDKSystemRegistry *registry);
/**
 * @brief Register the system registry.
 * @param registry Registry on which the operation is performed.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_register(
    LDKSystemRegistry *registry, const LDKSystemDesc *desc);
/**
 * @brief Unregister the system registry.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_unregister(
    LDKSystemRegistry *registry, u64 id);
/**
 * @brief Find a registered system by its stable ID.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @param out Output object or buffer receiving the result.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_find_by_id(
    LDKSystemRegistry *registry, u64 id, LDKSystemDesc *out);
/**
 * @brief Count registered systems.
 * @param registry Registry on which the operation is performed.
 * @return Number of matching entries.
 * @group ldk.module.system
 */
LDK_API u32 ldk_system_registry_count(const LDKSystemRegistry *registry);
/**
 * @brief Get a system registered at a particular index.
 * @param registry Registry on which the operation is performed.
 * @param index Zero-based index of the selected item.
 * @param out Output object or buffer receiving the result.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_at(
    const LDKSystemRegistry *registry, u32 index, LDKSystemDesc *out);
/**
 * @brief Clear the state of the system registry.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_clear(LDKSystemRegistry *registry);

/** Prepare the bucket lists without initializing individual systems.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_start(LDKSystemRegistry *registry);

/** Terminate all active systems in reverse registration order and unprepare.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_stop(LDKSystemRegistry *registry);

/**
 * Start/stop a registered system without changing the bucket lists.
 * Repeated start/stop calls succeed without repeating callbacks. A failed
 * initialize callback is followed by terminate so partially initialized state
 * can be released. A failed start leaves other systems unchanged.
 *
 * Stateful systems (data_size > 0) must have data bound before start.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_start(
    LDKSystemRegistry *registry, u64 id);
/**
 * @brief Stop the system registry system.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_stop(
    LDKSystemRegistry *registry, u64 id);
/**
 * @brief Check whether the system registry system is started.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_is_started(
    const LDKSystemRegistry *registry, u64 id);

/** Suspend only a system's update callback without terminating it.
 * Intended for temporary editor play-mode debugging; the flag is cleared
 * when the system is terminated. This is separate from registry pause.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_pause_set(
    LDKSystemRegistry *registry, u64 id, bool paused);

/** Query the system's individual update suspension state.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_is_paused(
    const LDKSystemRegistry *registry, u64 id);

/** True while a bucket or a system lifecycle callback is executing.
 * @param registry Registry on which the operation is performed.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_is_busy(const LDKSystemRegistry* registry);

/** True while a bucket or a system lifecycle callback is executing.
 * @param registry Registry on which the operation is performed.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_is_busy(const LDKSystemRegistry* registry);

/**
 * Bind scene-owned system instance data. The registry never allocates or frees
 * this pointer. Data cannot be replaced while the system is initialized.
 * Passing NULL unbinds the current instance.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @param data Source data or system-owned context for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_data_set(
    LDKSystemRegistry *registry, u64 id, void *data);
/**
 * @brief Get the system registry system data.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group ldk.module.system
 */
LDK_API void *ldk_system_registry_system_data_get(
    const LDKSystemRegistry *registry, u64 id);

/**
 * Change the entity group delivered to a registered system's update callback.
 * Engine-internal binding, resolved by the ECS from LDKSystemDesc requirements.
 * Scene start/stop must preserve it. Game code should use the ECS facade.
 * Passing NULL binds the system to the empty group.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @param group Entity group used for this system update.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_system_group_set(LDKSystemRegistry *registry,
    u64 id, const LDKEntityGroup *group);
/**
 * @brief Get the system registry system group.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group ldk.module.system
 */
LDK_API const LDKEntityGroup *ldk_system_registry_system_group_get(
    const LDKSystemRegistry *registry, u64 id);

/**
 * Install an engine-side synchronization point invoked after each executed
 * update callback. The system registry does not interpret the callback; the
 * ECS uses it to commit deferred grouping membership changes safely between
 * systems. Passing NULL clears the handler.
 * @param registry Registry on which the operation is performed.
 * @param fn Callback invoked by this operation.
 * @param user Opaque caller-supplied data passed to the callback.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_callback_sync_set(LDKSystemRegistry *registry,
    LDKSystemCallbackSyncFn fn, void *user);

/** Suspend/resume execution without terminating individual systems.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_pause(LDKSystemRegistry *registry);
/**
 * @brief Resume the system registry.
 * @param registry Registry on which the operation is performed.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_resume(LDKSystemRegistry *registry);
/**
 * @brief Check whether the system registry is paused.
 * @param registry Registry on which the operation is performed.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_is_paused(
    const LDKSystemRegistry *registry);

/** True while a bucket or a system lifecycle callback is executing.
 * @param registry Registry on which the operation is performed.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_is_busy(const LDKSystemRegistry *registry);

/**
 * Run initialized, enabled systems whose single update callback belongs to the
 * requested bucket. While paused, only systems with RUN_WHEN_PAUSED execute.
 * Structural and lifecycle changes are forbidden during update and lifecycle
 * callbacks.
 * @param registry Registry on which the operation is performed.
 * @param bucket Bucket used by this operation.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_run_bucket(
    LDKSystemRegistry *registry, LDKSystemBucket bucket, float dt);
/**
 * @brief Check whether a system ID is registered.
 * @param registry Registry on which the operation is performed.
 * @param id Identifier of the target object.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.module.system
 */
LDK_API bool ldk_system_registry_has(
    const LDKSystemRegistry *registry, u64 id);

#ifdef __cplusplus
}
#endif

#endif // LDK_SYSTEM_H
