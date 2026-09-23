#ifndef DEMO_ISLAND_TERRAIN_H
#define DEMO_ISLAND_TERRAIN_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>

int island_terrain_system_initialize(void *data);
void island_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void island_terrain_system_terminate(void *data);

//@system initialize=island_terrain_system_initialize update=island_terrain_system_update terminate=island_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
typedef struct IslandTerrain
{
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
  float deep_water_max;
  float shallow_water_max;
  float shore_max;
  float mountain_min;
  float dry_moisture_max;
  float grass_moisture_max;
  float forest_decoration_chance;
  float grass_decoration_0_chance;
  float grass_decoration_1_chance;
  float mountain_decoration_chance;
  float dry_decoration_chance;
  float shore_decoration_chance;
  float shallow_water_decoration_chance;
  float mountain_resource_chance;
  float forest_resource_chance;
  float grass_resource_chance;
  float dry_resource_chance;
  float shore_resource_chance;
  float shallow_water_resource_chance;

  u32 radius;
  float cell_size;
  float elevation;
  float forward_offset;
  LDKAssetMaterial material;

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
} IslandTerrain;

#endif // DEMO_ISLAND_TERRAIN_H
