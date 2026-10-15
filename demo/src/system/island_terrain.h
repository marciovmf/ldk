#ifndef DEMO_ISLAND_TERRAIN_H
#define DEMO_ISLAND_TERRAIN_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

int island_terrain_system_initialize(void *data);
void island_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void island_terrain_system_terminate(void *data);

bool island_terrain_grass_state_at(Vec3 world_position,
    float *out_density, float *out_min_height);
bool island_terrain_grass_density_multiply_at(
    Vec3 world_position, float multiplier);

/** Game-specific, non-rendering interpretation of the generated island tile. */
typedef enum TFTFIslandTileKind
{
  TFTF_ISLAND_TILE_DEEP_WATER = 0,
  TFTF_ISLAND_TILE_SHALLOW_WATER,
  TFTF_ISLAND_TILE_SHORE,
  TFTF_ISLAND_TILE_DRY,
  TFTF_ISLAND_TILE_GRASS,
  TFTF_ISLAND_TILE_FOREST,
  TFTF_ISLAND_TILE_THORNS,
  TFTF_ISLAND_TILE_MOUNTAIN,
  TFTF_ISLAND_TILE_COUNT
} TFTFIslandTileKind;

bool island_terrain_world_to_cell(
    Vec3 world_position, i32 *out_x, i32 *out_y);
bool island_terrain_cell_world_bounds(
    i32 cell_x, i32 cell_y, Vec2 *out_min, Vec2 *out_max);
bool tftf_island_terrain_tile_kind_at_cell(
    i32 cell_x, i32 cell_y, TFTFIslandTileKind *out_kind);
bool tftf_island_terrain_tile_kind_at(
    Vec3 world_position, TFTFIslandTileKind *out_kind);

typedef enum TFTFIslandMapChannel
{
  TFTF_ISLAND_MAP_CHANNEL_HEIGHT = 0,
  TFTF_ISLAND_MAP_CHANNEL_DECORATION,
  TFTF_ISLAND_MAP_CHANNEL_RESOURCE,
  TFTF_ISLAND_MAP_CHANNEL_RESERVED,
  TFTF_ISLAND_MAP_CHANNEL_COUNT
} TFTFIslandMapChannel;

u8 island_terrain_map_decoration_mask_get(u16 packed);
float island_terrain_map_density_get(u16 packed);

/**
 * Read-only view of the generated RGBA16 island map.
 * Channels:
 * R = height.
 * G = decoration mask in the high byte, shared density in the low byte.
 * B = resource type in the high byte, density in the low byte.
 * A = reserved.
 */
typedef struct IslandMapDataView
{
  const u16 *texels;
  u32 width;
  u32 height;
  u32 revision;
  u32 seed;
} IslandMapDataView;

bool island_terrain_map_data_get(IslandMapDataView *out_view);

