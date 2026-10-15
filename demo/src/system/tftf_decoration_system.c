#include "tftf_decoration_system.h"

#include "island_terrain.h"

#include <component/ldk_instanced_mesh_source.h>
#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <system/ldk_terrain_system.h>
#include <stdx/stdx_math.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TFTF_DECORATION_SLOT_COUNT 8u

typedef struct TFTFDecorationSlotConfig
{
  LDKAssetMesh mesh;
  LDKAssetMaterial material;
  u32 min_count;
  u32 max_count;
  float min_scale;
  float max_scale;
} TFTFDecorationSlotConfig;

typedef struct TFTFDecorationBatch
{
  LDKAssetMesh mesh;
  LDKAssetMaterial material;
  LDKEntity entity;
  Mat4 *instances;
  u32 instance_count;
  u32 instance_capacity;
} TFTFDecorationBatch;

typedef struct TFTFDecorationRuntime
{
  TFTFDecorationSystem *owner;
  TFTFDecorationBatch *batches;
  u32 batch_count;
  u32 batch_capacity;
  u64 config_hash;
  bool built;
} TFTFDecorationRuntime;

static TFTFDecorationRuntime s_runtime;

static u32 s_hash_u32(u32 value)
{
  value ^= value >> 16;
  value *= 0x7feb352du;
  value ^= value >> 15;
  value *= 0x846ca68bu;
  value ^= value >> 16;
  return value;
}

static u32 s_hash_cell(i32 cell_x, i32 cell_y, u32 seed)
{
  u32 hash = seed;

  hash ^= s_hash_u32((u32)cell_x + 0x9e3779b9u);
  hash ^= s_hash_u32((u32)cell_y + 0x85ebca6bu);
  return s_hash_u32(hash);
}

static float s_hash_rand01(i32 cell_x, i32 cell_y, u32 seed)
{
  return (float)((double)s_hash_cell(cell_x, cell_y, seed) /
      4294967296.0);
}

static u32 s_instance_seed(u32 map_seed, TFTFIslandTileKind kind,
    u32 slot, u32 instance, u32 salt)
{
  u32 identity = ((u32)kind << 24u) ^ (slot << 16u) ^ instance;

  return map_seed ^ s_hash_u32(identity ^ salt);
}

