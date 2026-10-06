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
  TFTF_ISLAND_TILE_MOUNTAIN
} TFTFIslandTileKind;

bool tftf_island_terrain_tile_kind_at(
    Vec3 world_position, TFTFIslandTileKind *out_kind);

/**
 * Read-only view of the generated RGBA16 island map.
 * Channels: R=height, G=decoration, B=resource, A=reserved.
 */
typedef struct IslandMapDataView
{
  const u16 *texels;
  u32 width;
  u32 height;
} IslandMapDataView;

bool island_terrain_map_data_get(IslandMapDataView *out_view);

//@system initialize=island_terrain_system_initialize update=island_terrain_system_update terminate=island_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
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

  //@begin_group "Biome Thresholds"
  float deep_water_max;
  float shallow_water_max;
  float shore_max;
  float mountain_min;
  float dry_moisture_max;
  float grass_moisture_max;
  //@end_group

  //@begin_group "Decorations"
  float forest_decoration_chance;
  float grass_decoration_0_chance;
  float grass_decoration_1_chance;
  float mountain_decoration_chance;
  float dry_decoration_chance;
  float shore_decoration_chance;
  float shallow_water_decoration_chance;
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
