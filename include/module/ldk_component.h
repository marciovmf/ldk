/**
 * # Component registry
 *
 * Component registry and storage
 * Manages component type registration and packed storage for entities.
 *
 * # Components
 *
 * Components are plain C structs annotated with `//@component`.
 * Comet reads the annotation at build time and generates the component type,
 * registration and field metadata used by the ECS, scene serialization and
 * editor.
 *
 * Example:
 *
 *   //@component
 *   typedef struct HealthComponent
 *   {
 *     float health;
 *
 *     //@inspect min=0 max=100
 *     float max_health;
 *
 *     //@inspect hidden runtime
 *     bool dead;
 *   } HealthComponent;
 *
 * Fields are exposed automatically. `//@inspect` can optionally control how a
 * field is presented or serialized. Inspector groups can be created with
 * `//@begin_group "Name"` and `//@end_group`.
 *
 * Component structs do not need to be registered manually.
 */

#ifndef LDK_COMPONENT_H
#define LDK_COMPONENT_H

#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * @brief Callback used when a component is attached to an entity.
   * @param entity_registry Entity registry used by this operation.
   * @param component_registry Component registry used by this operation.
   * @param entity Entity to inspect or modify.
   * @param component Component used by this operation.
   * @param component_index Zero-based index of the component entry.
   * @param initial_value Initial value used by this operation.
   * @param user Opaque caller-supplied data passed to the callback.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  typedef bool (*LDKComponentAttachFn)(LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry, LDKEntity entity, void* component, u32 component_index, const void* initial_value, void* user);

  /**
   * @brief Callback used when component instance data is destroyed.
   * @param entity_registry Entity registry used by this operation.
   * @param component_registry Component registry used by this operation.
   * @param entity Entity to inspect or modify.
   * @param component Component used by this operation.
   * @param component_index Zero-based index of the component entry.
   * @param user Opaque caller-supplied data passed to the callback.
   * @group ldk.module.component
   */
  typedef void (*LDKComponentDestroyFn)( LDKEntityRegistry* entity_registry, LDKComponentRegistry* component_registry, LDKEntity entity, void* component, u32 component_index, void* user);

  /**
   * A descriptor that identifies a Component.
   * Components are plain data structures that can be attached to entities.
   * Component types needs to be registered before they are used.
   * @group ldk.module.component
   */
  typedef enum LDKComponentFlags
  {
    LDK_COMPONENT_FLAG_NONE = 0,
    LDK_COMPONENT_FLAG_HIDE_IN_INSPECTOR = 1 << 0
  } LDKComponentFlags;

  /** @group ldk.module.component */
  typedef struct LDKComponentDesc
  {
    const char* name;
    u32 type;
    u32 entry_size;
    u32 initial_capacity;
    u32 flags;
    const u32* required_components;
    u32 required_component_count;
    LDKComponentAttachFn attach;
    LDKComponentDestroyFn destroy;
    void* user;
  } LDKComponentDesc;

  /** @group ldk.module.component */
  typedef struct LDKRegisteredComponent
  {
    LDKComponentDesc desc;
    XArray* store;
    XArray* owners;
  } LDKRegisteredComponent;

  X_HASHTABLE_TYPE_NAMED(u32, LDKRegisteredComponent, u32_registered_component);

  /** @group ldk.module.component */
  typedef struct LDKComponentRegistry
  {
    XHashtable_u32_registered_component* table;
  } LDKComponentRegistry;

  /**
   * @brief Initialize the component registry.
   * @param registry Registry on which the operation is performed.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_registry_initialize(LDKComponentRegistry* registry);
  /**
   * @brief Shut down the component registry.
   * @param registry Registry on which the operation is performed.
   * @group ldk.module.component
   */
  LDK_API void ldk_component_registry_terminate(LDKComponentRegistry* registry);
  /**
   * @brief Check whether a component type is registered.
   * @param registry Registry on which the operation is performed.
   * @param type Type identifier or kind used by the operation.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_is_registered(LDKComponentRegistry* registry, u32 type);
  /**
   * @brief Get the component desc.
   * @param registry Registry on which the operation is performed.
   * @param type Type identifier or kind used by the operation.
   * @param out_desc Receives the descriptor.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_desc_get(LDKComponentRegistry* registry,
      u32 type, LDKComponentDesc* out_desc);
  /**
   * @brief Get the component store.
   * @param registry Registry on which the operation is performed.
   * @param type Type identifier or kind used by the operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.module.component
   */
  LDK_API XArray* ldk_component_store_get(LDKComponentRegistry* registry, u32 type);
  /**
   * @brief Get the component owners.
   * @param registry Registry on which the operation is performed.
   * @param type Type identifier or kind used by the operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.module.component
   */
  LDK_API XArray* ldk_component_owners_get(LDKComponentRegistry* registry, u32 type);
  /**
   * @brief Detach a component from an entity.
   * @param registry Registry on which the operation is performed.
   * @param entity_system Entity system used by this operation.
   * @param entity Entity to inspect or modify.
   * @param component_type Registered ECS component type identifier.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_detach(LDKComponentRegistry* registry,
      LDKEntityRegistry* entity_system, LDKEntity entity, u32 component_type);
  /**
   * Removes (destroys) ALL components, including core engine components like Transform.
   * Intended for entity destruction only.
   * @param registry Registry on which the operation is performed.
   * @param entity_system Entity system used by this operation.
   * @param entity Entity to inspect or modify.
   * @group ldk.module.component
   */
  LDK_API void ldk_component_registry_remove_all(LDKComponentRegistry* registry,
      LDKEntityRegistry* entity_system, LDKEntity entity);
  /**
   * @brief Create the component.
   * @param module Module used by this operation.
   * @param component_type Registered ECS component type identifier.
   * @param component_index Zero-based index of the component entry.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.module.component
   */
  LDK_API void* ldk_component_create(LDKComponentRegistry* module, u32 component_type, u32* component_index);
  /**
   * @brief Get the component.
   * @param module Module used by this operation.
   * @param component_type Registered ECS component type identifier.
   * @param component_index Zero-based index of the component entry.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.module.component
   */
  LDK_API void* ldk_component_get(LDKComponentRegistry* module, u32 component_type, u32 component_index);
  /**
   * @brief Destroy the component.
   * @param module Module used by this operation.
   * @param entity_module Entity module used by this operation.
   * @param component_type Registered ECS component type identifier.
   * @param component_index Zero-based index of the component entry.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_destroy(LDKComponentRegistry* module,
      LDKEntityRegistry* entity_module, u32 component_type, u32 component_index);
  /**
   * @brief Register the component.
   * @param registry Registry on which the operation is performed.
   * @param desc Description or configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_register(LDKComponentRegistry* registry, const LDKComponentDesc* desc);
  /**
   * @brief Attach a component to an entity.
   * @param registry Registry on which the operation is performed.
   * @param entity_registry Entity registry used by this operation.
   * @param entity Entity to inspect or modify.
   * @param component_type Registered ECS component type identifier.
   * @param component_index Zero-based index of the component entry.
   * @param initial_value Initial value used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.module.component
   */
  LDK_API bool ldk_component_attach(LDKComponentRegistry* registry, LDKEntityRegistry* entity_registry,
      LDKEntity entity, u32 component_type, u32 component_index, const void* initial_value);
  /**
   * @brief Release data associated with a component instance.
   * @param registry Registry on which the operation is performed.
   * @param entity_registry Entity registry used by this operation.
   * @param entity Entity to inspect or modify.
   * @param component_type Registered ECS component type identifier.
   * @param component_index Zero-based index of the component entry.
   * @group ldk.module.component
   */
  LDK_API void ldk_component_destroy_data(LDKComponentRegistry* registry, LDKEntityRegistry* entity_registry,
      LDKEntity entity, u32 component_type, u32 component_index);

  /**
   * @brief Get the registered name of a component type.
   * @param registry Registry on which the operation is performed.
   * @param type Type identifier or kind used by the operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.module.component
   */
  LDK_API const char* ldk_component_name_get(LDKComponentRegistry* registry, u32 type);

#ifdef __cplusplus
}
#endif

#endif