static bool s_slot_config_get(const TFTFDecorationSystem *system,
    TFTFIslandTileKind kind, u32 slot, TFTFDecorationSlotConfig *out_config)
{
  if (!system || !out_config || slot >= TFTF_DECORATION_SLOT_COUNT)
  {
    return false;
  }

  if (kind == TFTF_ISLAND_TILE_DEEP_WATER)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->deep_water_decoration_0_mesh;
      out_config->material = system->deep_water_decoration_0_material;
      out_config->min_count = system->deep_water_decoration_0_min_count;
      out_config->max_count = system->deep_water_decoration_0_max_count;
      out_config->min_scale = system->deep_water_decoration_0_min_scale;
      out_config->max_scale = system->deep_water_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->deep_water_decoration_1_mesh;
      out_config->material = system->deep_water_decoration_1_material;
      out_config->min_count = system->deep_water_decoration_1_min_count;
      out_config->max_count = system->deep_water_decoration_1_max_count;
      out_config->min_scale = system->deep_water_decoration_1_min_scale;
      out_config->max_scale = system->deep_water_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->deep_water_decoration_2_mesh;
      out_config->material = system->deep_water_decoration_2_material;
      out_config->min_count = system->deep_water_decoration_2_min_count;
      out_config->max_count = system->deep_water_decoration_2_max_count;
      out_config->min_scale = system->deep_water_decoration_2_min_scale;
      out_config->max_scale = system->deep_water_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->deep_water_decoration_3_mesh;
      out_config->material = system->deep_water_decoration_3_material;
      out_config->min_count = system->deep_water_decoration_3_min_count;
      out_config->max_count = system->deep_water_decoration_3_max_count;
      out_config->min_scale = system->deep_water_decoration_3_min_scale;
      out_config->max_scale = system->deep_water_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->deep_water_decoration_4_mesh;
      out_config->material = system->deep_water_decoration_4_material;
      out_config->min_count = system->deep_water_decoration_4_min_count;
      out_config->max_count = system->deep_water_decoration_4_max_count;
      out_config->min_scale = system->deep_water_decoration_4_min_scale;
      out_config->max_scale = system->deep_water_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->deep_water_decoration_5_mesh;
      out_config->material = system->deep_water_decoration_5_material;
      out_config->min_count = system->deep_water_decoration_5_min_count;
      out_config->max_count = system->deep_water_decoration_5_max_count;
      out_config->min_scale = system->deep_water_decoration_5_min_scale;
      out_config->max_scale = system->deep_water_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->deep_water_decoration_6_mesh;
      out_config->material = system->deep_water_decoration_6_material;
      out_config->min_count = system->deep_water_decoration_6_min_count;
      out_config->max_count = system->deep_water_decoration_6_max_count;
      out_config->min_scale = system->deep_water_decoration_6_min_scale;
      out_config->max_scale = system->deep_water_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->deep_water_decoration_7_mesh;
      out_config->material = system->deep_water_decoration_7_material;
      out_config->min_count = system->deep_water_decoration_7_min_count;
      out_config->max_count = system->deep_water_decoration_7_max_count;
      out_config->min_scale = system->deep_water_decoration_7_min_scale;
      out_config->max_scale = system->deep_water_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_SHALLOW_WATER)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->shallow_water_decoration_0_mesh;
      out_config->material = system->shallow_water_decoration_0_material;
      out_config->min_count = system->shallow_water_decoration_0_min_count;
      out_config->max_count = system->shallow_water_decoration_0_max_count;
      out_config->min_scale = system->shallow_water_decoration_0_min_scale;
      out_config->max_scale = system->shallow_water_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->shallow_water_decoration_1_mesh;
      out_config->material = system->shallow_water_decoration_1_material;
      out_config->min_count = system->shallow_water_decoration_1_min_count;
      out_config->max_count = system->shallow_water_decoration_1_max_count;
      out_config->min_scale = system->shallow_water_decoration_1_min_scale;
      out_config->max_scale = system->shallow_water_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->shallow_water_decoration_2_mesh;
      out_config->material = system->shallow_water_decoration_2_material;
      out_config->min_count = system->shallow_water_decoration_2_min_count;
      out_config->max_count = system->shallow_water_decoration_2_max_count;
      out_config->min_scale = system->shallow_water_decoration_2_min_scale;
      out_config->max_scale = system->shallow_water_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->shallow_water_decoration_3_mesh;
      out_config->material = system->shallow_water_decoration_3_material;
      out_config->min_count = system->shallow_water_decoration_3_min_count;
      out_config->max_count = system->shallow_water_decoration_3_max_count;
      out_config->min_scale = system->shallow_water_decoration_3_min_scale;
      out_config->max_scale = system->shallow_water_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->shallow_water_decoration_4_mesh;
      out_config->material = system->shallow_water_decoration_4_material;
      out_config->min_count = system->shallow_water_decoration_4_min_count;
      out_config->max_count = system->shallow_water_decoration_4_max_count;
      out_config->min_scale = system->shallow_water_decoration_4_min_scale;
      out_config->max_scale = system->shallow_water_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->shallow_water_decoration_5_mesh;
      out_config->material = system->shallow_water_decoration_5_material;
      out_config->min_count = system->shallow_water_decoration_5_min_count;
      out_config->max_count = system->shallow_water_decoration_5_max_count;
      out_config->min_scale = system->shallow_water_decoration_5_min_scale;
      out_config->max_scale = system->shallow_water_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->shallow_water_decoration_6_mesh;
      out_config->material = system->shallow_water_decoration_6_material;
      out_config->min_count = system->shallow_water_decoration_6_min_count;
      out_config->max_count = system->shallow_water_decoration_6_max_count;
      out_config->min_scale = system->shallow_water_decoration_6_min_scale;
      out_config->max_scale = system->shallow_water_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->shallow_water_decoration_7_mesh;
      out_config->material = system->shallow_water_decoration_7_material;
      out_config->min_count = system->shallow_water_decoration_7_min_count;
      out_config->max_count = system->shallow_water_decoration_7_max_count;
      out_config->min_scale = system->shallow_water_decoration_7_min_scale;
      out_config->max_scale = system->shallow_water_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_SHORE)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->shore_decoration_0_mesh;
      out_config->material = system->shore_decoration_0_material;
      out_config->min_count = system->shore_decoration_0_min_count;
      out_config->max_count = system->shore_decoration_0_max_count;
      out_config->min_scale = system->shore_decoration_0_min_scale;
      out_config->max_scale = system->shore_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->shore_decoration_1_mesh;
      out_config->material = system->shore_decoration_1_material;
      out_config->min_count = system->shore_decoration_1_min_count;
      out_config->max_count = system->shore_decoration_1_max_count;
      out_config->min_scale = system->shore_decoration_1_min_scale;
      out_config->max_scale = system->shore_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->shore_decoration_2_mesh;
      out_config->material = system->shore_decoration_2_material;
      out_config->min_count = system->shore_decoration_2_min_count;
      out_config->max_count = system->shore_decoration_2_max_count;
      out_config->min_scale = system->shore_decoration_2_min_scale;
      out_config->max_scale = system->shore_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->shore_decoration_3_mesh;
      out_config->material = system->shore_decoration_3_material;
      out_config->min_count = system->shore_decoration_3_min_count;
      out_config->max_count = system->shore_decoration_3_max_count;
      out_config->min_scale = system->shore_decoration_3_min_scale;
      out_config->max_scale = system->shore_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->shore_decoration_4_mesh;
      out_config->material = system->shore_decoration_4_material;
      out_config->min_count = system->shore_decoration_4_min_count;
      out_config->max_count = system->shore_decoration_4_max_count;
      out_config->min_scale = system->shore_decoration_4_min_scale;
      out_config->max_scale = system->shore_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->shore_decoration_5_mesh;
      out_config->material = system->shore_decoration_5_material;
      out_config->min_count = system->shore_decoration_5_min_count;
      out_config->max_count = system->shore_decoration_5_max_count;
      out_config->min_scale = system->shore_decoration_5_min_scale;
      out_config->max_scale = system->shore_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->shore_decoration_6_mesh;
      out_config->material = system->shore_decoration_6_material;
      out_config->min_count = system->shore_decoration_6_min_count;
      out_config->max_count = system->shore_decoration_6_max_count;
      out_config->min_scale = system->shore_decoration_6_min_scale;
      out_config->max_scale = system->shore_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->shore_decoration_7_mesh;
      out_config->material = system->shore_decoration_7_material;
      out_config->min_count = system->shore_decoration_7_min_count;
      out_config->max_count = system->shore_decoration_7_max_count;
      out_config->min_scale = system->shore_decoration_7_min_scale;
      out_config->max_scale = system->shore_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_DRY)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->dry_decoration_0_mesh;
      out_config->material = system->dry_decoration_0_material;
      out_config->min_count = system->dry_decoration_0_min_count;
      out_config->max_count = system->dry_decoration_0_max_count;
      out_config->min_scale = system->dry_decoration_0_min_scale;
      out_config->max_scale = system->dry_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->dry_decoration_1_mesh;
      out_config->material = system->dry_decoration_1_material;
      out_config->min_count = system->dry_decoration_1_min_count;
      out_config->max_count = system->dry_decoration_1_max_count;
      out_config->min_scale = system->dry_decoration_1_min_scale;
      out_config->max_scale = system->dry_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->dry_decoration_2_mesh;
      out_config->material = system->dry_decoration_2_material;
      out_config->min_count = system->dry_decoration_2_min_count;
      out_config->max_count = system->dry_decoration_2_max_count;
      out_config->min_scale = system->dry_decoration_2_min_scale;
      out_config->max_scale = system->dry_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->dry_decoration_3_mesh;
      out_config->material = system->dry_decoration_3_material;
      out_config->min_count = system->dry_decoration_3_min_count;
      out_config->max_count = system->dry_decoration_3_max_count;
      out_config->min_scale = system->dry_decoration_3_min_scale;
      out_config->max_scale = system->dry_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->dry_decoration_4_mesh;
      out_config->material = system->dry_decoration_4_material;
      out_config->min_count = system->dry_decoration_4_min_count;
      out_config->max_count = system->dry_decoration_4_max_count;
      out_config->min_scale = system->dry_decoration_4_min_scale;
      out_config->max_scale = system->dry_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->dry_decoration_5_mesh;
      out_config->material = system->dry_decoration_5_material;
      out_config->min_count = system->dry_decoration_5_min_count;
      out_config->max_count = system->dry_decoration_5_max_count;
      out_config->min_scale = system->dry_decoration_5_min_scale;
      out_config->max_scale = system->dry_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->dry_decoration_6_mesh;
      out_config->material = system->dry_decoration_6_material;
      out_config->min_count = system->dry_decoration_6_min_count;
      out_config->max_count = system->dry_decoration_6_max_count;
      out_config->min_scale = system->dry_decoration_6_min_scale;
      out_config->max_scale = system->dry_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->dry_decoration_7_mesh;
      out_config->material = system->dry_decoration_7_material;
      out_config->min_count = system->dry_decoration_7_min_count;
      out_config->max_count = system->dry_decoration_7_max_count;
      out_config->min_scale = system->dry_decoration_7_min_scale;
      out_config->max_scale = system->dry_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_GRASS)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->grass_decoration_0_mesh;
      out_config->material = system->grass_decoration_0_material;
      out_config->min_count = system->grass_decoration_0_min_count;
      out_config->max_count = system->grass_decoration_0_max_count;
      out_config->min_scale = system->grass_decoration_0_min_scale;
      out_config->max_scale = system->grass_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->grass_decoration_1_mesh;
      out_config->material = system->grass_decoration_1_material;
      out_config->min_count = system->grass_decoration_1_min_count;
      out_config->max_count = system->grass_decoration_1_max_count;
      out_config->min_scale = system->grass_decoration_1_min_scale;
      out_config->max_scale = system->grass_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->grass_decoration_2_mesh;
      out_config->material = system->grass_decoration_2_material;
      out_config->min_count = system->grass_decoration_2_min_count;
      out_config->max_count = system->grass_decoration_2_max_count;
      out_config->min_scale = system->grass_decoration_2_min_scale;
      out_config->max_scale = system->grass_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->grass_decoration_3_mesh;
      out_config->material = system->grass_decoration_3_material;
      out_config->min_count = system->grass_decoration_3_min_count;
      out_config->max_count = system->grass_decoration_3_max_count;
      out_config->min_scale = system->grass_decoration_3_min_scale;
      out_config->max_scale = system->grass_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->grass_decoration_4_mesh;
      out_config->material = system->grass_decoration_4_material;
      out_config->min_count = system->grass_decoration_4_min_count;
      out_config->max_count = system->grass_decoration_4_max_count;
      out_config->min_scale = system->grass_decoration_4_min_scale;
      out_config->max_scale = system->grass_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->grass_decoration_5_mesh;
      out_config->material = system->grass_decoration_5_material;
      out_config->min_count = system->grass_decoration_5_min_count;
      out_config->max_count = system->grass_decoration_5_max_count;
      out_config->min_scale = system->grass_decoration_5_min_scale;
      out_config->max_scale = system->grass_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->grass_decoration_6_mesh;
      out_config->material = system->grass_decoration_6_material;
      out_config->min_count = system->grass_decoration_6_min_count;
      out_config->max_count = system->grass_decoration_6_max_count;
      out_config->min_scale = system->grass_decoration_6_min_scale;
      out_config->max_scale = system->grass_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->grass_decoration_7_mesh;
      out_config->material = system->grass_decoration_7_material;
      out_config->min_count = system->grass_decoration_7_min_count;
      out_config->max_count = system->grass_decoration_7_max_count;
      out_config->min_scale = system->grass_decoration_7_min_scale;
      out_config->max_scale = system->grass_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_FOREST)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->forest_decoration_0_mesh;
      out_config->material = system->forest_decoration_0_material;
      out_config->min_count = system->forest_decoration_0_min_count;
      out_config->max_count = system->forest_decoration_0_max_count;
      out_config->min_scale = system->forest_decoration_0_min_scale;
      out_config->max_scale = system->forest_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->forest_decoration_1_mesh;
      out_config->material = system->forest_decoration_1_material;
      out_config->min_count = system->forest_decoration_1_min_count;
      out_config->max_count = system->forest_decoration_1_max_count;
      out_config->min_scale = system->forest_decoration_1_min_scale;
      out_config->max_scale = system->forest_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->forest_decoration_2_mesh;
      out_config->material = system->forest_decoration_2_material;
      out_config->min_count = system->forest_decoration_2_min_count;
      out_config->max_count = system->forest_decoration_2_max_count;
      out_config->min_scale = system->forest_decoration_2_min_scale;
      out_config->max_scale = system->forest_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->forest_decoration_3_mesh;
      out_config->material = system->forest_decoration_3_material;
      out_config->min_count = system->forest_decoration_3_min_count;
      out_config->max_count = system->forest_decoration_3_max_count;
      out_config->min_scale = system->forest_decoration_3_min_scale;
      out_config->max_scale = system->forest_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->forest_decoration_4_mesh;
      out_config->material = system->forest_decoration_4_material;
      out_config->min_count = system->forest_decoration_4_min_count;
      out_config->max_count = system->forest_decoration_4_max_count;
      out_config->min_scale = system->forest_decoration_4_min_scale;
      out_config->max_scale = system->forest_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->forest_decoration_5_mesh;
      out_config->material = system->forest_decoration_5_material;
      out_config->min_count = system->forest_decoration_5_min_count;
      out_config->max_count = system->forest_decoration_5_max_count;
      out_config->min_scale = system->forest_decoration_5_min_scale;
      out_config->max_scale = system->forest_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->forest_decoration_6_mesh;
      out_config->material = system->forest_decoration_6_material;
      out_config->min_count = system->forest_decoration_6_min_count;
      out_config->max_count = system->forest_decoration_6_max_count;
      out_config->min_scale = system->forest_decoration_6_min_scale;
      out_config->max_scale = system->forest_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->forest_decoration_7_mesh;
      out_config->material = system->forest_decoration_7_material;
      out_config->min_count = system->forest_decoration_7_min_count;
      out_config->max_count = system->forest_decoration_7_max_count;
      out_config->min_scale = system->forest_decoration_7_min_scale;
      out_config->max_scale = system->forest_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_THORNS)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->thorns_decoration_0_mesh;
      out_config->material = system->thorns_decoration_0_material;
      out_config->min_count = system->thorns_decoration_0_min_count;
      out_config->max_count = system->thorns_decoration_0_max_count;
      out_config->min_scale = system->thorns_decoration_0_min_scale;
      out_config->max_scale = system->thorns_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->thorns_decoration_1_mesh;
      out_config->material = system->thorns_decoration_1_material;
      out_config->min_count = system->thorns_decoration_1_min_count;
      out_config->max_count = system->thorns_decoration_1_max_count;
      out_config->min_scale = system->thorns_decoration_1_min_scale;
      out_config->max_scale = system->thorns_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->thorns_decoration_2_mesh;
      out_config->material = system->thorns_decoration_2_material;
      out_config->min_count = system->thorns_decoration_2_min_count;
      out_config->max_count = system->thorns_decoration_2_max_count;
      out_config->min_scale = system->thorns_decoration_2_min_scale;
      out_config->max_scale = system->thorns_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->thorns_decoration_3_mesh;
      out_config->material = system->thorns_decoration_3_material;
      out_config->min_count = system->thorns_decoration_3_min_count;
      out_config->max_count = system->thorns_decoration_3_max_count;
      out_config->min_scale = system->thorns_decoration_3_min_scale;
      out_config->max_scale = system->thorns_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->thorns_decoration_4_mesh;
      out_config->material = system->thorns_decoration_4_material;
      out_config->min_count = system->thorns_decoration_4_min_count;
      out_config->max_count = system->thorns_decoration_4_max_count;
      out_config->min_scale = system->thorns_decoration_4_min_scale;
      out_config->max_scale = system->thorns_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->thorns_decoration_5_mesh;
      out_config->material = system->thorns_decoration_5_material;
      out_config->min_count = system->thorns_decoration_5_min_count;
      out_config->max_count = system->thorns_decoration_5_max_count;
      out_config->min_scale = system->thorns_decoration_5_min_scale;
      out_config->max_scale = system->thorns_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->thorns_decoration_6_mesh;
      out_config->material = system->thorns_decoration_6_material;
      out_config->min_count = system->thorns_decoration_6_min_count;
      out_config->max_count = system->thorns_decoration_6_max_count;
      out_config->min_scale = system->thorns_decoration_6_min_scale;
      out_config->max_scale = system->thorns_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->thorns_decoration_7_mesh;
      out_config->material = system->thorns_decoration_7_material;
      out_config->min_count = system->thorns_decoration_7_min_count;
      out_config->max_count = system->thorns_decoration_7_max_count;
      out_config->min_scale = system->thorns_decoration_7_min_scale;
      out_config->max_scale = system->thorns_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  if (kind == TFTF_ISLAND_TILE_MOUNTAIN)
  {
    switch (slot)
    {
    case 0u:
      out_config->mesh = system->mountain_decoration_0_mesh;
      out_config->material = system->mountain_decoration_0_material;
      out_config->min_count = system->mountain_decoration_0_min_count;
      out_config->max_count = system->mountain_decoration_0_max_count;
      out_config->min_scale = system->mountain_decoration_0_min_scale;
      out_config->max_scale = system->mountain_decoration_0_max_scale;
      return true;
    case 1u:
      out_config->mesh = system->mountain_decoration_1_mesh;
      out_config->material = system->mountain_decoration_1_material;
      out_config->min_count = system->mountain_decoration_1_min_count;
      out_config->max_count = system->mountain_decoration_1_max_count;
      out_config->min_scale = system->mountain_decoration_1_min_scale;
      out_config->max_scale = system->mountain_decoration_1_max_scale;
      return true;
    case 2u:
      out_config->mesh = system->mountain_decoration_2_mesh;
      out_config->material = system->mountain_decoration_2_material;
      out_config->min_count = system->mountain_decoration_2_min_count;
      out_config->max_count = system->mountain_decoration_2_max_count;
      out_config->min_scale = system->mountain_decoration_2_min_scale;
      out_config->max_scale = system->mountain_decoration_2_max_scale;
      return true;
    case 3u:
      out_config->mesh = system->mountain_decoration_3_mesh;
      out_config->material = system->mountain_decoration_3_material;
      out_config->min_count = system->mountain_decoration_3_min_count;
      out_config->max_count = system->mountain_decoration_3_max_count;
      out_config->min_scale = system->mountain_decoration_3_min_scale;
      out_config->max_scale = system->mountain_decoration_3_max_scale;
      return true;
    case 4u:
      out_config->mesh = system->mountain_decoration_4_mesh;
      out_config->material = system->mountain_decoration_4_material;
      out_config->min_count = system->mountain_decoration_4_min_count;
      out_config->max_count = system->mountain_decoration_4_max_count;
      out_config->min_scale = system->mountain_decoration_4_min_scale;
      out_config->max_scale = system->mountain_decoration_4_max_scale;
      return true;
    case 5u:
      out_config->mesh = system->mountain_decoration_5_mesh;
      out_config->material = system->mountain_decoration_5_material;
      out_config->min_count = system->mountain_decoration_5_min_count;
      out_config->max_count = system->mountain_decoration_5_max_count;
      out_config->min_scale = system->mountain_decoration_5_min_scale;
      out_config->max_scale = system->mountain_decoration_5_max_scale;
      return true;
    case 6u:
      out_config->mesh = system->mountain_decoration_6_mesh;
      out_config->material = system->mountain_decoration_6_material;
      out_config->min_count = system->mountain_decoration_6_min_count;
      out_config->max_count = system->mountain_decoration_6_max_count;
      out_config->min_scale = system->mountain_decoration_6_min_scale;
      out_config->max_scale = system->mountain_decoration_6_max_scale;
      return true;
    case 7u:
      out_config->mesh = system->mountain_decoration_7_mesh;
      out_config->material = system->mountain_decoration_7_material;
      out_config->min_count = system->mountain_decoration_7_min_count;
      out_config->max_count = system->mountain_decoration_7_max_count;
      out_config->min_scale = system->mountain_decoration_7_min_scale;
      out_config->max_scale = system->mountain_decoration_7_max_scale;
      return true;
    default:
      return false;
    }
  }

  return false;
}

static void s_slot_config_normalize(TFTFDecorationSlotConfig *config)
{
  float min_scale;
  float max_scale;

  if (!config)
  {
    return;
  }

  min_scale = config->min_scale;
  max_scale = config->max_scale;
  if (!isfinite(min_scale) || min_scale <= 0.0f)
  {
    min_scale = 1.0f;
  }
  if (!isfinite(max_scale) || max_scale <= 0.0f)
  {
    max_scale = min_scale;
  }
  if (max_scale < min_scale)
  {
    float swap = min_scale;
    min_scale = max_scale;
    max_scale = swap;
  }

  config->min_scale = min_scale;
  config->max_scale = max_scale;
}

static void s_hash_bytes(u64 *hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;

  for (size_t i = 0u; i < size; ++i)
  {
    *hash ^= (u64)bytes[i];
    *hash *= UINT64_C(1099511628211);
  }
}

static u64 s_config_hash(const TFTFDecorationSystem *system)
{
  u64 hash = UINT64_C(14695981039346656037);

  if (!system)
  {
    return 0u;
  }

  s_hash_bytes(&hash, &system->radius, sizeof(system->radius));
  for (u32 kind = 0u; kind < (u32)TFTF_ISLAND_TILE_COUNT; ++kind)
  {
    for (u32 slot = 0u; slot < TFTF_DECORATION_SLOT_COUNT; ++slot)
    {
      TFTFDecorationSlotConfig config;
      if (!s_slot_config_get(
              system, (TFTFIslandTileKind)kind, slot, &config))
      {
        continue;
      }
      s_slot_config_normalize(&config);

      s_hash_bytes(&hash, &config.mesh.h.index, sizeof(config.mesh.h.index));
      s_hash_bytes(
          &hash, &config.mesh.h.version, sizeof(config.mesh.h.version));
      s_hash_bytes(
          &hash, &config.material.h.index, sizeof(config.material.h.index));
      s_hash_bytes(&hash, &config.material.h.version,
          sizeof(config.material.h.version));
      s_hash_bytes(&hash, &config.min_count, sizeof(config.min_count));
      s_hash_bytes(&hash, &config.max_count, sizeof(config.max_count));
      s_hash_bytes(&hash, &config.min_scale, sizeof(config.min_scale));
      s_hash_bytes(&hash, &config.max_scale, sizeof(config.max_scale));
    }
  }

  return hash;
}

