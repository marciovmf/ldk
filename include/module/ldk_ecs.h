/**
 * @file   ldk_ecs.h
 * @brief  ECS facade module.
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

typedef struct LDKGroupingDesc
{
  u64 id;
  const char *name;
  const u32 *component_types;
  u32 component_count;
} LDKGroupingDesc;

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
LDK_API bool ldk_ecs_initialize(
    LDKECS *context, u32 entity_page_capacity, u32 entity_initial_pages);
LDK_API void ldk_ecs_terminate(void);

// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------
LDK_API LDKEntity ldk_ecs_entity_create(void);
LDK_API void ldk_ecs_entity_destroy(LDKEntity entity);

// ---------------------------------------------------------------------------
// Component management
// ---------------------------------------------------------------------------
LDK_API void *ldk_ecs_component_add(
    LDKEntity entity, u32 component_type, const void *initial_value);
LDK_API void *ldk_ecs_component_get(LDKEntity entity, u32 component_type);
LDK_API const void *ldk_ecs_component_get_const(
    LDKEntity entity, u32 component_type);
LDK_API bool ldk_ecs_component_remove(
    LDKEntity entity, u32 component_type);
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
 * Hashes count and sorted unique u32 IDs in fixed little-endian byte order. */
LDK_API u64 ldk_ecs_grouping_id(
    const u32 *component_types, u32 component_count);
LDK_API bool ldk_ecs_component_is_enabled(LDKEntity entity, u32 component_type);
LDK_API bool ldk_ecs_component_enabled_set(
    LDKEntity entity, u32 component_type, bool enabled);
LDK_API bool ldk_ecs_grouping_register(const LDKGroupingDesc *desc);
LDK_API bool ldk_ecs_grouping_unregister(u64 id);
LDK_API bool ldk_ecs_grouping_find_by_id(u64 id, LDKGroupingDesc *out);
LDK_API u32 ldk_ecs_grouping_count(void);
LDK_API bool ldk_ecs_grouping_at(u32 index, LDKGroupingDesc *out);
LDK_API const LDKEntityGroup *ldk_ecs_grouping_get(u64 id);

/** Legacy compatibility no-op. Project [groupings] sections are ignored. */
LDK_API bool ldk_ecs_grouping_configure_file(const char *ini_path);

/** True when a grouping originated in the current configuration file. */
LDK_API bool ldk_ecs_grouping_is_config_defined(u64 id);

// ---------------------------------------------------------------------------
// System management
// ---------------------------------------------------------------------------
LDK_API bool ldk_ecs_system_register(const LDKSystemDesc *desc);
LDK_API bool ldk_ecs_system_unregister(u64 id);

/** Start/stop an individual registered system without changing the catalog. */
LDK_API bool ldk_ecs_system_start(u64 id);
LDK_API bool ldk_ecs_system_stop(u64 id);
LDK_API bool ldk_ecs_system_is_started(u64 id);

/** Suspend/resume execution without destroying individual system state. */
LDK_API bool ldk_ecs_system_pause(void);
LDK_API bool ldk_ecs_system_resume(void);
LDK_API bool ldk_ecs_system_is_paused(void);

// ---------------------------------------------------------------------------
// Entity iteraction
// ---------------------------------------------------------------------------
typedef bool (*LDKECSEntityIterFn)(LDKEntity entity, void *user);

LDK_API bool ldk_ecs_entity_foreach(LDKECSEntityIterFn fn, void *user);
LDK_API u32 ldk_ecs_entity_component_count(LDKEntity entity);
LDK_API bool ldk_ecs_entity_component_type_at(
    LDKEntity entity, u32 component_index, u32 *out_component_type);
LDK_API const char *ldk_ecs_entity_name_get(LDKEntity entity);
LDK_API bool ldk_ecs_entity_name_set(LDKEntity entity, const char *name);

  // ---------------------------------------------------------------------------
  // Entity iteraction
  // ---------------------------------------------------------------------------
  typedef bool (*LDKECSEntityIterFn)(LDKEntity entity, void* user);

  LDK_API bool ldk_ecs_entity_foreach(LDKECSEntityIterFn fn, void* user);
  LDK_API u32 ldk_ecs_entity_component_count(LDKEntity entity);
  LDK_API bool ldk_ecs_entity_component_type_at(LDKEntity entity, u32 component_index, u32* out_component_type);
  LDK_API const char* ldk_ecs_entity_name_get(LDKEntity entity);
  LDK_API bool ldk_ecs_entity_name_set(LDKEntity entity, const char* name);

#if defined(LDK_ENGINE) || defined(LDK_EDITOR)
// ---------------------------------------------------------------------------
//  Engine and editor registry access
// ---------------------------------------------------------------------------
LDK_API LDKEntityRegistry *ldk_ecs_entity_registry_get(void);
LDK_API LDKComponentRegistry *ldk_ecs_component_registry_get(void);
LDK_API LDKSystemRegistry *ldk_ecs_system_registry_get(void);
#endif

#ifdef LDK_ENGINE
LDK_API bool ldk_ecs_system_registry_start(LDKECS *context);
LDK_API bool ldk_ecs_system_bucket_run(
    LDKECS *context, LDKSystemBucket bucket, float delta_time);
LDK_API bool ldk_ecs_system_registry_stop(LDKECS *context);
#endif

#if defined(LDK_ENGINE) || defined(LDK_EDITOR)
/**
 * Clear all runtime membership state for registered Entity Groups while
 * preserving their definitions. Intended for full scene/ECS entity resets.
 */
LDK_API bool ldk_ecs_grouping_runtime_reset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_ECS_H
