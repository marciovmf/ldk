/**
 * # Material instance component
 *
 * Per-entity material parameters; rendering resources stay shared.
 */

#ifndef LDK_MATERIAL_INSTANCE_H
#define LDK_MATERIAL_INSTANCE_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_component.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.component.material_instance */
//@component
typedef struct LDKMaterialInstance
{
  /* Resolved from the MeshSource albedo material; never authored here. */
  //@inspect readonly runtime
  LDKAssetTextureAtlas atlas;
  /* 0 uses the material's default region. IDs are persistent across renames. */
  //@inspect widget=ATLAS_REGION source=atlas label=Sprite
  u64 sprite_id;
  //@inspect hidden runtime
  LDKAssetTextureAtlas cached_atlas;
  //@inspect hidden runtime
  u64 cached_revision;
  //@inspect hidden runtime
  u64 cached_sprite_id;
  //@inspect hidden runtime
  u64 cached_default_sprite_id;
  //@inspect hidden runtime
  u32 cached_image_width;
  //@inspect hidden runtime
  u32 cached_image_height;
  //@inspect hidden runtime
  Vec4 cached_uv_rect;
  //@inspect hidden runtime
  bool cached_drawable;
  //@inspect hidden runtime
  bool cache_valid;
} LDKMaterialInstance;

#ifdef LDK_ENGINE
/**
 * @brief Build the ECS registration descriptor for the material instance component.
 * @param initial_capacity Initial number of entries reserved in the registry.
 * @return A descriptor for the requested component or resource.
 * @group ldk.component.material_instance
 */
LDK_API LDKComponentDesc ldk_material_instance_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif
