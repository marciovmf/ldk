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


  LDK_API bool ldk_ecs_initialize(LDKECS* context, u32 entity_page_capacity, u32 entity_initial_pages);
  LDK_API void ldk_ecs_terminate(void);
  LDK_API LDKEntity ldk_ecs_create_entity(void);
  LDK_API void ldk_ecs_destroy_entity(LDKEntity entity);
  LDK_API void* ldk_ecs_add_component(LDKEntity entity, u32 component_type, const void* initial_value);
  LDK_API void* ldk_ecs_get_component(LDKEntity entity, u32 component_type);
  LDK_API const void* ldk_ecs_get_component_const(LDKEntity entity, u32 component_type);
  LDK_API bool ldk_ecs_remove_component(LDKEntity entity, u32 component_type);
  LDK_API bool ldk_ecs_register_component(const char* name, u32 type, u32 entry_size,
      u32 initial_capacity, LDKComponentAttachFn attach, LDKComponentDestroyFn destroy, void* user);
  LDK_API bool ldk_ecs_register_system(const LDKSystemDesc* desc);
  LDK_API bool ldk_ecs_unregister_system(u64 id);

  LDK_API LDKEntityRegistry* ldk_ecs_entity_registry(void);
  LDK_API LDKComponentRegistry* ldk_ecs_component_registry(void);
  LDK_API LDKSystemRegistry* ldk_ecs_system_registry(void);

#ifdef LDK_ENGINE
  LDK_API bool ldk_ecs_system_registry_start(LDKECS* context);
  LDK_API bool ldk_ecs_run_system_bucket(LDKECS* context, LDKSystemBucket bucket, float delta_time);
  LDK_API bool ldk_ecs_system_registry_stop(LDKECS* context);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_ECS_H
