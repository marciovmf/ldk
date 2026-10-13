/**
 * # Entity registry
 *
 * Entity system (handles, lifecycle, component binding)
 *
 * Defines entity creation, destruction, and component associations.
 */

#ifndef LDK_ENTITY_H
#define LDK_ENTITY_H

#include <ldk_common.h>
#include <stdx/stdx_common.h>
#include <stdx/stdx_hpool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LDK_ENTITY_MAX_COMPONENTS
/** @group modules.entity.components */
#define LDK_ENTITY_MAX_COMPONENTS 16
#endif

#ifndef LDK_ENTITY_NAME_MAX_LEN
/** @group modules.entity.names */
#define LDK_ENTITY_NAME_MAX_LEN   32
#endif

/** @group modules.entity.components */
#define LDK_ENTITY_INVALID_COMPONENT_INDEX UINT32_MAX

/** @group modules.entity.lifecycle */
typedef XHandle LDKEntity;
/** @group modules.entity.components */
typedef struct LDKComponentRegistry LDKComponentRegistry;
struct LDKComponentRegistry;

/** @group modules.entity.flags */
typedef enum LDKEntityInternalFlags
{
  LDK_ENTITY_INTERNAL_NONE           = 0,
  LDK_ENTITY_INTERNAL_PENDING_DELETE = 1 << 0,
  LDK_ENTITY_INTERNAL_HAS_TRANSFORM  = 1 << 1,
  LDK_ENTITY_INTERNAL_HAS_CAMERA     = 1 << 2,
  LDK_ENTITY_INTERNAL_HAS_RENDERABLE = 1 << 3,
  LDK_ENTITY_INTERNAL_HAS_LIGHT      = 1 << 4,
  LDK_ENTITY_INTERNAL_EDITOR         = 1 << 5
} LDKEntityInternalFlags;

/**
 * Serialized IDs of engine-owned components. These values are persistent:
 * once assigned, a retired ID must not be reused for another component.
 * User component IDs must stay below the engine-reserved high-ID range.
 * @group modules.entity.components
 */
typedef enum LDKComponentInstanceFlags
{
  LDK_COMPONENT_INSTANCE_FLAG_NONE = 0,
  LDK_COMPONENT_INSTANCE_FLAG_HIDE_IN_EDITOR = 1 << 0,
  LDK_COMPONENT_INSTANCE_FLAG_DISABLED = 1 << 1
} LDKComponentInstanceFlags;

/** @group modules.entity.components */
typedef enum LDKBuiltinComponentType
{
  LDK_COMPONENT_TYPE_TRANSFORM    = UINT32_MAX - 1,
  LDK_COMPONENT_TYPE_CAMERA       = UINT32_MAX - 2,
  LDK_COMPONENT_TYPE_MESH_SOURCE  = UINT32_MAX - 3,
  LDK_COMPONENT_TYPE_POINT_LIGHT = UINT32_MAX - 4,
  LDK_COMPONENT_TYPE_SPOT_LIGHT = UINT32_MAX - 5,
  LDK_COMPONENT_TYPE_DIRECTIONAL_LIGHT = UINT32_MAX - 6,
  LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE = UINT32_MAX - 7,
  LDK_COMPONENT_TYPE_PARTICLE_EMITTER = UINT32_MAX - 8,
  LDK_COMPONENT_TYPE_POST_PROCESSING = UINT32_MAX - 9,
  LDK_COMPONENT_TYPE_TEXT3D = UINT32_MAX - 10,
  LDK_COMPONENT_TYPE_AUDIO_SOURCE = UINT32_MAX - 11,
  LDK_COMPONENT_TYPE_GRASS_INTERACT = UINT32_MAX - 12,
  /* UINT32_MAX - 13 was the retired CharacterController. Do not reuse it. */
  LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER = UINT32_MAX - 14,
  LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE = UINT32_MAX - 15,
  LDK_COMPONENT_TYPE_MATERIAL_INSTANCE = UINT32_MAX - 16,
} LDKBuiltinComponentType;