static bool s_mesh_equal(LDKAssetMesh a, LDKAssetMesh b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static bool s_material_equal(LDKAssetMaterial a, LDKAssetMaterial b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static bool s_batches_reserve(u32 required)
{
  TFTFDecorationBatch *batches;
  u32 capacity;

  if (required <= s_runtime.batch_capacity)
  {
    return true;
  }

  capacity = s_runtime.batch_capacity ? s_runtime.batch_capacity : 8u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*batches))
  {
    return false;
  }

  batches = (TFTFDecorationBatch *)realloc(
      s_runtime.batches, (size_t)capacity * sizeof(*batches));
  if (!batches)
  {
    return false;
  }

  s_runtime.batches = batches;
  s_runtime.batch_capacity = capacity;
  return true;
}

static TFTFDecorationBatch *s_batch_get(
    LDKAssetMesh mesh, LDKAssetMaterial material)
{
  for (u32 i = 0u; i < s_runtime.batch_count; ++i)
  {
    if (s_mesh_equal(s_runtime.batches[i].mesh, mesh) &&
        s_material_equal(s_runtime.batches[i].material, material))
    {
      return &s_runtime.batches[i];
    }
  }

  if (!s_batches_reserve(s_runtime.batch_count + 1u))
  {
    return NULL;
  }

  TFTFDecorationBatch *batch =
      &s_runtime.batches[s_runtime.batch_count++];
  memset(batch, 0, sizeof(*batch));
  batch->mesh = mesh;
  batch->material = material;
  batch->entity = x_handle_null();
  return batch;
}

