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
  } LDKRegisteredComponent;

  X_HASHTABLE_TYPE_NAMED(u32, LDKRegisteredComponent, u32_registered_component)

    typedef struct LDKComponentRegistry
    {
      XHashtable_u32_registered_component* table;
    } LDKComponentRegistry;

  bool ldk_component_registry_initialize(LDKComponentRegistry* registry);
  void ldk_component_registry_terminate(LDKComponentRegistry* registry);

  bool ldk_component_register(
      LDKComponentRegistry* registry,
      const char* name,
      u32 type,
      u32 entry_size,
      u32 initial_capacity);

  bool ldk_component_is_registered(LDKComponentRegistry* registry, u32 type);
  XArray* ldk_component_get_store(LDKComponentRegistry* registry, u32 type);

  bool ldk_component_remove_entity(
      LDKComponentRegistry* registry,
      LDKEntitySystem* entity_system,
      LDKEntity entity,
      u32 component_type);

  void ldk_component_registry_remove_all(
      LDKComponentRegistry* registry,
      LDKEntitySystem* entity_system,
      LDKEntity entity);

#ifdef __cplusplus
}
#endif

#endif
