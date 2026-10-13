/**
 * # Entity-component-system facade
 *
 * ECS facade module.
 *
 * Thin convenience facade over Entity, Component and System modules.
 * This module is prefered to be used on user code while the direct submodule
 * access is reserved for internal engine use.
 */

#ifndef LDK_ECS_H
#define LDK_ECS_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <module/ldk_component.h>
#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group modules.ecs.groupings */
typedef struct LDKGroupingDesc
{
  u64 id;
  const char *name;
  const u32 *component_types;
  u32 component_count;
} LDKGroupingDesc;

/** @group modules.ecs.lifecycle */
typedef struct LDKECS
{
  LDKEntityRegistry entity;
  LDKComponentRegistry component;
  LDKSystemRegistry system;
  void *grouping_internal;
} LDKECS;

// ---------------------------------------------------------------------------
// ECS lifecycle
// ---------------------------------------------------------------------------
/**
 * @brief Initialize the ECS.
 * @param context Context used by this operation.
 * @param entity_page_capacity Entities allocated per page.
 * @param entity_initial_pages Number of entity pages initially allocated.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.lifecycle
 */
LDK_API bool ldk_ecs_initialize(
    LDKECS *context, u32 entity_page_capacity, u32 entity_initial_pages);
/**
 * @brief Shut down the ECS.
 * @group modules.ecs.lifecycle
 */
LDK_API void ldk_ecs_terminate(void);

// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------
/**
 * @brief Create the ECS entity.
 * @return The requested value.
 * @group modules.ecs.entities
 */
LDK_API LDKEntity ldk_ecs_entity_create(void);
/**
 * @brief Destroy the ECS entity.
 * @param entity Entity to inspect or modify.
 * @group modules.ecs.entities
 */
LDK_API void ldk_ecs_entity_destroy(LDKEntity entity);

// ---------------------------------------------------------------------------
// Component management
// ---------------------------------------------------------------------------
/**
 * @brief Add the ECS component.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param initial_value Initial value used by this operation.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.components
 */
LDK_API void *ldk_ecs_component_add(
    LDKEntity entity, u32 component_type, const void *initial_value);
/**
 * @brief Get the ECS component.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.components
 */
LDK_API void *ldk_ecs_component_get(LDKEntity entity, u32 component_type);
/**
 * @brief Get read-only access to the ECS component.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.components
 */
LDK_API const void *ldk_ecs_component_get_const(
    LDKEntity entity, u32 component_type);
/**
 * @brief Remove the ECS component.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.components
 */
LDK_API bool ldk_ecs_component_remove(
    LDKEntity entity, u32 component_type);
/**
 * @brief Register the ECS component.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.components
 */
LDK_API bool ldk_ecs_component_register(const LDKComponentDesc *desc);

// ---------------------------------------------------------------------------
// Grouping management
// ---------------------------------------------------------------------------
/**
 * Register a grouping description from code. A grouping matches every
 * game entity that has all component types listed by the description.
 * Editor-internal entities and disabled requirements are excluded.
 * Empty requirements use the empty group, ID zero. desc.id is ignored.
 */
/** Zero means an empty requirement set (or an invalid declaration).
 * Hashes count and sorted unique u32 IDs in fixed little-endian byte order.
 * @param component_types Array of registered ECS component type identifiers.
 * @param component_count Number of component identifiers in the array.
 * @return Computed identifier or hash.
 * @group modules.ecs.groupings
 */
LDK_API u64 ldk_ecs_grouping_id(
    const u32 *component_types, u32 component_count);
/**
 * @brief Check whether the ECS component is enabled.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.ecs.components
 */
LDK_API bool ldk_ecs_component_is_enabled(LDKEntity entity, u32 component_type);
/**
 * @brief Enable or disable one component instance.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param enabled Enabled used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.components
 */
LDK_API bool ldk_ecs_component_enabled_set(
    LDKEntity entity, u32 component_type, bool enabled);
