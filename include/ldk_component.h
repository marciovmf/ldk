/**
 * @file   ldk_component.h
 * @brief  Component registry and storage
 *
 * Manages component type registration and packed storage for entities.
 */

#ifndef LDK_COMPONENT_H
#define LDK_COMPONENT_H

#include <ldk_common.h>
#include <ldk_entity.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * A descriptor that identifies a Component.
   * Components are plain data structures that can be attached to entities.
   * Component types needs to be registered before they are used.
   */
  typedef struct LDKComponentDesc
  {
    const char* name;
    u32 type;
    u32 entry_size;
    u32 initial_capacity;
  } LDKComponentDesc;

  typedef struct LDKRegisteredComponent
  {
    LDKComponentDesc desc;
    XArray* store;
    XArray* owners;
  } LDKRegisteredComponent;

  X_HASHTABLE_TYPE_NAMED(u32, LDKRegisteredComponent, u32_registered_component);

  typedef struct LDKComponentRegistry
  {
    XHashtable_u32_registered_component* table;
  } LDKComponentRegistry;

  LDK_API bool ldk_component_registry_initialize(LDKComponentRegistry* registry);
  LDK_API void ldk_component_registry_terminate(LDKComponentRegistry* registry);
  LDK_API bool ldk_component_register(LDKComponentRegistry* registry, const char* name, u32 type, u32 entry_size, u32 initial_capacity);
  LDK_API bool ldk_component_is_registered(LDKComponentRegistry* registry, u32 type);
  LDK_API XArray* ldk_component_get_store(LDKComponentRegistry* registry, u32 type);
  LDK_API XArray* ldk_component_get_owners(LDKComponentRegistry* registry, u32 type);
  LDK_API bool ldk_component_remove_entity(LDKComponentRegistry* registry, LDKEntityRegistry* entity_system, LDKEntity entity, u32 component_type);
  LDK_API void ldk_component_registry_remove_all(LDKComponentRegistry* registry, LDKEntityRegistry* entity_system, LDKEntity entity);
  LDK_API void* ldk_component_create(LDKComponentRegistry* module, u32 component_type, u32* component_index);
  LDK_API void* ldk_component_get(LDKComponentRegistry* module, u32 component_type, u32 component_index);
  LDK_API bool ldk_component_destroy(LDKComponentRegistry* module, LDKEntityRegistry* entity_module, u32 component_type, u32 component_index);

#ifdef __cplusplus
}
#endif

#endif