static bool s_batch_instances_reserve(
    TFTFDecorationBatch *batch, u32 required)
{
  Mat4 *instances;
  u32 capacity;

  if (!batch)
  {
    return false;
  }
  if (required <= batch->instance_capacity)
  {
    return true;
  }

  capacity = batch->instance_capacity ? batch->instance_capacity : 32u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*instances))
  {
    return false;
  }

  instances = (Mat4 *)realloc(
      batch->instances, (size_t)capacity * sizeof(*instances));
  if (!instances)
  {
    return false;
  }

  batch->instances = instances;
  batch->instance_capacity = capacity;
  return true;
}

static bool s_batch_append(TFTFDecorationBatch *batch, Mat4 transform)
{
  if (!batch || batch->instance_count == UINT32_MAX ||
      !s_batch_instances_reserve(batch, batch->instance_count + 1u))
  {
    return false;
  }

  batch->instances[batch->instance_count++] = transform;
  return true;
}

static void s_batch_counts_reset(void)
{
  for (u32 i = 0u; i < s_runtime.batch_count; ++i)
  {
    s_runtime.batches[i].instance_count = 0u;
  }
}

static u32 s_instance_count_for_cell(i32 cell_x, i32 cell_y,
    u32 map_seed, TFTFIslandTileKind kind, u32 slot, float density,
    u32 min_count, u32 max_count)
{
  double expected;
  double whole;
  double fraction;
  u32 count;

  if (max_count < min_count)
  {
    max_count = min_count;
  }
  if (max_count == min_count)
  {
    return min_count;
  }

  density = float_clamp(density, 0.0f, 1.0f);
  expected = (double)min_count +
      (double)(max_count - min_count) * (double)density;
  fraction = modf(expected, &whole);
  count = (u32)whole;

  if (fraction > 0.0 && count < max_count)
  {
    u32 seed = s_instance_seed(
        map_seed, kind, slot, 0u, 0x6bc27969u);
    if ((double)s_hash_rand01(cell_x, cell_y, seed) < fraction)
    {
      ++count;
    }
  }

  return count;
}