/**
 * A list of all components and component types in an entity.
 * @group modules.entity.components
 */
typedef struct LDKComponentDirectory
{
  u32 component_type[LDK_ENTITY_MAX_COMPONENTS];
  u32 component_index[LDK_ENTITY_MAX_COMPONENTS];
  u16 component_flags[LDK_ENTITY_MAX_COMPONENTS];
  u16 component_count;
  u16 version;
} LDKComponentDirectory;

/**
 * Stable reference to a component through its owning entity.
 * @group modules.entity.components.references
 */
typedef struct LDKComponentRef
{
  LDKEntity entity;
  u16 version;
  u16 slot_index;
} LDKComponentRef;

/** @group modules.entity.lifecycle */
typedef struct LDKEntityInfo
{
  LDKComponentDirectory components;
  u32 transform_index; // Transform is a special component. An entity always have a transform.
  u16 internal_flags;
  u16 flags;
  u64 name_hash; /* Stable hash of the authoring name, zero means unnamed. */
#if defined(_DEBUG) || defined(LDK_EDITOR)
  u8 name[LDK_ENTITY_NAME_MAX_LEN];
#endif
} LDKEntityInfo;

/**
 * @brief Callback visiting an entity during iteration.
 * @param entity Entity to inspect or modify.
 * @param info Info used by this operation.
 * @param user Opaque caller-supplied data passed to the callback.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.iteration
 */
typedef bool (*LDKEntityIterFn)(LDKEntity entity, LDKEntityInfo* info, void* user);

/** @group modules.entity.registry */
typedef struct LDKEntityRegistry
{
  XHPool pool;
  /* Engine notification; flags remain per instance, outside component data. */
  void (*component_state_changed)(void *user, LDKEntity entity);
  void *component_state_user;
} LDKEntityRegistry;

/** @group modules.entity.lifecycle */
typedef struct LDKTransform LDKTransform;

/**
 * @brief Initialize the entity module.
 * @param system System used by this operation.
 * @param page_capacity Number of entries per allocation page.
 * @param initial_pages Number of pages initially allocated.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.registry
 */
LDK_API bool ldk_entity_module_initialize(LDKEntityRegistry* system, u32 page_capacity, u32 initial_pages);
/**
 * @brief Shut down the entity module.
 * @param system System used by this operation.
 * @group modules.entity.registry
 */
LDK_API void ldk_entity_module_terminate(LDKEntityRegistry* system);
/**
 * @brief Clear the state of the entity module.
 * @param system System used by this operation.
 * @group modules.entity.registry
 */
LDK_API void ldk_entity_module_clear(LDKEntityRegistry* system);
/**
 * @brief Create the entity.
 * @param system System used by this operation.
 * @return The requested value.
 * @group modules.entity.lifecycle
 */
LDK_API LDKEntity ldk_entity_create(LDKEntityRegistry* system);
/**
 * @brief Destroy the entity.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @group modules.entity.lifecycle
 */