/**
 * @brief Register the ECS grouping.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_register(const LDKGroupingDesc *desc);
/**
 * @brief Unregister the ECS grouping.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_unregister(u64 id);
/**
 * @brief Find a registered ECS grouping by its stable ID.
 * @param id Identifier of the target object.
 * @param out Output object or buffer receiving the result.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_find_by_id(u64 id, LDKGroupingDesc *out);
/**
 * @brief Count the registered ECS group descriptions.
 * @return Number of matching entries.
 * @group modules.ecs.groupings
 */
LDK_API u32 ldk_ecs_grouping_count(void);
/**
 * @brief Get a registered ECS grouping by index.
 * @param index Zero-based index of the selected item.
 * @param out Output object or buffer receiving the result.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_at(u32 index, LDKGroupingDesc *out);
/**
 * @brief Get the runtime entity group matching a registered grouping ID.
 * @param id Identifier of the target object.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.groupings
 */
LDK_API const LDKEntityGroup *ldk_ecs_grouping_get(u64 id);

/** Legacy compatibility no-op. Project [groupings] sections are ignored.
 * @param ini_path Path to the INI configuration.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_configure_file(const char *ini_path);

/** True when a grouping originated in the current configuration file.
 * @param id Identifier of the target object.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_is_config_defined(u64 id);

// ---------------------------------------------------------------------------
// System management
// ---------------------------------------------------------------------------
/**
 * @brief Register the ECS system.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_register(const LDKSystemDesc *desc);
/**
 * @brief Unregister the ECS system.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_unregister(u64 id);

/** Start/stop an individual registered system without changing the catalog.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_start(u64 id);
/**
 * @brief Stop the ECS system.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_stop(u64 id);
/**
 * @brief Check whether the ECS system is started.
 * @param id Identifier of the target object.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_is_started(u64 id);

/** Suspend/resume execution without destroying individual system state.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_pause(void);
/**
 * @brief Resume the ECS system.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_resume(void);
/**
 * @brief Check whether the ECS system is paused.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.ecs.systems
 */
LDK_API bool ldk_ecs_system_is_paused(void);

// ---------------------------------------------------------------------------
// Entity iteraction
// ---------------------------------------------------------------------------
/**
 * @brief Callback visiting an ECS entity during iteration.
 * @param entity Entity to inspect or modify.
 * @param user Opaque caller-supplied data passed to the callback.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.lifecycle
 */
typedef bool (*LDKECSEntityIterFn)(LDKEntity entity, void *user);

/**
 * @brief Visit active ECS entities using a caller-provided callback.
 * @param fn Callback invoked by this operation.
 * @param user Opaque caller-supplied data passed to the callback.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.entities
 */
LDK_API bool ldk_ecs_entity_foreach(LDKECSEntityIterFn fn, void *user);
/**
 * @brief Count components attached to an ECS entity.
 * @param entity Entity to inspect or modify.
 * @return Number of matching entries.
 * @group modules.ecs.entities.components
 */
LDK_API u32 ldk_ecs_entity_component_count(LDKEntity entity);
/**
 * @brief Read the type ID of an entity component by index.
 * @param entity Entity to inspect or modify.
 * @param component_index Zero-based index of the component entry.
 * @param out_component_type Receives the resulting component type.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.entities.components
 */
LDK_API bool ldk_ecs_entity_component_type_at(
    LDKEntity entity, u32 component_index, u32 *out_component_type);
/**
 * @brief Get the ECS entity name.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.entities.names
 */
LDK_API const char *ldk_ecs_entity_name_get(LDKEntity entity);
/**
 * @brief Set the ECS entity name.
 * @param entity Entity to inspect or modify.
 * @param name Name identifying the object or setting.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.entities.names
 */
LDK_API bool ldk_ecs_entity_name_set(LDKEntity entity, const char *name);
/**
 * @brief Get the stored hash of an entity name.
 * @param entity Entity to inspect or modify.
 * @return Computed identifier or hash.
 * @group modules.ecs.entities.names
 */