static bool s_batch_component_get(TFTFDecorationBatch *batch,
    LDKAssetManager *assets, LDKInstancedMeshSource **out_source)
{
  LDKInstancedMeshSource *source;

  if (!batch || !assets || !out_source)
  {
    return false;
  }

  if (x_handle_is_null(batch->entity))
  {
    batch->entity = ldk_ecs_entity_create();
    if (x_handle_is_null(batch->entity))
    {
      return false;
    }

    source = (LDKInstancedMeshSource *)ldk_ecs_component_add(
        batch->entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE, NULL);
    if (!source || !ldk_mesh_source_set_data(&source->source, batch->mesh) ||
        (!x_handle_is_null(batch->material.h) &&
            !ldk_mesh_source_set_material_asset(
                &source->source, assets, batch->material)))
    {
      ldk_ecs_entity_destroy(batch->entity);
      batch->entity = x_handle_null();
      return false;
    }
  }
  else
  {
    source = (LDKInstancedMeshSource *)ldk_ecs_component_get(
        batch->entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
    if (!source)
    {
      return false;
    }
  }

  *out_source = source;
  return true;
}

static bool s_batches_commit(
    TFTFDecorationSystem *system, LDKAssetManager *assets)
{
  u32 active_batches = 0u;
  u32 total_instances = 0u;

  for (u32 i = 0u; i < s_runtime.batch_count; ++i)
  {
    TFTFDecorationBatch *batch = &s_runtime.batches[i];
    LDKInstancedMeshSource *source;

    if (batch->instance_count == 0u && x_handle_is_null(batch->entity))
    {
      continue;
    }
    if (!s_batch_component_get(batch, assets, &source) ||
        !ldk_instanced_mesh_source_set_instances(
            source, batch->instances, batch->instance_count))
    {
      return false;
    }

    if (batch->instance_count > 0u)
    {
      ++active_batches;
      if (UINT32_MAX - total_instances < batch->instance_count)
      {
        total_instances = UINT32_MAX;
      }
      else
      {
        total_instances += batch->instance_count;
      }
    }
  }

  system->batch_count = active_batches;
  system->instance_count = total_instances;
  return true;
}

static void s_rendered_instances_clear(TFTFDecorationSystem *system)
{
  if (!system)
  {
    return;
  }

  for (u32 i = 0u; i < s_runtime.batch_count; ++i)
  {
    TFTFDecorationBatch *batch = &s_runtime.batches[i];
    LDKInstancedMeshSource *source;

    batch->instance_count = 0u;
    if (x_handle_is_null(batch->entity))
    {
      continue;
    }

    source = (LDKInstancedMeshSource *)ldk_ecs_component_get(
        batch->entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
    if (source)
    {
      (void)ldk_instanced_mesh_source_resize_instances(source, 0u);
    }
  }

  system->active_cell_count = 0u;
  system->batch_count = 0u;
  system->instance_count = 0u;
  system->center_x = INT32_MIN;
  system->center_y = INT32_MIN;
  s_runtime.built = false;
}

static bool s_player_world_position(LDKEntity entity, Vec3 *out_position)
{
  Mat4 world;

  if (!out_position || !ldk_scenegraph_update_entity(entity) ||
      !ldk_transform_get_world_matrix(entity, &world))
  {
    return false;
  }

  *out_position = vec3_make(world.m[12], world.m[13], world.m[14]);
  return true;
}

static bool s_rebuild(TFTFDecorationSystem *system,
    const IslandMapDataView *map, i32 center_x, i32 center_y, u64 config_hash)
{
  LDKAssetManager *assets;
  i64 min_x;
  i64 min_y;
  i64 max_x;
  i64 max_y;
  u32 active_cells = 0u;

  if (!system || !map || !map->texels || system->radius > INT32_MAX)
  {
    return false;
  }

  assets = (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets || !ldk_terrain_system_is_active())
  {
    return false;
  }

  min_x = (i64)center_x - (i64)system->radius;
  min_y = (i64)center_y - (i64)system->radius;
  max_x = (i64)center_x + (i64)system->radius;
  max_y = (i64)center_y + (i64)system->radius;

  if (min_x < 0)
  {
    min_x = 0;
  }
  if (min_y < 0)
  {
    min_y = 0;
  }
  if (max_x >= (i64)map->width)
  {
    max_x = (i64)map->width - 1;
  }
  if (max_y >= (i64)map->height)
  {
    max_y = (i64)map->height - 1;
  }
  if (min_x > max_x || min_y > max_y)
  {
    return false;
  }

  s_batch_counts_reset();

  for (i64 y = min_y; y <= max_y; ++y)
  {
    for (i64 x = min_x; x <= max_x; ++x)
    {
      u64 cell_index = (u64)y * (u64)map->width + (u64)x;
      const u16 *texel =
          &map->texels[cell_index * TFTF_ISLAND_MAP_CHANNEL_COUNT];
      u16 packed = texel[TFTF_ISLAND_MAP_CHANNEL_DECORATION];
      u8 mask = island_terrain_map_decoration_mask_get(packed);
      TFTFIslandTileKind kind;

      ++active_cells;
      if (mask == 0u || !tftf_island_terrain_tile_kind_at_cell(
                             (i32)x, (i32)y, &kind))
      {
        continue;
      }

      float density = island_terrain_map_density_get(packed);
      Vec2 cell_min;
      Vec2 cell_max;
      if (!island_terrain_cell_world_bounds(
              (i32)x, (i32)y, &cell_min, &cell_max))
      {
        return false;
      }

      for (u32 slot = 0u; slot < TFTF_DECORATION_SLOT_COUNT; ++slot)
      {
        TFTFDecorationSlotConfig config;
        u32 instance_count;

        if ((mask & (u8)(1u << slot)) == 0u ||
            !s_slot_config_get(system, kind, slot, &config))
        {
          continue;
        }
        s_slot_config_normalize(&config);
        if (x_handle_is_null(config.mesh.h) ||
            !ldk_asset_manager_mesh_is_alive(assets, config.mesh))
        {
          continue;
        }

        instance_count = s_instance_count_for_cell((i32)x, (i32)y,
            map->seed, kind, slot, density, config.min_count,
            config.max_count);
        if (instance_count == 0u)
        {
          continue;
        }

        TFTFDecorationBatch *batch =
            s_batch_get(config.mesh, config.material);
        if (!batch)
        {
          return false;
        }

        for (u32 instance = 0u; instance < instance_count; ++instance)
        {
          float tx = s_hash_rand01((i32)x, (i32)y,
              s_instance_seed(map->seed, kind, slot, instance,
                  0x9e3779b9u));
          float tz = s_hash_rand01((i32)x, (i32)y,
              s_instance_seed(map->seed, kind, slot, instance,
                  0x243f6a88u));
          float angle = s_hash_rand01((i32)x, (i32)y,
              s_instance_seed(map->seed, kind, slot, instance,
                  0xb7e15162u)) *
              (2.0f * STDXM_PI);
          float scale_t = s_hash_rand01((i32)x, (i32)y,
              s_instance_seed(map->seed, kind, slot, instance,
                  0x94d049bbu));
          float scale = config.min_scale +
              (config.max_scale - config.min_scale) * scale_t;
          float world_x =
              cell_min.x + (cell_max.x - cell_min.x) * tx;
          float world_z =
              cell_min.y + (cell_max.y - cell_min.y) * tz;
          float world_y;

          if (!ldk_terrain_height_at_world(world_x, world_z, &world_y))
          {
            return false;
          }

          Mat4 transform = mat4_compose(
              vec3_make(world_x, world_y, world_z),
              quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), angle),
              vec3_make(scale, scale, scale));
          if (!s_batch_append(batch, transform))
          {
            return false;
          }
        }
      }
    }
  }

  if (!s_batches_commit(system, assets))
  {
    return false;
  }

  system->center_x = center_x;
  system->center_y = center_y;
  system->map_revision = map->revision;
  system->active_cell_count = active_cells;
  ++system->rebuild_count;
  s_runtime.config_hash = config_hash;
  s_runtime.built = true;
  return true;
}

