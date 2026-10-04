/**
 * @file ldk_terrain_system.h
 * @brief Generic configurable height-field terrain system.
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

#define LDK_TERRAIN_SURFACE_COUNT 8u

//@enum
typedef enum LDKTerrainChannel
{
  LDK_TERRAIN_CHANNEL_R = 0,
  LDK_TERRAIN_CHANNEL_G,
  LDK_TERRAIN_CHANNEL_B,
  LDK_TERRAIN_CHANNEL_A
} LDKTerrainChannel;

LDK_API bool ldk_terrain_heightmap_set(LDKAssetImage image);
LDK_API bool ldk_terrain_system_is_active(void);
LDK_API bool ldk_terrain_height_at_world(
    float world_x, float world_z, float *out_height);

int ldk_terrain_system_initialize(void *data);
void ldk_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void ldk_terrain_system_terminate(void *data);

//@system name=TerrainSystem initialize=ldk_terrain_system_initialize update=ldk_terrain_system_update terminate=ldk_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
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
  Vec4 surface_0_atlas_rect;
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
  Vec4 surface_1_atlas_rect;
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
  Vec4 surface_2_atlas_rect;
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
  Vec4 surface_3_atlas_rect;
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
  Vec4 surface_4_atlas_rect;
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
  Vec4 surface_5_atlas_rect;
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
  Vec4 surface_6_atlas_rect;
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
  Vec4 surface_7_atlas_rect;
  Vec2 surface_7_uv_scale;
  //@inspect slider min=0 max=1
  float surface_7_texture_weight;
  //@end_group

  //@begin_group "Rendering"
  bool casts_shadows;
  //@end_group

  //@begin_group "Runtime"
  //@inspect runtime
  u32 data_width;
  //@inspect runtime
  u32 data_height;
  //@inspect runtime
  u32 chunk_count;
  //@inspect runtime
  u32 visible_chunk_count;
  //@inspect runtime
  u32 mesh_update_count;
  //@inspect runtime
  bool has_geometry;
  //@end_group
} LDKTerrainSystem;

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_SYSTEM_H