LDK_API u64 ldk_ecs_entity_name_hash_get(LDKEntity entity);
/**
 * @brief Assign the stored hash of an entity name.
 * @param entity Entity to inspect or modify.
 * @param hash Hash used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.entities.names
 */
LDK_API bool ldk_ecs_entity_name_hash_set(LDKEntity entity, u64 hash);

  // ---------------------------------------------------------------------------
  // Entity iteraction
  // ---------------------------------------------------------------------------
  /**
   * @brief Callback visiting an ECS entity during iteration.
   * @param entity Entity to inspect or modify.
   * @param user Opaque caller-supplied data passed to the callback.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ecs.lifecycle
   */
  typedef bool (*LDKECSEntityIterFn)(LDKEntity entity, void* user);

  /**
   * @brief Visit active ECS entities using a caller-provided callback.
   * @param fn Callback invoked by this operation.
   * @param user Opaque caller-supplied data passed to the callback.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ecs.entities
   */
  LDK_API bool ldk_ecs_entity_foreach(LDKECSEntityIterFn fn, void* user);
  /**
   * @brief Count components attached to an ECS entity.
   * @param entity Entity to inspect or modify.
   * @return Number of matching entries.
   * @group modules.ecs.entities.components
   */
  LDK_API u32 ldk_ecs_entity_component_count(LDKEntity entity);
  /**
   * @brief Read the type ID of an entity component by index.
   * @param entity Entity to inspect or modify.
   * @param component_index Zero-based index of the component entry.
   * @param out_component_type Receives the resulting component type.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ecs.entities.components
   */
  LDK_API bool ldk_ecs_entity_component_type_at(LDKEntity entity, u32 component_index, u32* out_component_type);
  /**
   * @brief Get the ECS entity name.
   * @param entity Entity to inspect or modify.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group modules.ecs.entities.names
   */
  LDK_API const char* ldk_ecs_entity_name_get(LDKEntity entity);
  /**
   * @brief Set the ECS entity name.
   * @param entity Entity to inspect or modify.
   * @param name Name identifying the object or setting.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ecs.entities.names
   */
  LDK_API bool ldk_ecs_entity_name_set(LDKEntity entity, const char* name);

#if defined(LDK_ENGINE) || defined(LDK_EDITOR)
// ---------------------------------------------------------------------------
//  Engine and editor registry access
// ---------------------------------------------------------------------------
/**
 * @brief Get the ECS entity registry.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.entities
 */
LDK_API LDKEntityRegistry *ldk_ecs_entity_registry_get(void);
/**
 * @brief Get the ECS component registry.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.components
 */
LDK_API LDKComponentRegistry *ldk_ecs_component_registry_get(void);
/**
 * @brief Get the ECS system registry.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.ecs.systems.execution
 */
LDK_API LDKSystemRegistry *ldk_ecs_system_registry_get(void);
#endif

#ifdef LDK_ENGINE
/**
 * @brief Start the ECS system registry.
 * @param context Context used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems.execution
 */
LDK_API bool ldk_ecs_system_registry_start(LDKECS *context);
/**
 * @brief Execute the ECS system bucket.
 * @param context Context used by this operation.
 * @param bucket Bucket used by this operation.
 * @param delta_time Elapsed time since the preceding update, in seconds.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems.execution
 */
LDK_API bool ldk_ecs_system_bucket_run(
    LDKECS *context, LDKSystemBucket bucket, float delta_time);
/**
 * @brief Stop the ECS system registry.
 * @param context Context used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.systems.execution
 */
LDK_API bool ldk_ecs_system_registry_stop(LDKECS *context);
#endif

#if defined(LDK_ENGINE) || defined(LDK_EDITOR)
/**
 * Clear all runtime membership state for registered Entity Groups while
 * preserving their definitions. Intended for full scene/ECS entity resets.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.ecs.groupings
 */
LDK_API bool ldk_ecs_grouping_runtime_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_ECS_H
