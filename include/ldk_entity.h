/**
 * @file   ldk_entity.h
 * @brief  Entity system (handles, lifecycle, component binding)
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
#define LDK_ENTITY_MAX_COMPONENTS 16
#endif

#ifndef LDK_ENTITY_NAME_MAX_LEN   
#define LDK_ENTITY_NAME_MAX_LEN   32
#endif

  typedef XHandle LDKEntity;

  typedef enum LDKEntityInternalFlags
  {
    LDK_ENTITY_INTERNAL_NONE           = 0,
    LDK_ENTITY_INTERNAL_PENDING_DELETE = 1 << 0,
    LDK_ENTITY_INTERNAL_HAS_TRANSFORM  = 1 << 1,
    LDK_ENTITY_INTERNAL_HAS_CAMERA     = 1 << 2,
    LDK_ENTITY_INTERNAL_HAS_RENDERABLE = 1 << 3,
    LDK_ENTITY_INTERNAL_HAS_LIGHT      = 1 << 4
  } LDKEntityInternalFlags;

  /**
   * A list of all components and component types in an entity
   */
  typedef struct LDKComponentDirectory
  {
    u32 component_type[LDK_ENTITY_MAX_COMPONENTS];
    u32 component_index[LDK_ENTITY_MAX_COMPONENTS];
    u8 component_count;
  } LDKComponentDirectory;

  /**
   * Metadata information of an Entity.
   * This is the actual entity data 
   */
  typedef struct LDKEntityInfo
  {
    LDKComponentDirectory components;
    u16 internal_flags;
    u16 flags;
#ifdef _DEBUG
    u8 name[LDK_ENTITY_NAME_MAX_LEN];
#endif
  } LDKEntityInfo;

  typedef bool (*LDKEntityIterFn)(LDKEntity entity, LDKEntityInfo* info, void* user);

  typedef struct LDKEntityRegistry
  {
    XHPool pool;
  } LDKEntityRegistry;

  LDK_API bool ldk_entity_module_initialize(LDKEntityRegistry* system, u32 page_capacity, u32 initial_pages);

  LDK_API void ldk_entity_module_terminate(LDKEntityRegistry* system);

  LDK_API void ldk_entity_module_clear(LDKEntityRegistry* system);

  LDK_API LDKEntity ldk_entity_create(LDKEntityRegistry* system);

  LDK_API void ldk_entity_destroy(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API bool ldk_entity_is_alive(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API LDKEntityInfo* ldk_entity_get_info(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API const LDKEntityInfo* ldk_entity_get_info_const(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API u32 ldk_entity_alive_count(LDKEntityRegistry* system);

  LDK_API void ldk_entity_set_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API u16 ldk_entity_get_flags(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API void ldk_entity_add_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API void ldk_entity_remove_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API bool ldk_entity_has_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API void ldk_entity_set_internal_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API u16 ldk_entity_get_internal_flags(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API void ldk_entity_add_internal_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API void ldk_entity_remove_internal_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API bool ldk_entity_has_internal_flags(LDKEntityRegistry* system, LDKEntity entity, u16 flags);

  LDK_API bool ldk_entity_set_name(LDKEntityRegistry* system, LDKEntity entity, const char* name);

  LDK_API const char* ldk_entity_get_name(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API u32 ldk_entity_component_count(LDKEntityRegistry* system, LDKEntity entity);

  LDK_API bool ldk_entity_find_component(
      LDKEntityRegistry* system,
      LDKEntity entity,
      u32 component_type,
      u32* out_slot,
      u32* out_component_index);

  LDK_API bool ldk_entity_has_component(
      LDKEntityRegistry* system,
      LDKEntity entity,
      u32 component_type);

  LDK_API bool ldk_entity_add_component_ref(
      LDKEntityRegistry* system,
      LDKEntity entity,
      u32 component_type,
      u32 component_index);

  LDK_API bool ldk_entity_update_component_ref(
      LDKEntityRegistry* system,
      LDKEntity entity,
      u32 component_type,
      u32 component_index);

  LDK_API bool ldk_entity_remove_component_ref(
      LDKEntityRegistry* system,
      LDKEntity entity,
      u32 component_type);

  LDK_API void ldk_entity_foreach(
      LDKEntityRegistry* system,
      LDKEntityIterFn fn,
      void* user);

#ifdef __cplusplus
}
#endif

#endif // LDK_ENTITY_H