//@system components=LDK_COMPONENT_TYPE_CAMERA initialize=island_terrain_system_initialize update=island_terrain_system_update terminate=island_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
typedef struct IslandTerrain
{
  //@begin_group "Island Generation"
  u32 seed;
  u32 map_size;
  float land_scale;
  float island_radius;
  float terrain_scale;
  /** How much the existing terrain noise shapes land elevation inside each biome. */
  //@inspect slider min=0 max=1
  float terrain_height_strength;

  /*
   * Small-scale height variation applied inside every land biome so broad
   * regions never become perfectly planar.
   */
  float surface_noise_scale;
  //@inspect min=1 max=8
  u32 surface_noise_octaves;
  //@inspect slider min=0 max=0.5
  float surface_noise_strength;

  float moisture_scale;
  u32 land_octaves;
  u32 terrain_octaves;
  u32 moisture_octaves;
  float land_noise_strength;
  /** Smooth only the generated land height channel across biome boundaries. */
  //@inspect slider min=0 max=1
  float height_smoothing_strength;
  /** Width, in heightmap cells, of the softened band around sharp height transitions. */
  //@inspect min=0 max=24
  u32 height_smoothing_radius;
  /**
   * Minimum normalized neighbor-height difference considered a hard edge.
   * This catches biome borders and coast/shore steps without blurring normal
   * surface noise inside a biome.
   */
  //@inspect slider min=0 max=0.1
  float height_smoothing_edge_threshold;
  //@inspect min=0 max=8
  u32 height_smoothing_passes;
  //@end_group

  //@begin_group "Height Bands"
  /** Normalized heights, multiplied by TerrainSystem.height_scale. */
  //@inspect slider min=0 max=1
  float deep_water_height;
  //@inspect slider min=0 max=1
  float shallow_water_height;
  //@inspect slider min=0 max=1
  float dry_height;
  //@inspect slider min=0 max=1
  float grass_height;
  //@inspect slider min=0 max=1
  float forest_height;
  //@inspect slider min=0 max=1
  float thorn_height;
  //@inspect slider min=0 max=1
  float mountain_height;
  //@inspect slider min=0 max=1
  float max_height;
  /** Fraction of the coastal shallow band used by the deep-to-shallow slope. */
  //@inspect slider min=0.01 max=1
  float shallow_slope_fraction;
  //@end_group

  //@begin_group "Biome Thresholds"
  float deep_water_max;
  float shallow_water_max;
  float shore_max;
  float mountain_min;
  float dry_moisture_max;
  float grass_moisture_max;
  /** Independent noise selects patches of thorn vegetation below mountains. */
  float thorn_noise_scale;
  //@inspect slider min=0 max=1
  float thorn_noise_min;
  /** Additional thorn noise weight in cells otherwise assigned to tall grass. */
  //@inspect slider min=0 max=1
  float thorn_grass_bias;
  //@end_group

  //@begin_group "Inland Shallow Water"
  /** Candidate spacing in heightmap cells. Zero disables inland pools. */
  //@inspect min=0
  float shallow_patch_spacing;
  //@inspect slider min=0 max=1
  float shallow_patch_chance;
  //@inspect min=1
  float shallow_patch_min_radius;
  //@inspect min=1
  float shallow_patch_max_radius;
  //@inspect min=1
  float shallow_patch_bank_width;
  //@end_group

  //@begin_group "Decorations"
  //@begin_group "Deep Water"
  //@inspect slider min=0 max=1
  float deep_water_decoration_0_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_1_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_2_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_3_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_4_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_5_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_6_chance;
  //@inspect slider min=0 max=1
  float deep_water_decoration_7_chance;
  //@end_group

  //@begin_group "Shallow Water"
  //@inspect slider min=0 max=1
  float shallow_water_decoration_0_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_1_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_2_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_3_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_4_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_5_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_6_chance;
  //@inspect slider min=0 max=1
  float shallow_water_decoration_7_chance;
  //@end_group

  //@begin_group "Shore"
  //@inspect slider min=0 max=1
  float shore_decoration_0_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_1_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_2_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_3_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_4_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_5_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_6_chance;
  //@inspect slider min=0 max=1
  float shore_decoration_7_chance;
  //@end_group

  //@begin_group "Dry"
  //@inspect slider min=0 max=1
  float dry_decoration_0_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_1_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_2_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_3_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_4_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_5_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_6_chance;
  //@inspect slider min=0 max=1
  float dry_decoration_7_chance;
  //@end_group

  //@begin_group "Grass"
  //@inspect slider min=0 max=1
  float grass_decoration_0_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_1_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_2_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_3_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_4_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_5_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_6_chance;
  //@inspect slider min=0 max=1
  float grass_decoration_7_chance;
  //@end_group

  //@begin_group "Forest"
  //@inspect slider min=0 max=1
  float forest_decoration_0_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_1_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_2_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_3_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_4_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_5_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_6_chance;
  //@inspect slider min=0 max=1
  float forest_decoration_7_chance;
  //@end_group

  //@begin_group "Thorns"
  //@inspect slider min=0 max=1
  float thorns_decoration_0_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_1_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_2_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_3_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_4_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_5_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_6_chance;
  //@inspect slider min=0 max=1
  float thorns_decoration_7_chance;
  //@end_group

  //@begin_group "Mountain"
  //@inspect slider min=0 max=1
  float mountain_decoration_0_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_1_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_2_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_3_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_4_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_5_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_6_chance;
  //@inspect slider min=0 max=1
  float mountain_decoration_7_chance;
  //@end_group

  //@end_group

  //@begin_group "Resources"
  float mountain_resource_chance;
  float forest_resource_chance;
  float grass_resource_chance;
  float dry_resource_chance;
  float shore_resource_chance;
  float shallow_water_resource_chance;
  //@end_group

  //@begin_group "Grass"
  XSmallstr dry_grass_type_name;
  XSmallstr grass_type_name;
  XSmallstr forest_grass_type_name;
  XSmallstr thorn_grass_type_name;
  XSmallstr shallow_grass_type_name;
  //@end_group

  //@begin_group "Island Rendering"
  u32 radius;
  float cell_size;
  float elevation;
  float forward_offset;
  LDKAssetMaterial material;
  //@end_group

  //@begin_group "Runtime"
  //@inspect runtime
  i32 center_x;
  //@inspect runtime
  i32 center_y;
  //@inspect runtime
  u32 cached_radius;
  //@inspect runtime
  float cached_cell_size;
  //@inspect runtime
  float cached_elevation;
  //@inspect runtime
  u32 map_revision;
  //@inspect runtime
  u32 active_tile_count;
  //@inspect runtime
  u32 tiles_added;
  //@inspect runtime
  u32 tiles_removed;
  //@inspect runtime
  u32 mesh_update_count;
  //@inspect runtime
  bool mesh_dirty;
  //@inspect runtime
  bool has_geometry;
  //@inspect runtime
  bool map_loaded_from_cache;
  //@inspect runtime
  LDKResourceMesh mesh;
  //@end_group
} IslandTerrain;

#endif // DEMO_ISLAND_TERRAIN_H