int tftf_decoration_system_initialize(void *data)
{
  TFTFDecorationSystem *system = data;

  if (!system || s_runtime.owner)
  {
    return -1;
  }

  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.owner = system;
  system->center_x = INT32_MIN;
  system->center_y = INT32_MIN;
  system->map_revision = 0u;
  system->active_cell_count = 0u;
  system->batch_count = 0u;
  system->instance_count = 0u;
  system->rebuild_count = 0u;
  return 0;
}

void tftf_decoration_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  TFTFDecorationSystem *system = data;
  IslandMapDataView map;
  Vec3 player_position;
  i32 center_x;
  i32 center_y;
  u64 config_hash;

  (void)dt;

  if (!system || s_runtime.owner != system || !group || group->count == 0u)
  {
    if (system && s_runtime.owner == system && system->instance_count > 0u)
    {
      s_rendered_instances_clear(system);
    }
    return;
  }

  if (!island_terrain_map_data_get(&map) ||
      !s_player_world_position(group->entities[0], &player_position) ||
      !island_terrain_world_to_cell(player_position, &center_x, &center_y))
  {
    return;
  }

  config_hash = s_config_hash(system);
  if (s_runtime.built && system->center_x == center_x &&
      system->center_y == center_y && system->map_revision == map.revision &&
      s_runtime.config_hash == config_hash)
  {
    return;
  }

  (void)s_rebuild(system, &map, center_x, center_y, config_hash);
}

void tftf_decoration_system_terminate(void *data)
{
  TFTFDecorationSystem *system = data;

  if (!system || s_runtime.owner != system)
  {
    return;
  }

  for (u32 i = 0u; i < s_runtime.batch_count; ++i)
  {
    TFTFDecorationBatch *batch = &s_runtime.batches[i];
    if (!x_handle_is_null(batch->entity))
    {
      ldk_ecs_entity_destroy(batch->entity);
    }
    free(batch->instances);
  }
  free(s_runtime.batches);
  memset(&s_runtime, 0, sizeof(s_runtime));

  system->center_x = INT32_MIN;
  system->center_y = INT32_MIN;
  system->map_revision = 0u;
  system->active_cell_count = 0u;
  system->batch_count = 0u;
  system->instance_count = 0u;
}
