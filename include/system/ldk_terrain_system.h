/**
 * # Terrain system
 *
 * Generic configurable height-field terrain system.
 */

#ifndef LDK_TERRAIN_SYSTEM_H
#define LDK_TERRAIN_SYSTEM_H

#include <ldk_asset.h>
#include <ldk_color.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group systems.terrain.surfaces */
#define LDK_TERRAIN_SURFACE_COUNT 8u

/** @group systems.terrain.lifecycle */
//@enum
typedef enum LDKTerrainChannel
{
  LDK_TERRAIN_CHANNEL_R = 0,
  LDK_TERRAIN_CHANNEL_G,
  LDK_TERRAIN_CHANNEL_B,
  LDK_TERRAIN_CHANNEL_A
} LDKTerrainChannel;

/**
 * @brief Assign a heightmap image to the terrain system.
 * @param image Image used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group systems.terrain.heightmaps
 */
LDK_API bool ldk_terrain_heightmap_set(LDKAssetImage image);
/**
 * @brief Check whether the terrain system is active.
 * @return True when the queried condition holds; false otherwise.
 * @group systems.terrain.lifecycle
 */
LDK_API bool ldk_terrain_system_is_active(void);
/**
 * @brief Query terrain height at a world-space X/Z point.
 * @param world_x World-space X coordinate.
 * @param world_z World-space Z coordinate.
 * @param out_height Receives the surface height in world-space units.
 * @return True if the operation succeeds; false otherwise.
 * @group systems.terrain.lifecycle
 */
LDK_API bool ldk_terrain_height_at_world(
    float world_x, float world_z, float *out_height);

/**
 * @brief Initialize the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @return System initialization status code.
 * @group systems.terrain.lifecycle
 */
int ldk_terrain_system_initialize(void *data);
/**
 * @brief Update the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group systems.terrain.lifecycle
 */
void ldk_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
/**
 * @brief Shut down the terrain system.
 * @param data Source data or system-owned context for this operation.
 * @group systems.terrain.lifecycle
 */
void ldk_terrain_system_terminate(void *data);

/** @group systems.terrain.lifecycle */
//@system name=LDKTerrainSystem initialize=ldk_terrain_system_initialize update=ldk_terrain_system_update terminate=ldk_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
typedef struct LDKTerrainSystem
{
  //@begin_group "Height Source"
  LDKAssetImage heightmap;
  LDKTerrainChannel height_channel;
  //@inspect min=0.0001
  float cell_size;
  float height_scale;
  float height_offset;
  bool center_on_origin;
  float origin_x;
  float origin_z;
  //@end_group

  //@begin_group "Geometry"
  //@inspect min=1 max=256
  u32 chunk_size;
  //@end_group

  //@begin_group "Appearance"
  LDKAssetMaterial material;
  LDKAssetTextureAtlas texture_atlas;
  //@inspect min=1 max=8
  u32 surface_count;
  //@end_group

  //@begin_group "Surface 0"
  XSmallstr surface_0_name;
  //@inspect slider min=0 max=1
  float surface_0_min_height;
  //@inspect slider min=0 max=1
  float surface_0_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_0_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_0_sprite_id;
  Vec2 surface_0_uv_scale;
  //@inspect slider min=0 max=1
  float surface_0_texture_weight;
  //@end_group
  //@begin_group "Surface 1"
  XSmallstr surface_1_name;
  //@inspect slider min=0 max=1
  float surface_1_min_height;
  //@inspect slider min=0 max=1
  float surface_1_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_1_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_1_sprite_id;
  Vec2 surface_1_uv_scale;
  //@inspect slider min=0 max=1
  float surface_1_texture_weight;
  //@end_group
  //@begin_group "Surface 2"
  XSmallstr surface_2_name;
  //@inspect slider min=0 max=1
  float surface_2_min_height;
  //@inspect slider min=0 max=1
  float surface_2_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_2_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_2_sprite_id;
  Vec2 surface_2_uv_scale;
  //@inspect slider min=0 max=1
  float surface_2_texture_weight;
  //@end_group
  //@begin_group "Surface 3"
  XSmallstr surface_3_name;
  //@inspect slider min=0 max=1
  float surface_3_min_height;
  //@inspect slider min=0 max=1
  float surface_3_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_3_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_3_sprite_id;
  Vec2 surface_3_uv_scale;
  //@inspect slider min=0 max=1
  float surface_3_texture_weight;
  //@end_group
  //@begin_group "Surface 4"
  XSmallstr surface_4_name;
  //@inspect slider min=0 max=1
  float surface_4_min_height;
  //@inspect slider min=0 max=1
  float surface_4_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_4_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_4_sprite_id;
  Vec2 surface_4_uv_scale;
  //@inspect slider min=0 max=1
  float surface_4_texture_weight;
  //@end_group
  //@begin_group "Surface 5"
  XSmallstr surface_5_name;
  //@inspect slider min=0 max=1
  float surface_5_min_height;
  //@inspect slider min=0 max=1
  float surface_5_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_5_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_5_sprite_id;
  Vec2 surface_5_uv_scale;
  //@inspect slider min=0 max=1
  float surface_5_texture_weight;
  //@end_group
  //@begin_group "Surface 6"
  XSmallstr surface_6_name;
  //@inspect slider min=0 max=1
  float surface_6_min_height;
  //@inspect slider min=0 max=1
  float surface_6_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_6_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_6_sprite_id;
  Vec2 surface_6_uv_scale;
  //@inspect slider min=0 max=1
  float surface_6_texture_weight;
  //@end_group
  //@begin_group "Surface 7"
  XSmallstr surface_7_name;
  //@inspect slider min=0 max=1
  float surface_7_min_height;
  //@inspect slider min=0 max=1
  float surface_7_blend_width;
  //@inspect widget=COLOR
  rgba32 surface_7_color;
  /* Stable atlas region ID (0 means no sprite). */
  //@inspect widget=ATLAS_REGION source=texture_atlas label=Sprite
  u64 surface_7_sprite_id;
  Vec2 surface_7_uv_scale;
  //@inspect slider min=0 max=1
  float surface_7_texture_weight;
  //@end_group

  //@begin_group "Rendering"
  bool casts_shadows;
  //@end_group

  //@begin_group "Runtime"
  //@inspect readonly runtime
  u32 data_width;
  //@inspect readonly runtime
  u32 data_height;
  //@inspect readonly runtime
  u32 chunk_count;
  //@inspect readonly runtime
  u32 visible_chunk_count;
  //@inspect readonly runtime
  u32 mesh_update_count;
  //@inspect readonly runtime
  bool has_geometry;
  //@end_group
} LDKTerrainSystem;

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_SYSTEM_H
