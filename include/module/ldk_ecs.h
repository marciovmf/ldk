/**
 * @file   ldk_ecs.h
 * @brief  ECS facade module.
 *
 * Thin convenience facade over Entity, Component and System modules.
 * This module does not replace the direct submodule APIs.
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

typedef struct LDKECS
{
  LDKEntityRegistry entity;
  LDKComponentRegistry component;
  LDKSystemRegistry system;
} LDKECS;


  // ---------------------------------------------------------------------------
  // lifecycle
  // ---------------------------------------------------------------------------
  LDK_API bool ldk_ecs_initialize(LDKECS* context, u32 entity_page_capacity, u32 entity_initial_pages);
  LDK_API void ldk_ecs_terminate(void);

  // ---------------------------------------------------------------------------
  // Entity lifecycle
  // ---------------------------------------------------------------------------
  LDK_API LDKEntity ldk_ecs_entity_create(void);
  LDK_API void ldk_ecs_entity_destroy(LDKEntity entity);

  // ---------------------------------------------------------------------------
  // Component management
  // ---------------------------------------------------------------------------
  LDK_API void* ldk_ecs_component_add(LDKEntity entity, u32 component_type, const void* initial_value);
  LDK_API void* ldk_ecs_component_get(LDKEntity entity, u32 component_type);
  LDK_API const void* ldk_ecs_component_get_const(LDKEntity entity, u32 component_type);
  LDK_API bool ldk_ecs_component_remove(LDKEntity entity, u32 component_type);
  LDK_API bool ldk_ecs_component_register(const LDKComponentDesc* desc);
  LDK_API bool ldk_ecs_system_register(const LDKSystemDesc* desc);
  LDK_API bool ldk_ecs_system_unregister(u64 id);


#ifdef LDK_ENGINE
  // ---------------------------------------------------------------------------
  // Component management
  // ---------------------------------------------------------------------------
  LDK_API LDKEntityRegistry* ldk_ecs_entity_registry_get(void);
  LDK_API LDKComponentRegistry* ldk_ecs_component_registry_get(void);
  LDK_API LDKSystemRegistry* ldk_ecs_system_registry_get(void);

  LDK_API bool ldk_ecs_system_registry_start(LDKECS* context);
  LDK_API bool ldk_ecs_system_bucket_run(LDKECS* context, LDKSystemBucket bucket, float delta_time);
  LDK_API bool ldk_ecs_system_registry_stop(LDKECS* context);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_ECS_H
