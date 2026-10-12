/**
 * @file ldk_material_instance.h
 * @brief Per-entity material parameters; rendering resources stay shared.
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
  Vec4 cached_uv_rect;
  //@inspect hidden runtime
  bool cache_valid;
} LDKMaterialInstance;

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_material_instance_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif
