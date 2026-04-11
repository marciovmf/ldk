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

  typedef struct LDKEntitySystem
  {
    XHPool pool;
  } LDKEntitySystem;

  bool ldk_entity_system_initialize(LDKEntitySystem* system, u32 page_capacity, u32 initial_pages);
  void ldk_entity_system_terminate(LDKEntitySystem* system);
  void ldk_entity_system_clear(LDKEntitySystem* system);

  LDKEntity ldk_entity_create(LDKEntitySystem* system);
  void ldk_entity_destroy(LDKEntitySystem* system, LDKEntity entity);

  bool ldk_entity_is_alive(LDKEntitySystem* system, LDKEntity entity);

  LDKEntityInfo* ldk_entity_get_info(LDKEntitySystem* system, LDKEntity entity);
  const LDKEntityInfo* ldk_entity_get_info_const(LDKEntitySystem* system, LDKEntity entity);

  u32 ldk_entity_alive_count(LDKEntitySystem* system);

  void ldk_entity_set_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  u16 ldk_entity_get_flags(LDKEntitySystem* system, LDKEntity entity);
  void ldk_entity_add_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  void ldk_entity_remove_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  bool ldk_entity_has_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);

  void ldk_entity_set_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  u16 ldk_entity_get_internal_flags(LDKEntitySystem* system, LDKEntity entity);
  void ldk_entity_add_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  void ldk_entity_remove_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);
  bool ldk_entity_has_internal_flags(LDKEntitySystem* system, LDKEntity entity, u16 flags);

  bool ldk_entity_set_name(LDKEntitySystem* system, LDKEntity entity, const char* name);
  const char* ldk_entity_get_name(LDKEntitySystem* system, LDKEntity entity);

  u32 ldk_entity_component_count(LDKEntitySystem* system, LDKEntity entity);

  bool ldk_entity_find_component(
      LDKEntitySystem* system,
      LDKEntity entity,
      u32 component_type,
      u32* out_slot,
      u32* out_component_index);

  bool ldk_entity_has_component(
      LDKEntitySystem* system,
      LDKEntity entity,
      u32 component_type);

  bool ldk_entity_add_component_ref(
      LDKEntitySystem* system,
      LDKEntity entity,
      u32 component_type,
      u32 component_index);

  bool ldk_entity_update_component_ref(
      LDKEntitySystem* system,
      LDKEntity entity,
      u32 component_type,
      u32 component_index);

  bool ldk_entity_remove_component_ref(
      LDKEntitySystem* system,
      LDKEntity entity,
      u32 component_type);

  void ldk_entity_foreach(
      LDKEntitySystem* system,
      LDKEntityIterFn fn,
      void* user);

#ifdef __cplusplus
}
#endif

#endif // LDK_ENTITY_H