LDK_API void ldk_entity_destroy(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Check whether the entity refers to a live resource.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.lifecycle
 */
LDK_API bool ldk_entity_is_alive(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Get the entity info.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.lifecycle
 */
LDK_API LDKEntityInfo* ldk_entity_info_get(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Get read-only information about an entity.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.lifecycle
 */
LDK_API const LDKEntityInfo* ldk_entity_get_info_const(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Count the entity alive.
 * @param system System used by this operation.
 * @return Number of matching entries.
 * @group modules.entity.lifecycle
 */
LDK_API u32 ldk_entity_alive_count(LDKEntityRegistry* system);
/**
 * @brief Set the entity flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_flags_set(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Get the entity flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return The requested value.
 * @group modules.entity.flags
 */
LDK_API u16 ldk_entity_flags_get(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Add the entity flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_flags_add(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Remove the entity flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_flags_remove(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Check whether an entity has the requested flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.flags
 */
LDK_API bool ldk_entity_flags_has(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

/**
 * @brief Get the entity internal flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return The requested value.
 * @group modules.entity.flags
 */
LDK_API u16 ldk_entity_internal_flags_get(LDKEntityRegistry* system, LDKEntity entity);
/**
 * @brief Check whether an entity has the requested internal flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.flags
 */
LDK_API bool ldk_entity_internal_flags_has(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Set the entity name.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param name Name identifying the object or setting.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.names
 */
LDK_API bool ldk_entity_name_set(LDKEntityRegistry* system, LDKEntity entity, const char* name);
/**
 * @brief Get the entity name.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.names
 */
LDK_API const char* ldk_entity_name_get(LDKEntityRegistry* system, LDKEntity entity);
/* Name hashes are FNV-1a 64-bit. Names need not exist in release builds. */
/**
 * @brief Compute an entity name hash.
 * @param name Name identifying the object or setting.
 * @return Computed identifier or hash.
 * @group modules.entity.names
 */
LDK_API u64 ldk_entity_name_hash(const char *name);
/**
 * @brief Set the entity name hash.
 * @param registry Registry on which the operation is performed.
 * @param entity Entity to inspect or modify.
 * @param hash Hash used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.names
 */
LDK_API bool ldk_entity_name_hash_set(
    LDKEntityRegistry *registry, LDKEntity entity, u64 hash);
/**
 * @brief Get the entity name hash.
 * @param registry Registry on which the operation is performed.
 * @param entity Entity to inspect or modify.
 * @return Computed identifier or hash.
 * @group modules.entity.names
 */
LDK_API u64 ldk_entity_name_hash_get(
    LDKEntityRegistry *registry, LDKEntity entity);
/**
 * @brief Count the entity component.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Number of matching entries.
 * @group modules.entity.components
 */
LDK_API u32 ldk_entity_component_count(LDKEntityRegistry* system, LDKEntity entity);

/**
 * @brief Get the entity transform.
 * @param entity_module Entity module used by this operation.
 * @param component_module Component module used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.lifecycle
 */
LDK_API LDKTransform* ldk_entity_transform_get(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity);

/**
 * @brief Get read-only access to the entity transform.
 * @param entity_module Entity module used by this operation.
 * @param component_module Component module used by this operation.
 * @param entity Entity to inspect or modify.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.lifecycle
 */
LDK_API const LDKTransform* ldk_entity_transform_get_const(LDKEntityRegistry* entity_module,
    LDKComponentRegistry* component_module, LDKEntity entity);

/**
 * @brief Check whether an entity owns a specific component.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_has(LDKEntityRegistry* system, LDKEntity entity, u32 component_type);
/**
 * @brief Get the entity component flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return The requested value.
 * @group modules.entity.components
 */
LDK_API u16 ldk_entity_component_flags_get(LDKEntityRegistry* system,
    LDKEntity entity, u32 component_type);
/**
 * @brief Set the entity component flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param flags Bitmask of options controlling this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_flags_set(LDKEntityRegistry* system,
    LDKEntity entity, u32 component_type, u16 flags);
/**
 * @brief Add the entity component flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param flags Bitmask of options controlling this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_flags_add(LDKEntityRegistry* system,
    LDKEntity entity, u32 component_type, u16 flags);
/**
 * @brief Remove the entity component flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param flags Bitmask of options controlling this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_flags_remove(LDKEntityRegistry* system,
    LDKEntity entity, u32 component_type, u16 flags);
/**
 * @brief Check whether a component instance has requested flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param flags Bitmask of options controlling this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_flags_has(LDKEntityRegistry* system,
    LDKEntity entity, u32 component_type, u16 flags);
/**
 * @brief Check whether the component ref is valid.
 * @param system System used by this operation.
 * @param ref Ref used by this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group modules.entity.components.references
 */
LDK_API bool ldk_component_ref_is_valid(LDKEntityRegistry* system, LDKComponentRef ref);
/**
 * @brief Visit each living entity using a callback.
 * @param system System used by this operation.
 * @param fn Callback invoked by this operation.
 * @param user Opaque caller-supplied data passed to the callback.
 * @group modules.entity.iteration
 */
LDK_API void ldk_entity_foreach(LDKEntityRegistry* system, LDKEntityIterFn fn, void* user);
/**
 * @brief Get the entity component ref.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param out_ref Receives the resulting ref.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components.references
 */
LDK_API bool ldk_entity_component_ref_get(LDKEntityRegistry* system, LDKEntity entity, u32 component_type, LDKComponentRef* out_ref);
/**
 * @brief Find the entity component.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param out_slot Receives the resulting slot.
 * @param out_component_index Receives the resulting component index.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_find(LDKEntityRegistry* system, LDKEntity entity, u32 component_type, u32* out_slot, u32* out_component_index);
/**
 * @brief Resolve a component reference to its data.
 * @param entity_system Entity system used by this operation.
 * @param component_registry Component registry used by this operation.
 * @param ref Ref used by this operation.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.components.references
 */
LDK_API void* ldk_component_ref_get(LDKEntityRegistry* entity_system, struct LDKComponentRegistry* component_registry, LDKComponentRef ref);
/**
 * @brief Resolve a component reference as read-only data.
 * @param entity_system Entity system used by this operation.
 * @param component_registry Component registry used by this operation.
 * @param ref Ref used by this operation.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.components.references
 */
LDK_API const void* ldk_component_ref_get_const(LDKEntityRegistry* entity_system, struct LDKComponentRegistry* component_registry, LDKComponentRef ref);
/**
 * @brief Add the entity component.
 * @param entity_module Entity module used by this operation.
 * @param component_module Component module used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @param initial_value Initial value used by this operation.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.components
 */
LDK_API void* ldk_entity_component_add(LDKEntityRegistry* entity_module, LDKComponentRegistry* component_module, LDKEntity entity, u32 component_type, const void* initial_value);
/**
 * @brief Get the entity component.
 * @param entity_module Entity module used by this operation.
 * @param component_module Component module used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.entity.components
 */
LDK_API void* ldk_entity_component_get(LDKEntityRegistry* entity_module, LDKComponentRegistry* component_module, LDKEntity entity, u32 component_type);
/**
 * @brief Remove the entity component.
 * @param entity_module Entity module used by this operation.
 * @param component_module Component module used by this operation.
 * @param entity Entity to inspect or modify.
 * @param component_type Registered ECS component type identifier.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.components
 */
LDK_API bool ldk_entity_component_remove(LDKEntityRegistry* entity_module, LDKComponentRegistry* component_module, LDKEntity entity, u32 component_type);

/** @group modules.entity.iteration */
typedef struct LDKEntityIterator
{
  LDKEntityRegistry* registry;
  XHPoolIter iter;
  LDKEntity current;
  u8 has_current;
} LDKEntityIterator;

/**
 * @brief Begin the entity iterator.
 * @param registry Registry on which the operation is performed.
 * @return The requested value.
 * @group modules.entity.iteration
 */
LDK_API LDKEntityIterator ldk_entity_iterator_begin(LDKEntityRegistry* registry);
/**
 * @brief Advance an entity iterator to the next entity.
 * @param iterator Iterator used by this operation.
 * @param out_entity Receives the resulting entity.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.entity.iteration
 */
LDK_API bool ldk_entity_iterator_next(LDKEntityIterator* iterator, LDKEntity* out_entity);
/**
 * @brief Finish the entity iterator.
 * @param iterator Iterator used by this operation.
 * @group modules.entity.iteration
 */
LDK_API void ldk_entity_iterator_end(LDKEntityIterator* iterator);

#if defined(LDK_ENGINE) || defined(LDK_EDITOR)
/**
 * @brief Set the entity internal flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_internal_flags_set(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Add the entity internal flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_internal_flags_add(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
/**
 * @brief Remove the entity internal flags.
 * @param system System used by this operation.
 * @param entity Entity to inspect or modify.
 * @param flags Bitmask of options controlling this operation.
 * @group modules.entity.flags
 */
LDK_API void ldk_entity_internal_flags_remove(LDKEntityRegistry* system, LDKEntity entity, u16 flags);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_ENTITY_H
