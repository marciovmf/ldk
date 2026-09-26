#ifndef DEMO_ISLAND_TERRAIN_H
#define DEMO_ISLAND_TERRAIN_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>

int island_terrain_system_initialize(void *data);
void island_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void island_terrain_system_terminate(void *data);

bool island_terrain_grass_state_at(Vec3 world_position,
    float *out_density, float *out_min_height);
bool island_terrain_grass_density_multiply_at(
    Vec3 world_position, float multiplier);

//@enum
typedef enum IslandGrassBladePreset
{
  ISLAND_GRASS_BLADE_PRESET_THIN = 0,
  ISLAND_GRASS_BLADE_PRESET_CROSSED,
  ISLAND_GRASS_BLADE_PRESET_TUFT,
  ISLAND_GRASS_BLADE_PRESET_FLAT_TOP,
  ISLAND_GRASS_BLADE_PRESET_THORNY
} IslandGrassBladePreset;

//@system initialize=island_terrain_system_initialize update=island_terrain_system_update terminate=island_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
typedef struct IslandTerrain
{
  //@begin_group "Island Generation"
  u32 seed;
  u32 map_size;
  float land_scale;
  float island_radius;
  float terrain_scale;
  float moisture_scale;
  u32 land_octaves;
  u32 terrain_octaves;
  u32 moisture_octaves;
  float land_noise_strength;
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
  //@begin_group "Distribution"
  float dry_grass_density;
  float dry_grass_min_height;
  float dry_grass_max_height;
  float dry_grass_blade_width;
  float grass_density;
  float grass_min_height;
  float grass_max_height;
  float grass_blade_width;
  float forest_grass_density;
  float forest_grass_min_height;
  float forest_grass_max_height;
  float forest_grass_blade_width;
  //@end_group

  //@begin_group "Blade"
  IslandGrassBladePreset dry_grass_blade_preset;
  //@inspect slider min=0 max=0.5
  float dry_grass_blade_curvature;
  IslandGrassBladePreset grass_blade_preset;
  //@inspect slider min=0 max=0.5
  float grass_blade_curvature;
  IslandGrassBladePreset forest_grass_blade_preset;
  //@inspect slider min=0 max=0.5
  float forest_grass_blade_curvature;
  //@inspect slider min=0 max=89
  float grass_min_tilt_degrees;
  //@inspect slider min=0 max=89
  float grass_max_tilt_degrees;
  /** Shared lean azimuth: 0 degrees is +X, 90 degrees is +Z. */
  //@inspect slider min=0 max=360
  float grass_tilt_direction_degrees;
  //@end_group

  //@begin_group "Animation"
  //@inspect slider min=0 max=0.5
  float dry_grass_wind_strength;
  //@inspect slider min=0 max=0.5
  float grass_wind_strength;
  //@inspect slider min=0 max=0.5
  float forest_grass_wind_strength;
  /** Global wind frequency shared by every biome. */
  //@inspect slider min=0 max=5
  float grass_wind_speed;
  //@end_group

  //@begin_group "Material"
  /** Phong highlight strength and exponent for every grass biome. */
  //@inspect slider min=0 max=1
  float grass_specular;
  //@inspect slider min=1 max=256
  float grass_shininess;
  //@inspect slider min=0 max=4
  float grass_emission;
  //@inspect widget=COLOR
  u32 dry_grass_bottom_color;
  //@inspect widget=COLOR
  u32 dry_grass_top_color;
  //@inspect widget=COLOR
  u32 grass_bottom_color;
  //@inspect widget=COLOR
  u32 grass_top_color;
  //@inspect widget=COLOR
  u32 forest_grass_bottom_color;
  //@inspect widget=COLOR
  u32 forest_grass_top_color;
  bool grass_casts_shadows;
  //@end_group
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
