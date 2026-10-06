#include "island_terrain.h"
#include "tftf_terrain_atlas.h"

#include <component/ldk_transform.h>
#include <ldk.h>
#include <ldk_material_asset.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <system/ldk_grass_system.h>
#include <system/ldk_terrain_system.h>
#include <stdx/stdx_math.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISLAND_MAP_FILE_NAME "map.bin"
#define ISLAND_MAP_FILE_MAGIC 0x4C444B4Du
#define ISLAND_MAP_FILE_VERSION 2u
#define ISLAND_MAP_GENERATOR_VERSION 5u

#define ISLAND_TERRAIN_COLOR_DEEP_WATER 0x426F7DFFu
#define ISLAND_TERRAIN_COLOR_SHALLOW_WATER 0x6696A0FFu
#define ISLAND_TERRAIN_COLOR_SAND 0xC8AA74FFu
#define ISLAND_TERRAIN_COLOR_GRASS 0x78945AFFu
#define ISLAND_TERRAIN_COLOR_GRASS_DARK 0x657F4DFFu
#define ISLAND_TERRAIN_COLOR_ROCK 0x77766FFFu

#define ISLAND_DECORATION_RULE_COUNT 7u
#define ISLAND_RESOURCE_RULE_COUNT 6u

#define ISLAND_MAP_CHANNEL_HEIGHT 0u
#define ISLAND_MAP_CHANNEL_DECORATION 1u
#define ISLAND_MAP_CHANNEL_RESOURCE 2u
#define ISLAND_MAP_CHANNEL_RESERVED 3u
#define ISLAND_MAP_CHANNEL_COUNT 4u

#define ISLAND_MAP_PROP_TYPE_SHIFT 8u
#define ISLAND_MAP_PROP_DENSITY_MASK 0xffu

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef enum IslandTerrainClass
{
  ISLAND_TERRAIN_CLASS_DEEP_WATER,
  ISLAND_TERRAIN_CLASS_SHALLOW_WATER,
  ISLAND_TERRAIN_CLASS_SHORE,
  ISLAND_TERRAIN_CLASS_LAND,
  ISLAND_TERRAIN_CLASS_COUNT
} IslandTerrainClass;

typedef enum IslandBiome
{
  ISLAND_BIOME_MOUNTAIN,
  ISLAND_BIOME_DRY,
  ISLAND_BIOME_GRASS,
  ISLAND_BIOME_FOREST,
  ISLAND_BIOME_THORNS,
  ISLAND_BIOME_COUNT
} IslandBiome;

typedef enum IslandTerrainSurface
{
  /* Ordered from lowest to highest visual layer. */
  ISLAND_TERRAIN_SURFACE_DEEP_WATER,
  ISLAND_TERRAIN_SURFACE_SHALLOW_WATER,
  ISLAND_TERRAIN_SURFACE_SAND,
  ISLAND_TERRAIN_SURFACE_GRASS_DARK,
  ISLAND_TERRAIN_SURFACE_GRASS,
  ISLAND_TERRAIN_SURFACE_ROCK,
  ISLAND_TERRAIN_SURFACE_COUNT
} IslandTerrainSurface;

typedef enum IslandTerrainCutoutBit
{
  ISLAND_TERRAIN_CUTOUT_NORTH = 1u << 0,
  ISLAND_TERRAIN_CUTOUT_EAST = 1u << 1,
  ISLAND_TERRAIN_CUTOUT_SOUTH = 1u << 2,
  ISLAND_TERRAIN_CUTOUT_WEST = 1u << 3
} IslandTerrainCutoutBit;

#define ISLAND_TERRAIN_CUTOUT_MASK_COUNT 16u
#define ISLAND_TERRAIN_LAYER_ELEVATION_STEP 0.0005f

typedef struct IslandMapCell
{
  u8 terrain_class;
  u8 biome;
  u8 decoration_rule;
  u8 resource_rule;
} IslandMapCell;

typedef struct IslandMap
{
  IslandMapCell *cells;
  /* Interleaved RGBA16 UNORM map data, one texel per 1x1 metre cell. */
  u16 *data;
  u32 width;
  u32 height;
  u32 revision;
} IslandMap;

typedef struct IslandMapFileHeader
{
  u32 magic;
  u32 format_version;
  u32 generator_version;
  u32 seed;
  u32 width;
  u32 height;
  u64 config_hash;
} IslandMapFileHeader;

typedef struct IslandMapPropRule
{
  IslandTerrainClass terrain_class;
  IslandBiome biome;
  u32 seed;
  float chance;
  u8 value;
} IslandMapPropRule;

typedef struct IslandTerrainBounds
{
  i32 min_x;
  i32 min_y;
  i32 max_x;
  i32 max_y;
  bool valid;
} IslandTerrainBounds;

typedef struct IslandTerrainTile
{
  i32 x;
  i32 y;
  LDKGrassPatch grass;
} IslandTerrainTile;

typedef struct IslandTerrainUVRect
{
  float u0;
  float v0;
  float u1;
  float v1;
} IslandTerrainUVRect;

typedef struct IslandTerrainRuntime
{
  IslandTerrain *owner;

  IslandTerrainTile *tiles;
  u32 tile_count;
  u32 tile_capacity;
  float *grass_density_scales;

  LDKMeshVertex *vertices;
  u32 vertex_capacity;

  u32 *indices;
  u32 index_capacity;

  LDKAssetMaterial material_asset;
  u64 material_revision;
  LDKResourceMaterial renderer_material;
  LDKResourceTexture renderer_texture;
  LDKResourceTexture renderer_normal_map;
  LDKResourceTexture renderer_specular_map;
  LDKAssetImage terrain_image;
  LDKGrassTypeId dry_grass_type_id;
  LDKGrassTypeId grass_type_id;
  LDKGrassTypeId forest_grass_type_id;
  LDKGrassTypeId thorn_grass_type_id;
  LDKGrassTypeId shallow_grass_type_id;

  /* IslandTerrain can initialize before TerrainSystem has consumed the
   * procedural heightmap. Grass patches must not fall back to the old flat
   * tile elevation in that case; retry once terrain heights are queryable. */
  bool grass_height_sync_pending;
} IslandTerrainRuntime;

static IslandMap s_map;
static IslandTerrainRuntime s_runtime;

static uint32_t s_hash_u32(uint32_t x)
{
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static uint32_t s_hash_2d_i32(int x, int y, uint32_t seed)
{
  uint32_t h = seed;
  h ^= s_hash_u32((uint32_t)x + 0x9e3779b9u);
  h ^= s_hash_u32((uint32_t)y + 0x85ebca6bu);
  return s_hash_u32(h);
}

static bool s_island_terrain_tile_grass_update(
    IslandTerrain *system, IslandTerrainTile *tile);

static float s_hash_2d_rand01(int x, int y, uint32_t seed)
{
  uint32_t h = s_hash_2d_i32(x, y, seed);
  return (float)(h >> 8) * (1.0f / 16777215.0f);
}

static float s_lerp_f32(float a, float b, float t)
{
  return a + (b - a) * t;
}

static float s_smoothstep_f32(float t)
{
  return t * t * (3.0f - 2.0f * t);
}

static float s_value_noise_2d(float x, float y, uint32_t seed)
{
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  float tx = x - (float)x0;
  float ty = y - (float)y0;
  float a;
  float b;
  float c;
  float d;
  float top;
  float bottom;

  tx = s_smoothstep_f32(tx);
  ty = s_smoothstep_f32(ty);

  a = s_hash_2d_rand01(x0, y0, seed);
  b = s_hash_2d_rand01(x1, y0, seed);
  c = s_hash_2d_rand01(x0, y1, seed);
  d = s_hash_2d_rand01(x1, y1, seed);

  top = s_lerp_f32(a, b, tx);
  bottom = s_lerp_f32(c, d, tx);
  return s_lerp_f32(top, bottom, ty);
}

static float s_fbm_compose_2d(
    float x, float y, uint32_t seed, u32 octaves)
{
  float value = 0.0f;
  float amplitude = 1.0f;
  float frequency = 1.0f;
  float amplitude_sum = 0.0f;

  for (u32 i = 0u; i < octaves; ++i)
  {
    uint32_t octave_seed = seed + i * 1013u;

    value += s_value_noise_2d(
                 x * frequency, y * frequency, octave_seed) *
             amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.0f;
    amplitude *= 0.5f;
  }

  return amplitude_sum > 0.0f ? value / amplitude_sum : 0.0f;
}

static void s_island_terrain_generation_defaults(IslandTerrain *system)
{
  if (!system)
  {
    return;
  }

  if (x_smallstr_length(&system->dry_grass_type_name) == 0u)
  {
    x_smallstr_from_cstr(&system->dry_grass_type_name, "dry_grass");
  }
  if (x_smallstr_length(&system->grass_type_name) == 0u)
  {
    x_smallstr_from_cstr(&system->grass_type_name, "grass");
  }
  if (x_smallstr_length(&system->forest_grass_type_name) == 0u)
  {
    x_smallstr_from_cstr(&system->forest_grass_type_name, "forest_grass");
  }

  if (x_smallstr_length(&system->thorn_grass_type_name) == 0u)
  {
    x_smallstr_from_cstr(&system->thorn_grass_type_name, "thorn_grass");
  }

  if (x_smallstr_length(&system->shallow_grass_type_name) == 0u)
  {
    x_smallstr_from_cstr(&system->shallow_grass_type_name, "shallow_grass");
  }

  /* Older scenes omit these new fields. Zero spacing disables inland pools. */
  if (system->thorn_noise_scale == 0.0f && system->thorn_noise_min == 0.0f)
  {
    system->thorn_noise_scale = 0.025f;
    system->thorn_noise_min = 0.60f;
  }

  if (system->shallow_water_height == 0.0f && system->max_height == 0.0f)
  {
    system->deep_water_height = 0.00f;
    system->shallow_water_height = 0.10f;
    system->dry_height = 0.17f;
    system->grass_height = 0.20f;
    system->forest_height = 0.23f;
    system->thorn_height = 0.25f;
    system->mountain_height = 0.27f;
    system->max_height = 0.36f;
    system->shallow_slope_fraction = 1.00f;
  }

  if (system->map_size != 0u)
  {
    if (system->seed == 0u)
    {
      system->seed = 1u;
    }
    return;
  }

  system->seed = system->seed ? system->seed : 1u;
  system->map_size = 800u;
  system->land_scale = 0.0097f;
  system->island_radius = 0.8f;
  system->terrain_scale = 0.010f;
  system->terrain_height_strength = 0.85f;
  system->surface_noise_scale = 0.050f;
  system->surface_noise_octaves = 3u;
  system->surface_noise_strength = 0.30f;
  system->moisture_scale = 0.018f;
  system->land_octaves = 5u;
  system->terrain_octaves = 4u;
  system->moisture_octaves = 3u;
  system->land_noise_strength = 0.75f;
  system->height_smoothing_strength = 0.90f;
  system->height_smoothing_radius = 8u;
  system->height_smoothing_edge_threshold = 0.008f;
  system->height_smoothing_passes = 2u;
  system->deep_water_max = 0.00f;
  system->shallow_water_max = 0.06f;
  system->shore_max = 0.11f;
  system->mountain_min = 0.70f;
  system->dry_moisture_max = 0.25f;
  system->grass_moisture_max = 0.44f;
  system->thorn_noise_scale = 0.025f;
  system->thorn_noise_min = 0.60f;
  system->thorn_grass_bias = 0.12f;
  system->shallow_patch_spacing = 90.0f;
  system->shallow_patch_chance = 0.35f;
  system->shallow_patch_min_radius = 6.0f;
  system->shallow_patch_max_radius = 11.0f;
  system->shallow_patch_bank_width = 10.0f;

  system->forest_decoration_chance = 0.18f;
  system->grass_decoration_0_chance = 0.03f;
  system->grass_decoration_1_chance = 0.18f;
  system->mountain_decoration_chance = 0.14f;
  system->dry_decoration_chance = 0.04f;
  system->shore_decoration_chance = 0.025f;
  system->shallow_water_decoration_chance = 0.015f;

  system->mountain_resource_chance = 0.003f;
  system->forest_resource_chance = 0.003f;
  system->grass_resource_chance = 0.0002f;
  system->dry_resource_chance = 0.002f;
  system->shore_resource_chance = 0.0015f;
  system->shallow_water_resource_chance = 0.001f;
}

static bool s_island_chance_is_valid(float chance)
{
  return isfinite(chance) && chance >= 0.0f && chance <= 1.0f;
}

static bool s_island_terrain_generation_valid(const IslandTerrain *system)
{
  u64 cell_count;

  if (!system || system->seed == 0u || system->map_size == 0u ||
      system->map_size > INT32_MAX)
  {
    return false;
  }

  cell_count = (u64)system->map_size * (u64)system->map_size;
  if (cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(IslandMapCell)))
  {
    return false;
  }

  if (!isfinite(system->land_scale) || system->land_scale <= 0.0f ||
      !isfinite(system->island_radius) || system->island_radius <= 0.0f ||
      !isfinite(system->terrain_scale) || system->terrain_scale <= 0.0f ||
      !isfinite(system->terrain_height_strength) ||
      system->terrain_height_strength < 0.0f ||
      system->terrain_height_strength > 1.0f ||
      !isfinite(system->surface_noise_scale) ||
      system->surface_noise_scale <= 0.0f ||
      system->surface_noise_octaves == 0u ||
      system->surface_noise_octaves > 8u ||
      !isfinite(system->surface_noise_strength) ||
      system->surface_noise_strength < 0.0f ||
      system->surface_noise_strength > 0.5f ||
      !isfinite(system->moisture_scale) || system->moisture_scale <= 0.0f ||
      system->land_octaves == 0u || system->land_octaves > 32u ||
      system->terrain_octaves == 0u || system->terrain_octaves > 32u ||
      system->moisture_octaves == 0u || system->moisture_octaves > 32u ||
      !isfinite(system->land_noise_strength) ||
      system->land_noise_strength < 0.0f ||
      !isfinite(system->height_smoothing_strength) ||
      system->height_smoothing_strength < 0.0f ||
      system->height_smoothing_strength > 1.0f ||
      system->height_smoothing_radius > 24u ||
      !isfinite(system->height_smoothing_edge_threshold) ||
      system->height_smoothing_edge_threshold < 0.0f ||
      system->height_smoothing_edge_threshold > 0.1f ||
      system->height_smoothing_passes > 8u)
  {
    return false;
  }

  if (!isfinite(system->deep_water_max) ||
      !isfinite(system->shallow_water_max) ||
      !isfinite(system->shore_max) ||
      !(system->deep_water_max < system->shallow_water_max) ||
      !(system->shallow_water_max < system->shore_max))
  {
    return false;
  }

  if (!isfinite(system->mountain_min) || system->mountain_min < 0.0f ||
      system->mountain_min > 1.0f ||
      !isfinite(system->dry_moisture_max) ||
      !isfinite(system->grass_moisture_max) ||
      system->dry_moisture_max < 0.0f ||
      system->grass_moisture_max > 1.0f ||
      !(system->dry_moisture_max < system->grass_moisture_max))
  {
    return false;
  }

  if (!isfinite(system->deep_water_height) ||
      system->deep_water_height < 0.0f ||
      !isfinite(system->shallow_water_height) ||
      !isfinite(system->dry_height) || !isfinite(system->grass_height) ||
      !isfinite(system->forest_height) || !isfinite(system->thorn_height) ||
      !isfinite(system->mountain_height) || !isfinite(system->max_height) ||
      system->max_height > 1.0f ||
      !(system->deep_water_height < system->shallow_water_height) ||
      !(system->shallow_water_height < system->dry_height) ||
      !(system->dry_height < system->grass_height) ||
      !(system->grass_height < system->forest_height) ||
      !(system->forest_height < system->thorn_height) ||
      !(system->thorn_height < system->mountain_height) ||
      !(system->mountain_height < system->max_height) ||
      !isfinite(system->shallow_slope_fraction) ||
      system->shallow_slope_fraction <= 0.0f ||
      system->shallow_slope_fraction > 1.0f)
  {
    return false;
  }

  if (!isfinite(system->thorn_noise_scale) ||
      system->thorn_noise_scale <= 0.0f ||
      !s_island_chance_is_valid(system->thorn_noise_min) ||
      !s_island_chance_is_valid(system->thorn_grass_bias) ||
      !isfinite(system->shallow_patch_spacing) ||
      system->shallow_patch_spacing < 0.0f ||
      !s_island_chance_is_valid(system->shallow_patch_chance))
  {
    return false;
  }

  if (system->shallow_patch_spacing > 0.0f &&
      (!isfinite(system->shallow_patch_min_radius) ||
          system->shallow_patch_min_radius < 1.0f ||
          !isfinite(system->shallow_patch_max_radius) ||
          system->shallow_patch_max_radius < system->shallow_patch_min_radius ||
          !isfinite(system->shallow_patch_bank_width) ||
          system->shallow_patch_bank_width < 1.0f ||
          1.2f * system->shallow_patch_max_radius +
                  system->shallow_patch_bank_width >
              system->shallow_patch_spacing * 0.5f))
  {
    return false;
  }

  return s_island_chance_is_valid(system->forest_decoration_chance) &&
         s_island_chance_is_valid(system->grass_decoration_0_chance) &&
         s_island_chance_is_valid(system->grass_decoration_1_chance) &&
         s_island_chance_is_valid(system->mountain_decoration_chance) &&
         s_island_chance_is_valid(system->dry_decoration_chance) &&
         s_island_chance_is_valid(system->shore_decoration_chance) &&
         s_island_chance_is_valid(
             system->shallow_water_decoration_chance) &&
         s_island_chance_is_valid(system->mountain_resource_chance) &&
         s_island_chance_is_valid(system->forest_resource_chance) &&
         s_island_chance_is_valid(system->grass_resource_chance) &&
         s_island_chance_is_valid(system->dry_resource_chance) &&
         s_island_chance_is_valid(system->shore_resource_chance) &&
         s_island_chance_is_valid(system->shallow_water_resource_chance);
}

static void s_island_hash_bytes(u64 *hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;

  for (size_t i = 0u; i < size; ++i)
  {
    *hash ^= (u64)bytes[i];
    *hash *= UINT64_C(1099511628211);
  }
}

static u64 s_island_terrain_generation_hash(const IslandTerrain *system)
{
  u64 hash = UINT64_C(14695981039346656037);

#define ISLAND_HASH_FIELD(field)                                               \
  s_island_hash_bytes(&hash, &system->field, sizeof(system->field))

  ISLAND_HASH_FIELD(seed);
  ISLAND_HASH_FIELD(map_size);
  ISLAND_HASH_FIELD(land_scale);
  ISLAND_HASH_FIELD(island_radius);
  ISLAND_HASH_FIELD(terrain_scale);
  ISLAND_HASH_FIELD(terrain_height_strength);
  ISLAND_HASH_FIELD(surface_noise_scale);
  ISLAND_HASH_FIELD(surface_noise_octaves);
  ISLAND_HASH_FIELD(surface_noise_strength);
  ISLAND_HASH_FIELD(moisture_scale);
  ISLAND_HASH_FIELD(land_octaves);
  ISLAND_HASH_FIELD(terrain_octaves);
  ISLAND_HASH_FIELD(moisture_octaves);
  ISLAND_HASH_FIELD(land_noise_strength);
  ISLAND_HASH_FIELD(height_smoothing_strength);
  ISLAND_HASH_FIELD(height_smoothing_radius);
  ISLAND_HASH_FIELD(height_smoothing_edge_threshold);
  ISLAND_HASH_FIELD(height_smoothing_passes);
  ISLAND_HASH_FIELD(deep_water_height);
  ISLAND_HASH_FIELD(shallow_water_height);
  ISLAND_HASH_FIELD(dry_height);
  ISLAND_HASH_FIELD(grass_height);
  ISLAND_HASH_FIELD(forest_height);
  ISLAND_HASH_FIELD(thorn_height);
  ISLAND_HASH_FIELD(mountain_height);
  ISLAND_HASH_FIELD(max_height);
  ISLAND_HASH_FIELD(shallow_slope_fraction);
  ISLAND_HASH_FIELD(deep_water_max);
  ISLAND_HASH_FIELD(shallow_water_max);
  ISLAND_HASH_FIELD(shore_max);
  ISLAND_HASH_FIELD(mountain_min);
  ISLAND_HASH_FIELD(dry_moisture_max);
  ISLAND_HASH_FIELD(grass_moisture_max);
  ISLAND_HASH_FIELD(thorn_noise_scale);
  ISLAND_HASH_FIELD(thorn_noise_min);
  ISLAND_HASH_FIELD(thorn_grass_bias);
  ISLAND_HASH_FIELD(shallow_patch_spacing);
  ISLAND_HASH_FIELD(shallow_patch_chance);
  ISLAND_HASH_FIELD(shallow_patch_min_radius);
  ISLAND_HASH_FIELD(shallow_patch_max_radius);
  ISLAND_HASH_FIELD(shallow_patch_bank_width);
  ISLAND_HASH_FIELD(forest_decoration_chance);
  ISLAND_HASH_FIELD(grass_decoration_0_chance);
  ISLAND_HASH_FIELD(grass_decoration_1_chance);
  ISLAND_HASH_FIELD(mountain_decoration_chance);
  ISLAND_HASH_FIELD(dry_decoration_chance);
  ISLAND_HASH_FIELD(shore_decoration_chance);
  ISLAND_HASH_FIELD(shallow_water_decoration_chance);
  ISLAND_HASH_FIELD(mountain_resource_chance);
  ISLAND_HASH_FIELD(forest_resource_chance);
  ISLAND_HASH_FIELD(grass_resource_chance);
  ISLAND_HASH_FIELD(dry_resource_chance);
  ISLAND_HASH_FIELD(shore_resource_chance);
  ISLAND_HASH_FIELD(shallow_water_resource_chance);

#undef ISLAND_HASH_FIELD

  return hash;
}

static IslandTerrainClass s_island_terrain_class_select(
    const IslandTerrain *system, float land)
{
  if (land < system->deep_water_max)
  {
    return ISLAND_TERRAIN_CLASS_DEEP_WATER;
  }

  if (land < system->shallow_water_max)
  {
    return ISLAND_TERRAIN_CLASS_SHALLOW_WATER;
  }

  if (land < system->shore_max)
  {
    return ISLAND_TERRAIN_CLASS_SHORE;
  }

  return ISLAND_TERRAIN_CLASS_LAND;
}

static IslandBiome s_island_biome_select(
    const IslandTerrain *system, float terrain, float moisture, float thorns)
{
  if (terrain >= system->mountain_min)
  {
    return ISLAND_BIOME_MOUNTAIN;
  }

  /* Encourage thorn clearings inside tall grass without excluding the
   * independent patches elsewhere. Mountains retain their priority. */
  if (moisture >= system->dry_moisture_max &&
      moisture < system->grass_moisture_max)
  {
    thorns += system->thorn_grass_bias;
  }

  if (thorns >= system->thorn_noise_min)
  {
    return ISLAND_BIOME_THORNS;
  }

  if (moisture < system->dry_moisture_max)
  {
    return ISLAND_BIOME_DRY;
  }

  if (moisture < system->grass_moisture_max)
  {
    return ISLAND_BIOME_GRASS;
  }

  return ISLAND_BIOME_FOREST;
}

static u16 s_island_map_prop_pack(u8 type, float density)
{
  if (density < 0.0f)
  {
    density = 0.0f;
  }
  else if (density > 1.0f)
  {
    density = 1.0f;
  }

  u32 density_u8 = (u32)(density * 255.0f + 0.5f);
  return (u16)(((u16)type << ISLAND_MAP_PROP_TYPE_SHIFT) |
      (u16)(density_u8 & ISLAND_MAP_PROP_DENSITY_MASK));
}

static u8 s_island_map_prop_type(u16 packed)
{
  return (u8)(packed >> ISLAND_MAP_PROP_TYPE_SHIFT);
}

static u16 s_island_map_prop_select(u32 x, u32 y, float dist,
    IslandTerrainClass terrain_class, IslandBiome biome, u32 world_seed,
    const IslandMapPropRule *rules, u32 rule_count, bool use_center_bias)
{
  for (u32 rule_index = 0u; rule_index < rule_count; ++rule_index)
  {
    const IslandMapPropRule *rule = &rules[rule_index];
    float random;
    float value;
    float density;

    if (rule->terrain_class != terrain_class)
    {
      continue;
    }

    if (terrain_class == ISLAND_TERRAIN_CLASS_LAND && rule->biome != biome)
    {
      continue;
    }

    random = s_hash_2d_rand01((int)x, (int)y, world_seed ^ rule->seed);
    value = use_center_bias ? random * dist : random;

    if (value < rule->chance)
    {
      density = rule->chance;
      if (use_center_bias && dist > 1e-6f)
      {
        density = rule->chance / dist;
      }
      return s_island_map_prop_pack(rule->value, density);
    }
  }

  return 0u;
}

static u16 s_island_map_height_encode(const IslandTerrain *system,
    IslandTerrainClass terrain_class, IslandBiome biome, float land,
    float terrain, float surface_noise, const float *biome_terrain_min,
    const float *biome_terrain_max)
{
  float height;

  switch (terrain_class)
  {
  case ISLAND_TERRAIN_CLASS_DEEP_WATER:
    height = system->deep_water_height;
    break;

  case ISLAND_TERRAIN_CLASS_SHALLOW_WATER:
  {
    /* Slope within the configured fraction of the coastal shallow band. */
    float t = (land - system->deep_water_max) /
        ((system->shallow_water_max - system->deep_water_max) *
            system->shallow_slope_fraction);
    t = fminf(1.0f, fmaxf(0.0f, t));
    height = s_lerp_f32(system->deep_water_height,
        system->shallow_water_height, s_smoothstep_f32(t));
    break;
  }

  case ISLAND_TERRAIN_CLASS_SHORE:
  {
    float t = (land - system->shallow_water_max) /
        (system->shore_max - system->shallow_water_max);
    t = fminf(1.0f, fmaxf(0.0f, t));
    height = s_lerp_f32(system->shallow_water_height,
        system->dry_height, s_smoothstep_f32(t));
    break;
  }

  case ISLAND_TERRAIN_CLASS_LAND:
  default:
  {
    float terrain_min = biome_terrain_min[(u32)biome];
    float terrain_max = biome_terrain_max[(u32)biome];
    float terrain_range = terrain_max - terrain_min;
    float relief = terrain_range > 1e-6f
        ? (terrain - terrain_min) / terrain_range
        : 0.5f;
    float base_height;
    float top_height;

    if (relief < 0.0f)
    {
      relief = 0.0f;
    }
    else if (relief > 1.0f)
    {
      relief = 1.0f;
    }

    /*
     * The large terrain fBm can be locally very smooth. Add a small,
     * independent higher-frequency component so even broad biome interiors
     * retain some surface relief. The value is expressed as a fraction of
     * the biome's own height band, so it cannot change biome classification.
     */
    relief +=
        (surface_noise * 2.0f - 1.0f) * system->surface_noise_strength;
    if (relief < 0.0f)
    {
      relief = 0.0f;
    }
    else if (relief > 1.0f)
    {
      relief = 1.0f;
    }

    /*
     * Use the complete height band reserved for this biome. Keep a very small
     * margin below the next band's threshold so the TerrainSystem still
     * classifies the generated height unambiguously.
     */
    switch (biome)
    {
    case ISLAND_BIOME_DRY:
      base_height = system->dry_height;
      top_height = system->grass_height - 0.0005f;
      break;

    case ISLAND_BIOME_GRASS:
      base_height = system->grass_height;
      top_height = system->forest_height - 0.0005f;
      break;

    case ISLAND_BIOME_FOREST:
      base_height = system->forest_height;
      top_height = system->thorn_height - 0.0005f;
      break;

    case ISLAND_BIOME_THORNS:
      base_height = system->thorn_height;
      top_height = system->mountain_height - 0.0005f;
      break;

    case ISLAND_BIOME_MOUNTAIN:
    default:
      base_height = system->mountain_height;
      top_height = system->max_height;
      break;
    }

    height = s_lerp_f32(base_height, top_height, relief);
    break;
  }
  }

  if (height < 0.0f)
  {
    height = 0.0f;
  }
  else if (height > 1.0f)
  {
    height = 1.0f;
  }

  return (u16)(height * 65535.0f + 0.5f);
}

static void s_island_map_release(void)
{
  free(s_map.cells);
  free(s_map.data);
  memset(&s_map, 0, sizeof(s_map));
}

static void s_island_map_take(
    IslandMapCell *cells, u16 *map_data, u32 width, u32 height)
{
  u32 revision = s_map.revision + 1u;

  if (revision == 0u)
  {
    revision = 1u;
  }

  free(s_map.cells);
  free(s_map.data);
  s_map.cells = cells;
  s_map.data = map_data;
  s_map.width = width;
  s_map.height = height;
  s_map.revision = revision;
}

static bool s_island_map_cell_valid(const IslandMapCell *cell)
{
  if (!cell || cell->terrain_class >= ISLAND_TERRAIN_CLASS_COUNT ||
      cell->biome >= ISLAND_BIOME_COUNT ||
      cell->decoration_rule > ISLAND_DECORATION_RULE_COUNT ||
      cell->resource_rule > ISLAND_RESOURCE_RULE_COUNT)
  {
    return false;
  }

  return true;
}

static bool s_island_map_data_valid(const u16 *data, u64 cell_count)
{
  if (!data)
  {
    return false;
  }

  for (u64 i = 0u; i < cell_count; ++i)
  {
    const u16 *texel = &data[i * ISLAND_MAP_CHANNEL_COUNT];
    if (s_island_map_prop_type(texel[ISLAND_MAP_CHANNEL_DECORATION]) >
            ISLAND_DECORATION_RULE_COUNT ||
        s_island_map_prop_type(texel[ISLAND_MAP_CHANNEL_RESOURCE]) >
            ISLAND_RESOURCE_RULE_COUNT)
    {
      return false;
    }
  }
  return true;
}

#ifdef LDK_SHAREDLIB
static bool s_island_map_height_debug_write_ppm(const char *path)
{
  FILE *file;
  u64 cell_count;
  u16 min_height = UINT16_MAX;
  u16 max_height = 0u;
  float range;

  if (!path || !s_map.data || s_map.width == 0u || s_map.height == 0u)
  {
    return false;
  }

  cell_count = (u64)s_map.width * (u64)s_map.height;
  for (u64 i = 0u; i < cell_count; ++i)
  {
    u16 height =
        s_map.data[i * ISLAND_MAP_CHANNEL_COUNT + ISLAND_MAP_CHANNEL_HEIGHT];
    if (height < min_height)
    {
      min_height = height;
    }
    if (height > max_height)
    {
      max_height = height;
    }
  }

  file = fopen(path, "wb");
  if (!file)
  {
    ldk_log_warning("Could not write island heightmap debug image: %s\n",
        path);
    return false;
  }

  /*
   * P6 RGB instead of PGM because PPM tends to be easier to inspect with
   * generic image tools. Stretch the generated min/max range to 0..255 so
   * subtle relief is visible in the debug image. The exact source min/max is
   * logged below, so the visualization does not hide the real amplitude.
   */
  if (fprintf(file, "P6\n%u %u\n255\n", s_map.width, s_map.height) < 0)
  {
    fclose(file);
    return false;
  }

  range = max_height > min_height
      ? (float)(max_height - min_height)
      : 1.0f;

  for (u64 i = 0u; i < cell_count; ++i)
  {
    u16 height =
        s_map.data[i * ISLAND_MAP_CHANNEL_COUNT + ISLAND_MAP_CHANNEL_HEIGHT];
    float normalized = ((float)height - (float)min_height) / range;
    u8 value = (u8)(normalized * 255.0f + 0.5f);
    u8 rgb[3] = {value, value, value};

    if (fwrite(rgb, sizeof(rgb), 1u, file) != 1u)
    {
      fclose(file);
      return false;
    }
  }

  fclose(file);

  ldk_log_info(
      "Island heightmap debug PPM written to %s "
      "(source min=%.6f, max=%.6f, range=%.6f).\n",
      path,
      (float)min_height / 65535.0f,
      (float)max_height / 65535.0f,
      (float)(max_height - min_height) / 65535.0f);
  return true;
}
#endif

static bool s_island_map_load(IslandTerrain *system)
{
  IslandMapFileHeader header;
  IslandMapCell *cells = NULL;
  u16 *map_data = NULL;
  FILE *file;
  u64 cell_count;
  u64 value_count;
  u64 expected_hash;
  bool ok = false;

  if (!system)
  {
    return false;
  }

  file = fopen(ISLAND_MAP_FILE_NAME, "rb");
  if (!file)
  {
    return false;
  }

  memset(&header, 0, sizeof(header));
  if (fread(&header, sizeof(header), 1u, file) != 1u)
  {
    goto done;
  }

  expected_hash = s_island_terrain_generation_hash(system);
  if (header.magic != ISLAND_MAP_FILE_MAGIC ||
      header.format_version != ISLAND_MAP_FILE_VERSION ||
      header.generator_version != ISLAND_MAP_GENERATOR_VERSION ||
      header.seed != system->seed || header.width != system->map_size ||
      header.height != system->map_size || header.config_hash != expected_hash)
  {
    goto done;
  }

  cell_count = (u64)header.width * (u64)header.height;
  value_count = cell_count * ISLAND_MAP_CHANNEL_COUNT;
  if (cell_count == 0u || cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(*cells)) ||
      value_count > (u64)(SIZE_MAX / sizeof(*map_data)))
  {
    goto done;
  }

  cells = (IslandMapCell *)malloc((size_t)cell_count * sizeof(*cells));
  map_data = (u16 *)malloc((size_t)value_count * sizeof(*map_data));
  if (!cells || !map_data)
  {
    goto done;
  }

  if (fread(cells, sizeof(*cells), (size_t)cell_count, file) !=
          (size_t)cell_count ||
      fread(map_data, sizeof(*map_data), (size_t)value_count, file) !=
          (size_t)value_count)
  {
    goto done;
  }

  for (u64 i = 0u; i < cell_count; ++i)
  {
    if (!s_island_map_cell_valid(&cells[i]))
    {
      goto done;
    }
  }
  if (!s_island_map_data_valid(map_data, cell_count))
  {
    goto done;
  }

  s_island_map_take(cells, map_data, header.width, header.height);
  cells = NULL;
  map_data = NULL;
  ok = true;

done:
  free(cells);
  free(map_data);
  fclose(file);
  return ok;
}

static bool s_island_map_save(const IslandTerrain *system)
{
  IslandMapFileHeader header;
  FILE *file;
  u64 cell_count;
  u64 value_count;
  bool ok = false;

  if (!system || !s_map.cells || !s_map.data || s_map.width == 0u ||
      s_map.height == 0u)
  {
    return false;
  }

  cell_count = (u64)s_map.width * (u64)s_map.height;
  value_count = cell_count * ISLAND_MAP_CHANNEL_COUNT;
  if (cell_count == 0u || cell_count > SIZE_MAX ||
      value_count > SIZE_MAX / sizeof(*s_map.data))
  {
    return false;
  }

  memset(&header, 0, sizeof(header));
  header.magic = ISLAND_MAP_FILE_MAGIC;
  header.format_version = ISLAND_MAP_FILE_VERSION;
  header.generator_version = ISLAND_MAP_GENERATOR_VERSION;
  header.seed = system->seed;
  header.width = s_map.width;
  header.height = s_map.height;
  header.config_hash = s_island_terrain_generation_hash(system);

  file = fopen(ISLAND_MAP_FILE_NAME, "wb");
  if (!file)
  {
    return false;
  }

  if (fwrite(&header, sizeof(header), 1u, file) != 1u)
  {
    goto done;
  }

  if (fwrite(s_map.cells, sizeof(*s_map.cells), (size_t)cell_count, file) !=
          (size_t)cell_count ||
      fwrite(s_map.data, sizeof(*s_map.data), (size_t)value_count, file) !=
          (size_t)value_count)
  {
    goto done;
  }

  ok = true;

done:
  fclose(file);
  return ok;
}

static bool s_island_map_height_smooth_land(const IslandTerrain *system,
    const IslandMapCell *cells, u16 *map_data)
{
  u16 *source = NULL;
  u16 *target = NULL;
  u64 cell_count;
  float strength;
  i32 radius;
  float edge_threshold_u16;

  if (!system || !cells || !map_data)
  {
    return false;
  }

  if (system->height_smoothing_passes == 0u ||
      system->height_smoothing_strength <= 0.0f ||
      system->height_smoothing_radius == 0u)
  {
    return true;
  }

  cell_count = (u64)system->map_size * (u64)system->map_size;
  if (cell_count == 0u ||
      cell_count > (u64)(SIZE_MAX / sizeof(*source)))
  {
    return false;
  }

  source = (u16 *)malloc((size_t)cell_count * sizeof(*source));
  target = (u16 *)malloc((size_t)cell_count * sizeof(*target));
  if (!source || !target)
  {
    free(source);
    free(target);
    return false;
  }

  for (u64 i = 0u; i < cell_count; ++i)
  {
    source[i] =
        map_data[i * ISLAND_MAP_CHANNEL_COUNT + ISLAND_MAP_CHANNEL_HEIGHT];
  }

  strength = system->height_smoothing_strength;
  radius = (i32)system->height_smoothing_radius;
  edge_threshold_u16 =
      system->height_smoothing_edge_threshold * 65535.0f;

  for (u32 pass = 0u; pass < system->height_smoothing_passes; ++pass)
  {
    for (u32 y = 0u; y < system->map_size; ++y)
    {
      for (u32 x = 0u; x < system->map_size; ++x)
      {
        u64 index = (u64)y * (u64)system->map_size + (u64)x;
        bool near_hard_edge = false;
        double weighted_sum = 0.0;
        double weight_sum = 0.0;

        /* Preserve the analytic ocean floor and coastal ramp exactly. */
        if ((IslandTerrainClass)cells[index].terrain_class ==
                ISLAND_TERRAIN_CLASS_DEEP_WATER ||
            (IslandTerrainClass)cells[index].terrain_class ==
                ISLAND_TERRAIN_CLASS_SHALLOW_WATER)
        {
          target[index] = source[index];
          continue;
        }

        /*
         * Detect actual height discontinuities instead of only biome changes.
         * This also catches coast/shore steps such as the red regions in the
         * debug heightmap. Normal surface noise stays untouched because its
         * local delta is below the configured threshold.
         */
        for (i32 oy = -radius;
             oy <= radius && !near_hard_edge;
             ++oy)
        {
          for (i32 ox = -radius; ox <= radius; ++ox)
          {
            i32 sx;
            i32 sy;
            u64 sample_index;
            i32 neighbors[4][2];

            if (ox * ox + oy * oy > radius * radius)
            {
              continue;
            }

            sx = (i32)x + ox;
            sy = (i32)y + oy;
            if (sx < 0 || sy < 0 ||
                sx >= (i32)system->map_size ||
                sy >= (i32)system->map_size)
            {
              continue;
            }

            sample_index =
                (u64)sy * (u64)system->map_size + (u64)sx;

            neighbors[0][0] = sx - 1; neighbors[0][1] = sy;
            neighbors[1][0] = sx + 1; neighbors[1][1] = sy;
            neighbors[2][0] = sx;     neighbors[2][1] = sy - 1;
            neighbors[3][0] = sx;     neighbors[3][1] = sy + 1;

            for (u32 n = 0u; n < 4u; ++n)
            {
              i32 nx = neighbors[n][0];
              i32 ny = neighbors[n][1];
              u64 neighbor_index;
              float delta;

              if (nx < 0 || ny < 0 ||
                  nx >= (i32)system->map_size ||
                  ny >= (i32)system->map_size)
              {
                continue;
              }

              neighbor_index =
                  (u64)ny * (u64)system->map_size + (u64)nx;
              delta = fabsf(
                  (float)source[sample_index] -
                  (float)source[neighbor_index]);

              if (delta >= edge_threshold_u16)
              {
                near_hard_edge = true;
                break;
              }
            }

            if (near_hard_edge)
            {
              break;
            }
          }
        }

        if (!near_hard_edge)
        {
          target[index] = source[index];
          continue;
        }

        /*
         * Average across the whole transition radius. Unlike the previous
         * biome-only smoothing this works at land/shore/shallow borders too,
         * turning a discontinuity into a multi-cell slope.
         */
        for (i32 oy = -radius; oy <= radius; ++oy)
        {
          i32 sy = (i32)y + oy;

          if (sy < 0 || sy >= (i32)system->map_size)
          {
            continue;
          }

          for (i32 ox = -radius; ox <= radius; ++ox)
          {
            i32 sx = (i32)x + ox;
            i32 distance2 = ox * ox + oy * oy;
            float distance;
            float weight;
            u64 sample_index;

            if (sx < 0 || sx >= (i32)system->map_size ||
                distance2 > radius * radius)
            {
              continue;
            }

            sample_index =
                (u64)sy * (u64)system->map_size + (u64)sx;

            /*
             * Do not let deep ocean dominate the shoreline slope. The deep
             * water floor and shallow ramp remain analytic; shore/land
             * heights can blend without importing the zero ocean floor.
             */
            if ((IslandTerrainClass)cells[index].terrain_class !=
                    ISLAND_TERRAIN_CLASS_DEEP_WATER &&
                (IslandTerrainClass)cells[sample_index].terrain_class ==
                    ISLAND_TERRAIN_CLASS_DEEP_WATER)
            {
              continue;
            }

            distance = sqrtf((float)distance2);
            weight = (float)(radius + 1) - distance;
            if (weight <= 0.0f)
            {
              continue;
            }

            weighted_sum += (double)source[sample_index] * (double)weight;
            weight_sum += (double)weight;
          }
        }

        if (weight_sum <= 0.0)
        {
          target[index] = source[index];
        }
        else
        {
          float current = (float)source[index];
          float blurred = (float)(weighted_sum / weight_sum);
          float smoothed = s_lerp_f32(current, blurred, strength);

          if (smoothed < 0.0f)
          {
            smoothed = 0.0f;
          }
          else if (smoothed > 65535.0f)
          {
            smoothed = 65535.0f;
          }

          target[index] = (u16)(smoothed + 0.5f);
        }
      }
    }

    {
      u16 *swap = source;
      source = target;
      target = swap;
    }
  }

  for (u64 i = 0u; i < cell_count; ++i)
  {
    map_data[i * ISLAND_MAP_CHANNEL_COUNT + ISLAND_MAP_CHANNEL_HEIGHT] =
        source[i];
  }

  free(source);
  free(target);
  return true;
}

/* Distances here use heightmap cells, matching the island's generation scale. */
static float s_island_shallow_patch_weight(
    const IslandTerrain *system, u32 x, u32 y)
{
  float spacing = system->shallow_patch_spacing;
  float weight = 0.0f;
  i32 grid_x;
  i32 grid_y;

  if (spacing <= 0.0f || system->shallow_patch_chance <= 0.0f)
  {
    return 0.0f;
  }

  grid_x = (i32)floorf((float)x / spacing);
  grid_y = (i32)floorf((float)y / spacing);
  for (i32 oy = -1; oy <= 1; ++oy)
  {
    for (i32 ox = -1; ox <= 1; ++ox)
    {
      i32 gx = grid_x + ox;
      i32 gy = grid_y + oy;
      float center_x;
      float center_y;
      float radius;
      float dx;
      float dy;
      float distance;
      float angle;
      float nx;
      float ny;
      float land_noise;
      float land;
      float terrain;
      float t;

      if (s_hash_2d_rand01(gx, gy, system->seed ^ 0x68E31DA4u) >=
          system->shallow_patch_chance)
      {
        continue;
      }
      center_x = ((float)gx + 0.25f + 0.5f *
          s_hash_2d_rand01(gx, gy, system->seed ^ 0x1B56C4E9u)) * spacing;
      center_y = ((float)gy + 0.25f + 0.5f *
          s_hash_2d_rand01(gx, gy, system->seed ^ 0x9E3779B9u)) * spacing;
      radius = s_lerp_f32(system->shallow_patch_min_radius,
          system->shallow_patch_max_radius,
          s_hash_2d_rand01(gx, gy, system->seed ^ 0xA511E9B3u));
      dx = (float)x - center_x;
      dy = (float)y - center_y;
      distance = sqrtf(dx * dx + dy * dy);
      if (distance >= radius * 1.2f + system->shallow_patch_bank_width)
      {
        continue;
      }

      /* Only place basins well inland and away from mountain centres. */
      nx = center_x / (float)system->map_size * 2.0f - 1.0f;
      ny = center_y / (float)system->map_size * 2.0f - 1.0f;
      land_noise = s_fbm_compose_2d(center_x * system->land_scale,
          center_y * system->land_scale, system->seed, system->land_octaves);
      land = 1.0f - sqrtf(nx * nx + ny * ny) / system->island_radius +
          (land_noise - 0.5f) * system->land_noise_strength;
      terrain = s_fbm_compose_2d(center_x * system->terrain_scale,
          center_y * system->terrain_scale, system->seed,
          system->terrain_octaves);
      if (land < system->shore_max +
              4.0f * (radius + system->shallow_patch_bank_width) /
                  ((float)system->map_size * system->island_radius) ||
          terrain >= system->mountain_min)
      {
        continue;
      }

      /* Break up the circular contour without changing the flat basin floor. */
      angle = atan2f(dy, dx);
      radius *= 1.0f + 0.12f * sinf(angle * 3.0f + (float)gx) +
          0.08f * cosf(angle * 5.0f + (float)gy);
      t = (distance - radius) / system->shallow_patch_bank_width;
      t = fminf(1.0f, fmaxf(0.0f, t));
      weight = fmaxf(weight, 1.0f - s_smoothstep_f32(t));
    }
  }
  return weight;
}

static bool s_island_map_generate(const IslandTerrain *system)
{
  IslandMapCell *cells;
  u16 *map_data;
  IslandMapPropRule decorations[ISLAND_DECORATION_RULE_COUNT];
  IslandMapPropRule resources[ISLAND_RESOURCE_RULE_COUNT];
  u64 cell_count;
  u64 value_count;
  float *terrain_values;
  float biome_terrain_min[ISLAND_BIOME_COUNT];
  float biome_terrain_max[ISLAND_BIOME_COUNT];

  if (!system)
  {
    return false;
  }

  cell_count = (u64)system->map_size * (u64)system->map_size;
  value_count = cell_count * ISLAND_MAP_CHANNEL_COUNT;
  if (cell_count == 0u || cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(*cells)) ||
      value_count > (u64)(SIZE_MAX / sizeof(*map_data)))
  {
    return false;
  }

  decorations[0] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_FOREST, 0x71f34a9bu,
      system->forest_decoration_chance, 1u};
  decorations[1] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_GRASS, 0x71f34a9bu,
      system->grass_decoration_0_chance, 2u};
  decorations[2] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_GRASS, 0x31aa88c7u,
      system->grass_decoration_1_chance, 3u};
  decorations[3] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_MOUNTAIN, 0x9d872b41u,
      system->mountain_decoration_chance, 4u};
  decorations[4] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_DRY, 0x9d872b41u, system->dry_decoration_chance, 5u};
  decorations[5] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_SHORE,
      ISLAND_BIOME_GRASS, 0x11112222u,
      system->shore_decoration_chance, 6u};
  decorations[6] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_SHALLOW_WATER,
      ISLAND_BIOME_GRASS, 0x33334444u,
      system->shallow_water_decoration_chance, 7u};

  resources[0] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_MOUNTAIN, 0x44cc8821u,
      system->mountain_resource_chance, 1u};
  resources[1] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_FOREST, 0x55dd9932u,
      system->forest_resource_chance, 2u};
  resources[2] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_GRASS, 0x66eeaa43u,
      system->grass_resource_chance, 3u};
  resources[3] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_LAND,
      ISLAND_BIOME_DRY, 0x77ffbb54u, system->dry_resource_chance, 4u};
  resources[4] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_SHORE,
      ISLAND_BIOME_GRASS, 0x88aacc66u,
      system->shore_resource_chance, 5u};
  resources[5] = (IslandMapPropRule){ISLAND_TERRAIN_CLASS_SHALLOW_WATER,
      ISLAND_BIOME_GRASS, 0x99bbdd77u,
      system->shallow_water_resource_chance, 6u};

  cells = (IslandMapCell *)malloc((size_t)cell_count * sizeof(*cells));
  map_data = (u16 *)calloc((size_t)value_count, sizeof(*map_data));
  terrain_values = (float *)calloc((size_t)cell_count, sizeof(*terrain_values));
  if (!cells || !map_data || !terrain_values)
  {
    free(cells);
    free(map_data);
    free(terrain_values);
    return false;
  }

  for (u32 i = 0u; i < (u32)ISLAND_BIOME_COUNT; ++i)
  {
    biome_terrain_min[i] = INFINITY;
    biome_terrain_max[i] = -INFINITY;
  }

  /*
   * First pass: generate/classify the island and record the actual terrain fBm
   * range present in each biome. The old encoder assumed that fBm naturally
   * covered 0..1, but in practice it occupies a much narrower middle range,
   * which is why the heightmap looked almost flat.
   */
  for (u32 y = 0u; y < system->map_size; ++y)
  {
    for (u32 x = 0u; x < system->map_size; ++x)
    {
      float land_noise = s_fbm_compose_2d((float)x * system->land_scale,
          (float)y * system->land_scale, system->seed,
          system->land_octaves);
      float nx = ((float)x / (float)system->map_size) * 2.0f - 1.0f;
      float ny = ((float)y / (float)system->map_size) * 2.0f - 1.0f;
      float dist = sqrtf(nx * nx + ny * ny) / system->island_radius;
      float island_mask = 1.0f - dist;
      float land = island_mask +
                   (land_noise - 0.5f) * system->land_noise_strength;
      float terrain = 0.0f;
      IslandTerrainClass terrain_class =
          s_island_terrain_class_select(system, land);
      IslandBiome biome = ISLAND_BIOME_GRASS;
      u64 cell_index = (u64)y * (u64)system->map_size + (u64)x;
      IslandMapCell *cell = &cells[cell_index];
      if (terrain_class == ISLAND_TERRAIN_CLASS_LAND)
      {
        terrain = s_fbm_compose_2d(
            (float)x * system->terrain_scale,
            (float)y * system->terrain_scale, system->seed,
            system->terrain_octaves);
        float moisture = s_fbm_compose_2d(
            (float)x * system->moisture_scale,
            (float)y * system->moisture_scale, system->seed,
            system->moisture_octaves);

        float thorns = s_fbm_compose_2d(
            (float)x * system->thorn_noise_scale,
            (float)y * system->thorn_noise_scale,
            system->seed ^ 0xB5297A4Du, system->moisture_octaves);
        biome = s_island_biome_select(system, terrain, moisture, thorns);

        terrain_values[cell_index] = terrain;
        if (terrain < biome_terrain_min[(u32)biome])
        {
          biome_terrain_min[(u32)biome] = terrain;
        }
        if (terrain > biome_terrain_max[(u32)biome])
        {
          biome_terrain_max[(u32)biome] = terrain;
        }
      }

      cell->terrain_class = (u8)terrain_class;
      cell->biome = (u8)biome;
      cell->decoration_rule = 0u;
      cell->resource_rule = 0u;
    }
  }

  /*
   * Second pass: now that we know the range actually generated for every
   * biome, stretch that range across the biome's full height interval.
   */
  for (u32 y = 0u; y < system->map_size; ++y)
  {
    for (u32 x = 0u; x < system->map_size; ++x)
    {
      u64 cell_index = (u64)y * (u64)system->map_size + (u64)x;
      IslandMapCell *cell = &cells[cell_index];
      IslandTerrainClass terrain_class =
          (IslandTerrainClass)cell->terrain_class;
      IslandBiome biome = (IslandBiome)cell->biome;
      float land_noise = s_fbm_compose_2d(
          (float)x * system->land_scale,
          (float)y * system->land_scale,
          system->seed, system->land_octaves);
      float nx = ((float)x / (float)system->map_size) * 2.0f - 1.0f;
      float ny = ((float)y / (float)system->map_size) * 2.0f - 1.0f;
      float land = 1.0f - sqrtf(nx * nx + ny * ny) / system->island_radius +
          (land_noise - 0.5f) * system->land_noise_strength;
      float terrain = terrain_values[cell_index];
      float surface_noise = s_fbm_compose_2d(
          (float)x * system->surface_noise_scale,
          (float)y * system->surface_noise_scale,
          system->seed ^ 0xA511E9B3u,
          system->surface_noise_octaves);
      u16 *texel = &map_data[cell_index * ISLAND_MAP_CHANNEL_COUNT];

      texel[ISLAND_MAP_CHANNEL_HEIGHT] = s_island_map_height_encode(
          system, terrain_class, biome, land, terrain, surface_noise,
          biome_terrain_min, biome_terrain_max);
    }
  }

#ifdef LDK_SHAREDLIB
  ldk_log_info(
      "Island terrain fBm ranges: dry=[%.4f, %.4f] grass=[%.4f, %.4f] "
      "forest=[%.4f, %.4f] thorns=[%.4f, %.4f] mountain=[%.4f, %.4f].\n",
      biome_terrain_min[ISLAND_BIOME_DRY],
      biome_terrain_max[ISLAND_BIOME_DRY],
      biome_terrain_min[ISLAND_BIOME_GRASS],
      biome_terrain_max[ISLAND_BIOME_GRASS],
      biome_terrain_min[ISLAND_BIOME_FOREST],
      biome_terrain_max[ISLAND_BIOME_FOREST],
      biome_terrain_min[ISLAND_BIOME_THORNS],
      biome_terrain_max[ISLAND_BIOME_THORNS],
      biome_terrain_min[ISLAND_BIOME_MOUNTAIN],
      biome_terrain_max[ISLAND_BIOME_MOUNTAIN]);
#endif

  free(terrain_values);
  terrain_values = NULL;

  if (!s_island_map_height_smooth_land(system, cells, map_data))
  {
    free(cells);
    free(map_data);
    free(terrain_values);
    return false;
  }

  for (u32 y = 0u; y < system->map_size; ++y)
  {
    for (u32 x = 0u; x < system->map_size; ++x)
    {
      u64 index = (u64)y * (u64)system->map_size + (u64)x;
      IslandMapCell *cell = &cells[index];
      u16 *texel = &map_data[index * ISLAND_MAP_CHANNEL_COUNT];
      float nx = (float)x / (float)system->map_size * 2.0f - 1.0f;
      float ny = (float)y / (float)system->map_size * 2.0f - 1.0f;
      float dist = sqrtf(nx * nx + ny * ny) / system->island_radius;

      if ((IslandTerrainClass)cell->terrain_class == ISLAND_TERRAIN_CLASS_LAND)
      {
        float weight = s_island_shallow_patch_weight(system, x, y);
        if (weight > 0.0f)
        {
          float height = s_lerp_f32((float)texel[ISLAND_MAP_CHANNEL_HEIGHT],
              system->shallow_water_height * 65535.0f, weight);
          texel[ISLAND_MAP_CHANNEL_HEIGHT] = (u16)(height + 0.5f);
          if (weight >= 1.0f)
          {
            cell->terrain_class = ISLAND_TERRAIN_CLASS_SHALLOW_WATER;
          }
          else if (height < system->dry_height * 65535.0f)
          {
            cell->terrain_class = ISLAND_TERRAIN_CLASS_SHORE;
          }
        }
      }

      /* Reclassify props as well: basins use the existing shallow-water rules. */
      u16 decoration = s_island_map_prop_select(x, y, dist,
          (IslandTerrainClass)cell->terrain_class, (IslandBiome)cell->biome,
          system->seed, decorations, (u32)ARRAY_COUNT(decorations), false);
      u16 resource = s_island_map_prop_select(x, y, dist,
          (IslandTerrainClass)cell->terrain_class, (IslandBiome)cell->biome,
          system->seed, resources, (u32)ARRAY_COUNT(resources), true);
      cell->decoration_rule = s_island_map_prop_type(decoration);
      cell->resource_rule = s_island_map_prop_type(resource);
      texel[ISLAND_MAP_CHANNEL_DECORATION] = decoration;
      texel[ISLAND_MAP_CHANNEL_RESOURCE] = resource;
    }
  }

  s_island_map_take(cells, map_data, system->map_size, system->map_size);
  return true;
}


bool island_terrain_map_data_get(IslandMapDataView *out_view)
{
  if (!out_view || !s_map.data || s_map.width == 0u || s_map.height == 0u)
  {
    return false;
  }
  out_view->texels = s_map.data;
  out_view->width = s_map.width;
  out_view->height = s_map.height;
  return true;
}

static IslandTerrainSurface s_island_map_cell_surface(
    const IslandMapCell *cell)
{
  if (!cell)
  {
    return ISLAND_TERRAIN_SURFACE_GRASS;
  }

  switch ((IslandTerrainClass)cell->terrain_class)
  {
  case ISLAND_TERRAIN_CLASS_DEEP_WATER:
    return ISLAND_TERRAIN_SURFACE_DEEP_WATER;
  case ISLAND_TERRAIN_CLASS_SHALLOW_WATER:
    return ISLAND_TERRAIN_SURFACE_SHALLOW_WATER;
  case ISLAND_TERRAIN_CLASS_SHORE:
    return ISLAND_TERRAIN_SURFACE_SAND;
  case ISLAND_TERRAIN_CLASS_LAND:
    break;
  default:
    return ISLAND_TERRAIN_SURFACE_GRASS;
  }

  switch ((IslandBiome)cell->biome)
  {
  case ISLAND_BIOME_MOUNTAIN:
    return ISLAND_TERRAIN_SURFACE_ROCK;
  case ISLAND_BIOME_DRY:
  case ISLAND_BIOME_THORNS:
    return ISLAND_TERRAIN_SURFACE_GRASS_DARK;
  case ISLAND_BIOME_GRASS:
  case ISLAND_BIOME_FOREST:
  default:
    return ISLAND_TERRAIN_SURFACE_GRASS;
  }
}

static u32 s_island_terrain_surface_color(IslandTerrainSurface surface)
{
  switch (surface)
  {
  case ISLAND_TERRAIN_SURFACE_DEEP_WATER:
    return ISLAND_TERRAIN_COLOR_DEEP_WATER;
  case ISLAND_TERRAIN_SURFACE_SHALLOW_WATER:
    return ISLAND_TERRAIN_COLOR_SHALLOW_WATER;
  case ISLAND_TERRAIN_SURFACE_SAND:
    return ISLAND_TERRAIN_COLOR_SAND;
  case ISLAND_TERRAIN_SURFACE_GRASS_DARK:
    return ISLAND_TERRAIN_COLOR_GRASS_DARK;
  case ISLAND_TERRAIN_SURFACE_ROCK:
    return ISLAND_TERRAIN_COLOR_ROCK;
  case ISLAND_TERRAIN_SURFACE_GRASS:
  default:
    return ISLAND_TERRAIN_COLOR_GRASS;
  }
}

static const LDKEditorIcon s_island_terrain_atlas_icons
    [ISLAND_TERRAIN_SURFACE_COUNT][ISLAND_TERRAIN_CUTOUT_MASK_COUNT] =
{
  {
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
    TFTF_ICON_DEEP_WATER, TFTF_ICON_DEEP_WATER,
  },
  {
    TFTF_ICON_SHALLOW_WATER,
    TFTF_ICON_SHALLOW_WATER_CUT_N,
    TFTF_ICON_SHALLOW_WATER_CUT_E,
    TFTF_ICON_SHALLOW_WATER_CUT_NE,
    TFTF_ICON_SHALLOW_WATER_CUT_S,
    TFTF_ICON_SHALLOW_WATER_CUT_NS,
    TFTF_ICON_SHALLOW_WATER_CUT_ES,
    TFTF_ICON_SHALLOW_WATER_CUT_NES,
    TFTF_ICON_SHALLOW_WATER_CUT_W,
    TFTF_ICON_SHALLOW_WATER_CUT_NW,
    TFTF_ICON_SHALLOW_WATER_CUT_EW,
    TFTF_ICON_SHALLOW_WATER_CUT_NEW,
    TFTF_ICON_SHALLOW_WATER_CUT_SW,
    TFTF_ICON_SHALLOW_WATER_CUT_NSW,
    TFTF_ICON_SHALLOW_WATER_CUT_ESW,
    TFTF_ICON_SHALLOW_WATER_CUT_NESW,
  },
  {
    TFTF_ICON_SAND,
    TFTF_ICON_SAND_CUT_N,
    TFTF_ICON_SAND_CUT_E,
    TFTF_ICON_SAND_CUT_NE,
    TFTF_ICON_SAND_CUT_S,
    TFTF_ICON_SAND_CUT_NS,
    TFTF_ICON_SAND_CUT_ES,
    TFTF_ICON_SAND_CUT_NES,
    TFTF_ICON_SAND_CUT_W,
    TFTF_ICON_SAND_CUT_NW,
    TFTF_ICON_SAND_CUT_EW,
    TFTF_ICON_SAND_CUT_NEW,
    TFTF_ICON_SAND_CUT_SW,
    TFTF_ICON_SAND_CUT_NSW,
    TFTF_ICON_SAND_CUT_ESW,
    TFTF_ICON_SAND_CUT_NESW,
  },
  {
    TFTF_ICON_GRASS_DARK,
    TFTF_ICON_GRASS_DARK_CUT_N,
    TFTF_ICON_GRASS_DARK_CUT_E,
    TFTF_ICON_GRASS_DARK_CUT_NE,
    TFTF_ICON_GRASS_DARK_CUT_S,
    TFTF_ICON_GRASS_DARK_CUT_NS,
    TFTF_ICON_GRASS_DARK_CUT_ES,
    TFTF_ICON_GRASS_DARK_CUT_NES,
    TFTF_ICON_GRASS_DARK_CUT_W,
    TFTF_ICON_GRASS_DARK_CUT_NW,
    TFTF_ICON_GRASS_DARK_CUT_EW,
    TFTF_ICON_GRASS_DARK_CUT_NEW,
    TFTF_ICON_GRASS_DARK_CUT_SW,
    TFTF_ICON_GRASS_DARK_CUT_NSW,
    TFTF_ICON_GRASS_DARK_CUT_ESW,
    TFTF_ICON_GRASS_DARK_CUT_NESW,
  },
  {
    TFTF_ICON_GRASS,
    TFTF_ICON_GRASS_CUT_N,
    TFTF_ICON_GRASS_CUT_E,
    TFTF_ICON_GRASS_CUT_NE,
    TFTF_ICON_GRASS_CUT_S,
    TFTF_ICON_GRASS_CUT_NS,
    TFTF_ICON_GRASS_CUT_ES,
    TFTF_ICON_GRASS_CUT_NES,
    TFTF_ICON_GRASS_CUT_W,
    TFTF_ICON_GRASS_CUT_NW,
    TFTF_ICON_GRASS_CUT_EW,
    TFTF_ICON_GRASS_CUT_NEW,
    TFTF_ICON_GRASS_CUT_SW,
    TFTF_ICON_GRASS_CUT_NSW,
    TFTF_ICON_GRASS_CUT_ESW,
    TFTF_ICON_GRASS_CUT_NESW,
  },
  {
    TFTF_ICON_ROCK,
    TFTF_ICON_ROCK_CUT_N,
    TFTF_ICON_ROCK_CUT_E,
    TFTF_ICON_ROCK_CUT_NE,
    TFTF_ICON_ROCK_CUT_S,
    TFTF_ICON_ROCK_CUT_NS,
    TFTF_ICON_ROCK_CUT_ES,
    TFTF_ICON_ROCK_CUT_NES,
    TFTF_ICON_ROCK_CUT_W,
    TFTF_ICON_ROCK_CUT_NW,
    TFTF_ICON_ROCK_CUT_EW,
    TFTF_ICON_ROCK_CUT_NEW,
    TFTF_ICON_ROCK_CUT_SW,
    TFTF_ICON_ROCK_CUT_NSW,
    TFTF_ICON_ROCK_CUT_ESW,
    TFTF_ICON_ROCK_CUT_NESW,
  },
};

static LDKEditorIcon s_island_terrain_atlas_icon(
    IslandTerrainSurface surface, u32 cutout_mask)
{
  if ((u32)surface >= (u32)ISLAND_TERRAIN_SURFACE_COUNT ||
      cutout_mask >= ISLAND_TERRAIN_CUTOUT_MASK_COUNT)
  {
    return TFTF_ICON_GRASS;
  }

  return s_island_terrain_atlas_icons[(u32)surface][cutout_mask];
}

static IslandTerrainUVRect s_island_terrain_atlas_uv(
    IslandTerrainSurface surface, u32 cutout_mask)
{
  const LDKRectf rect =
      tftf_icon_rects[s_island_terrain_atlas_icon(surface, cutout_mask)];
  const float half_texel_u = 0.5f / (float)TFTF_ICON_ATLAS_WIDTH;
  const float half_texel_v = 0.5f / (float)TFTF_ICON_ATLAS_HEIGHT;
  IslandTerrainUVRect uv;

  uv.u0 = rect.x + half_texel_u;
  uv.v0 = rect.y + half_texel_v;
  uv.u1 = rect.x + rect.w - half_texel_u;
  uv.v1 = rect.y + rect.h - half_texel_v;
  return uv;
}

static IslandTerrainSurface s_island_terrain_surface_at(i32 x, i32 y)
{
  if (!s_map.cells || x < 0 || y < 0 || x >= (i32)s_map.width ||
      y >= (i32)s_map.height)
  {
    return ISLAND_TERRAIN_SURFACE_DEEP_WATER;
  }

  return s_island_map_cell_surface(
      &s_map.cells[(u32)y * s_map.width + (u32)x]);
}

static u32 s_island_terrain_cutout_mask(
    IslandTerrainSurface surface, i32 x, i32 y)
{
  u32 mask = 0u;

  if (s_island_terrain_surface_at(x, y - 1) < surface)
  {
    mask |= ISLAND_TERRAIN_CUTOUT_NORTH;
  }
  if (s_island_terrain_surface_at(x + 1, y) < surface)
  {
    mask |= ISLAND_TERRAIN_CUTOUT_EAST;
  }
  if (s_island_terrain_surface_at(x, y + 1) < surface)
  {
    mask |= ISLAND_TERRAIN_CUTOUT_SOUTH;
  }
  if (s_island_terrain_surface_at(x - 1, y) < surface)
  {
    mask |= ISLAND_TERRAIN_CUTOUT_WEST;
  }

  return mask;
}

static u32 s_island_terrain_layers_at(i32 x, i32 y)
{
  const IslandTerrainSurface top = s_island_terrain_surface_at(x, y);
  const IslandTerrainSurface neighbors[4] = {
      s_island_terrain_surface_at(x, y - 1),
      s_island_terrain_surface_at(x + 1, y),
      s_island_terrain_surface_at(x, y + 1),
      s_island_terrain_surface_at(x - 1, y),
  };
  u32 layers = 1u << (u32)top;

  for (u32 i = 0u; i < ARRAY_COUNT(neighbors); ++i)
  {
    if (neighbors[i] < top)
    {
      layers |= 1u << (u32)neighbors[i];
    }
  }

  return layers;
}

static LDKGrassTypeId s_island_terrain_grass_type_id(
    IslandBiome biome)
{
  switch (biome)
  {
  case ISLAND_BIOME_DRY:
    return s_runtime.dry_grass_type_id;
  case ISLAND_BIOME_FOREST:
    return s_runtime.forest_grass_type_id;
  case ISLAND_BIOME_THORNS:
    return s_runtime.thorn_grass_type_id;
  case ISLAND_BIOME_GRASS:
  default:
    return s_runtime.grass_type_id;
  }
}

static bool s_island_terrain_grass_types_resolve(
    const IslandTerrain *system, bool *out_changed)
{
  LDKGrassTypeId dry_id;
  LDKGrassTypeId grass_id;
  LDKGrassTypeId forest_id;
  LDKGrassTypeId thorn_id;
  LDKGrassTypeId shallow_id;
  bool changed;

  if (!system)
  {
    return false;
  }

  dry_id = ldk_grass_get_id_by_name(
      x_smallstr_cstr(&system->dry_grass_type_name));
  grass_id = ldk_grass_get_id_by_name(
      x_smallstr_cstr(&system->grass_type_name));
  forest_id = ldk_grass_get_id_by_name(
      x_smallstr_cstr(&system->forest_grass_type_name));
  thorn_id = ldk_grass_get_id_by_name(
      x_smallstr_cstr(&system->thorn_grass_type_name));
  shallow_id = ldk_grass_get_id_by_name(
      x_smallstr_cstr(&system->shallow_grass_type_name));

  changed = dry_id != s_runtime.dry_grass_type_id ||
      grass_id != s_runtime.grass_type_id ||
      forest_id != s_runtime.forest_grass_type_id ||
      thorn_id != s_runtime.thorn_grass_type_id ||
      shallow_id != s_runtime.shallow_grass_type_id;
  s_runtime.dry_grass_type_id = dry_id;
  s_runtime.grass_type_id = grass_id;
  s_runtime.forest_grass_type_id = forest_id;
  s_runtime.thorn_grass_type_id = thorn_id;
  s_runtime.shallow_grass_type_id = shallow_id;

  if (out_changed)
  {
    *out_changed = changed;
  }
  return true;
}

static bool s_island_terrain_tile_grass_desc(IslandTerrain *system,
    i32 tile_x, i32 tile_y, LDKGrassTypeId *out_type_id,
    LDKGrassPatchDesc *out_desc)
{
  const IslandMapCell *cell;
  LDKGrassTypeDesc type;
  LDKGrassTypeId type_id;
  IslandTerrainSurface surface;
  float origin_x;
  float origin_z;
  float layer_elevation;

  if (!system || !out_type_id || !out_desc || !s_map.cells ||
      !s_runtime.grass_density_scales || tile_x < 0 || tile_y < 0 ||
      tile_x >= (i32)s_map.width || tile_y >= (i32)s_map.height)
  {
    return false;
  }

  cell = &s_map.cells[(u32)tile_y * s_map.width + (u32)tile_x];
  bool shallow = (IslandTerrainClass)cell->terrain_class ==
      ISLAND_TERRAIN_CLASS_SHALLOW_WATER;
  if (!shallow &&
      ((IslandTerrainClass)cell->terrain_class != ISLAND_TERRAIN_CLASS_LAND ||
          (IslandBiome)cell->biome == ISLAND_BIOME_MOUNTAIN))
  {
    return false;
  }

  type_id = shallow ? s_runtime.shallow_grass_type_id :
      s_island_terrain_grass_type_id((IslandBiome)cell->biome);
  if (!ldk_grass_type_get(type_id, &type) || type.density == 0.0f)
  {
    return false;
  }

  ldk_grass_patch_desc_defaults(out_desc);
  out_desc->density_scale =
      s_runtime.grass_density_scales[(u32)tile_y * s_map.width +
          (u32)tile_x];
  if (out_desc->density_scale == 0.0f)
  {
    return false;
  }

  surface = s_island_map_cell_surface(cell);
  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;
  layer_elevation = system->elevation +
      (float)surface * system->cell_size *
          ISLAND_TERRAIN_LAYER_ELEVATION_STEP;

  float x0 = origin_x + (float)tile_x * system->cell_size;
  float x1 = x0 + system->cell_size;
  float z0 = origin_z + (float)tile_y * system->cell_size;
  float z1 = z0 + system->cell_size;
  float h00;
  float h10;
  float h01;

  if (ldk_terrain_system_is_active())
  {
    if (!ldk_terrain_height_at_world(x0, z1, &h00) ||
        !ldk_terrain_height_at_world(x1, z1, &h10) ||
        !ldk_terrain_height_at_world(x0, z0, &h01))
    {
      /*
       * TerrainSystem may already be initialized while its procedural image
       * has not yet been consumed by the first render update. Do not place
       * grass at the obsolete flat IslandTerrain elevation: that puts it
       * underneath the heightmap terrain. Leave the patch absent and retry.
       */
      s_runtime.grass_height_sync_pending = true;
      return false;
    }

    out_desc->plane.origin = vec3_make(x0, h00, z1);
    out_desc->plane.axis_u =
        vec3_make(system->cell_size, h10 - h00, 0.0f);
    out_desc->plane.axis_v =
        vec3_make(0.0f, h01 - h00, -system->cell_size);
  }
  else
  {
    /* Legacy IslandTerrain renderer fallback. */
    out_desc->plane.origin = vec3_make(x0, layer_elevation, z1);
    out_desc->plane.axis_u =
        vec3_make(system->cell_size, 0.0f, 0.0f);
    out_desc->plane.axis_v =
        vec3_make(0.0f, 0.0f, -system->cell_size);
  }
  out_desc->seed = s_hash_2d_i32(
      tile_x, tile_y, system->seed ^ 0x7a11c9e3u);
  *out_type_id = type_id;
  return true;
}

static bool s_island_terrain_world_to_cell(
    Vec3 world_position, i32 *out_x, i32 *out_y)
{
  IslandTerrain *system = s_runtime.owner;
  float origin_x;
  float origin_z;
  i32 x;
  i32 y;

  if (!system || !out_x || !out_y || !s_map.cells ||
      !isfinite(world_position.x) ||
      !isfinite(world_position.z) || !isfinite(system->cell_size) ||
      system->cell_size <= 0.0f)
  {
    return false;
  }

  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;
  x = (i32)floorf((world_position.x - origin_x) / system->cell_size);
  y = (i32)floorf((world_position.z - origin_z) / system->cell_size);
  if (x < 0 || y < 0 || x >= (i32)s_map.width ||
      y >= (i32)s_map.height)
  {
    return false;
  }

  *out_x = x;
  *out_y = y;
  return true;
}

bool tftf_island_terrain_tile_kind_at(
    Vec3 world_position, TFTFIslandTileKind *out_kind)
{
  i32 cell_x;
  i32 cell_y;
  const IslandMapCell *cell;

  if (!out_kind ||
      !s_island_terrain_world_to_cell(world_position, &cell_x, &cell_y))
  {
    return false;
  }

  cell = &s_map.cells[(u32)cell_y * s_map.width + (u32)cell_x];
  switch ((IslandTerrainClass)cell->terrain_class)
  {
  case ISLAND_TERRAIN_CLASS_DEEP_WATER:
    *out_kind = TFTF_ISLAND_TILE_DEEP_WATER;
    return true;
  case ISLAND_TERRAIN_CLASS_SHALLOW_WATER:
    *out_kind = TFTF_ISLAND_TILE_SHALLOW_WATER;
    return true;
  case ISLAND_TERRAIN_CLASS_SHORE:
    *out_kind = TFTF_ISLAND_TILE_SHORE;
    return true;
  case ISLAND_TERRAIN_CLASS_LAND:
    break;
  default:
    return false;
  }

  switch ((IslandBiome)cell->biome)
  {
  case ISLAND_BIOME_DRY:
    *out_kind = TFTF_ISLAND_TILE_DRY;
    break;
  case ISLAND_BIOME_GRASS:
    *out_kind = TFTF_ISLAND_TILE_GRASS;
    break;
  case ISLAND_BIOME_FOREST:
    *out_kind = TFTF_ISLAND_TILE_FOREST;
    break;
  case ISLAND_BIOME_MOUNTAIN:
    *out_kind = TFTF_ISLAND_TILE_MOUNTAIN;
    break;
  default:
    return false;
  }
  return true;
}

bool island_terrain_grass_state_at(Vec3 world_position,
    float *out_density, float *out_min_height)
{
  LDKGrassPatchDesc patch;
  LDKGrassTypeDesc type;
  LDKGrassTypeId type_id;
  i32 x;
  i32 y;

  if (!out_density || !out_min_height ||
      !s_island_terrain_grass_types_resolve(s_runtime.owner, NULL) ||
      !s_island_terrain_world_to_cell(world_position, &x, &y) ||
      !s_island_terrain_tile_grass_desc(
          s_runtime.owner, x, y, &type_id, &patch) ||
      !ldk_grass_type_get(type_id, &type))
  {
    return false;
  }

  *out_density = type.density * patch.density_scale;
  *out_min_height = type.min_height;
  return true;
}

bool island_terrain_grass_density_multiply_at(
    Vec3 world_position, float multiplier)
{
  IslandTerrain *system = s_runtime.owner;
  u32 scale_index;
  float previous_scale;
  i32 x;
  i32 y;

  if (!isfinite(multiplier) || multiplier < 0.0f || multiplier > 1.0f ||
      !s_island_terrain_world_to_cell(world_position, &x, &y))
  {
    return false;
  }

  scale_index = (u32)y * s_map.width + (u32)x;
  previous_scale = s_runtime.grass_density_scales[scale_index];
  s_runtime.grass_density_scales[scale_index] *= multiplier;

  for (u32 i = 0u; i < s_runtime.tile_count; ++i)
  {
    IslandTerrainTile *tile = &s_runtime.tiles[i];

    if (tile->x != x || tile->y != y)
    {
      continue;
    }
    if (s_island_terrain_tile_grass_update(system, tile))
    {
      return true;
    }

    s_runtime.grass_density_scales[scale_index] = previous_scale;
    (void)s_island_terrain_tile_grass_update(system, tile);
    return false;
  }

  return true;
}

static void s_island_terrain_tile_grass_remove(IslandTerrainTile *tile)
{
  if (!tile)
  {
    return;
  }

  if (ldk_grass_patch_is_valid(tile->grass))
  {
    (void)ldk_grass_remove(tile->grass);
  }
  tile->grass = ldk_grass_patch_null();
}

static bool s_island_terrain_tile_grass_update(
    IslandTerrain *system, IslandTerrainTile *tile)
{
  LDKGrassPatchDesc desc;
  LDKGrassTypeId type_id;

  if (!system || !tile)
  {
    return false;
  }

  if (!s_island_terrain_tile_grass_desc(
          system, tile->x, tile->y, &type_id, &desc))
  {
    s_island_terrain_tile_grass_remove(tile);
    return true;
  }

  if (ldk_grass_patch_is_valid(tile->grass))
  {
    return ldk_grass_update(tile->grass, type_id, &desc);
  }

  tile->grass = ldk_grass_add(type_id, &desc);
  return ldk_grass_patch_is_valid(tile->grass);
}

static bool s_island_terrain_grass_update_all(IslandTerrain *system)
{
  for (u32 i = 0u; i < s_runtime.tile_count; ++i)
  {
    if (!s_island_terrain_tile_grass_update(system, &s_runtime.tiles[i]))
    {
      return false;
    }
  }
  return true;
}

static IslandTerrainBounds s_island_terrain_bounds(
    i32 center_x, i32 center_y, u32 radius)
{
  IslandTerrainBounds bounds = {0};
  i64 min_x;
  i64 min_y;
  i64 max_x;
  i64 max_y;

  if (!s_map.cells || radius > INT32_MAX)
  {
    return bounds;
  }

  min_x = (i64)center_x - (i64)radius;
  min_y = (i64)center_y - (i64)radius;
  max_x = (i64)center_x + (i64)radius;
  max_y = (i64)center_y + (i64)radius;

  if (max_x < 0 || max_y < 0 || min_x >= (i64)s_map.width ||
      min_y >= (i64)s_map.height)
  {
    return bounds;
  }

  if (min_x < 0)
  {
    min_x = 0;
  }

  if (min_y < 0)
  {
    min_y = 0;
  }

  if (max_x >= (i64)s_map.width)
  {
    max_x = (i64)s_map.width - 1;
  }

  if (max_y >= (i64)s_map.height)
  {
    max_y = (i64)s_map.height - 1;
  }

  bounds.min_x = (i32)min_x;
  bounds.min_y = (i32)min_y;
  bounds.max_x = (i32)max_x;
  bounds.max_y = (i32)max_y;
  bounds.valid = true;
  return bounds;
}

static bool s_island_terrain_bounds_equal(
    const IslandTerrainBounds *a, const IslandTerrainBounds *b)
{
  if (!a || !b || a->valid != b->valid)
  {
    return false;
  }

  if (!a->valid)
  {
    return true;
  }

  return a->min_x == b->min_x && a->min_y == b->min_y &&
         a->max_x == b->max_x && a->max_y == b->max_y;
}

static bool s_island_terrain_bounds_contains(
    const IslandTerrainBounds *bounds, i32 x, i32 y)
{
  return bounds && bounds->valid && x >= bounds->min_x &&
         x <= bounds->max_x && y >= bounds->min_y && y <= bounds->max_y;
}

static bool s_island_terrain_bounds_cell_count(
    const IslandTerrainBounds *bounds, u32 *out_count)
{
  u64 columns;
  u64 rows;
  u64 count;

  if (!out_count)
  {
    return false;
  }

  *out_count = 0u;

  if (!bounds || !bounds->valid)
  {
    return true;
  }

  columns = (u64)((i64)bounds->max_x - (i64)bounds->min_x + 1);
  rows = (u64)((i64)bounds->max_y - (i64)bounds->min_y + 1);
  count = columns * rows;

  if (count > UINT32_MAX)
  {
    return false;
  }

  *out_count = (u32)count;
  return true;
}

static bool s_island_terrain_tiles_reserve(u32 required)
{
  IslandTerrainTile *tiles;
  u32 capacity;

  if (required <= s_runtime.tile_capacity)
  {
    return true;
  }

  capacity = s_runtime.tile_capacity ? s_runtime.tile_capacity : 64u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }

    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*tiles))
  {
    return false;
  }

  tiles = (IslandTerrainTile *)realloc(
      s_runtime.tiles, sizeof(*tiles) * (size_t)capacity);
  if (!tiles)
  {
    return false;
  }

  s_runtime.tiles = tiles;
  s_runtime.tile_capacity = capacity;
  return true;
}

static bool s_island_terrain_geometry_reserve(u32 tile_count)
{
  LDKMeshVertex *vertices;
  u32 *indices;
  u32 vertex_count;
  u32 index_count;

  if (tile_count > UINT32_MAX / 4u || tile_count > UINT32_MAX / 6u)
  {
    return false;
  }

  vertex_count = tile_count * 4u;
  index_count = tile_count * 6u;

  if (vertex_count > s_runtime.vertex_capacity)
  {
    if ((size_t)vertex_count > SIZE_MAX / sizeof(*vertices))
    {
      return false;
    }

    vertices = (LDKMeshVertex *)realloc(
        s_runtime.vertices, sizeof(*vertices) * (size_t)vertex_count);
    if (!vertices)
    {
      return false;
    }

    s_runtime.vertices = vertices;
    s_runtime.vertex_capacity = vertex_count;
  }

  if (index_count > s_runtime.index_capacity)
  {
    if ((size_t)index_count > SIZE_MAX / sizeof(*indices))
    {
      return false;
    }

    indices = (u32 *)realloc(
        s_runtime.indices, sizeof(*indices) * (size_t)index_count);
    if (!indices)
    {
      return false;
    }

    s_runtime.indices = indices;
    s_runtime.index_capacity = index_count;
  }

  return true;
}

static bool s_island_terrain_tiles_reset(
    IslandTerrain *system, const IslandTerrainBounds *bounds)
{
  for (u32 i = 0u; i < s_runtime.tile_count; ++i)
  {
    s_island_terrain_tile_grass_remove(&s_runtime.tiles[i]);
  }
  system->tiles_removed += s_runtime.tile_count;
  s_runtime.tile_count = 0u;

  if (!bounds->valid)
  {
    system->active_tile_count = 0u;
    return true;
  }

  for (i32 y = bounds->min_y; y <= bounds->max_y; ++y)
  {
    for (i32 x = bounds->min_x; x <= bounds->max_x; ++x)
    {
      IslandTerrainTile *tile = &s_runtime.tiles[s_runtime.tile_count++];

      tile->x = x;
      tile->y = y;
      tile->grass = ldk_grass_patch_null();
      if (!s_island_terrain_tile_grass_update(system, tile))
      {
        system->active_tile_count = s_runtime.tile_count;
        return false;
      }
      system->tiles_added += 1u;
    }
  }

  system->active_tile_count = s_runtime.tile_count;
  return true;
}

static void s_island_terrain_tiles_remove_outside(
    IslandTerrain *system, const IslandTerrainBounds *bounds)
{
  u32 write_index = 0u;

  for (u32 read_index = 0u; read_index < s_runtime.tile_count; ++read_index)
  {
    IslandTerrainTile tile = s_runtime.tiles[read_index];

    if (s_island_terrain_bounds_contains(bounds, tile.x, tile.y))
    {
      if (write_index != read_index)
      {
        s_runtime.tiles[write_index] = tile;
      }

      write_index += 1u;
    }
    else
    {
      s_island_terrain_tile_grass_remove(&tile);
      system->tiles_removed += 1u;
    }
  }

  s_runtime.tile_count = write_index;
  system->active_tile_count = write_index;
}

static bool s_island_terrain_tiles_add_difference(IslandTerrain *system,
    const IslandTerrainBounds *old_bounds,
    const IslandTerrainBounds *new_bounds)
{
  if (!new_bounds->valid)
  {
    return true;
  }

  for (i32 y = new_bounds->min_y; y <= new_bounds->max_y; ++y)
  {
    for (i32 x = new_bounds->min_x; x <= new_bounds->max_x; ++x)
    {
      IslandTerrainTile *tile;

      if (s_island_terrain_bounds_contains(old_bounds, x, y))
      {
        continue;
      }

      tile = &s_runtime.tiles[s_runtime.tile_count++];
      tile->x = x;
      tile->y = y;
      tile->grass = ldk_grass_patch_null();
      if (!s_island_terrain_tile_grass_update(system, tile))
      {
        system->active_tile_count = s_runtime.tile_count;
        return false;
      }
      system->tiles_added += 1u;
    }
  }

  system->active_tile_count = s_runtime.tile_count;
  return true;
}

static Vec3 s_island_terrain_flat_forward(Mat4 world)
{
  Vec3 forward = mat4_mul_dir(world, vec3_make(0.0f, 0.0f, -1.0f));
  float length_squared = forward.x * forward.x + forward.z * forward.z;

  if (!isfinite(length_squared) || length_squared <= 1e-8f)
  {
    return vec3_make(0.0f, 0.0f, 0.0f);
  }

  float inverse_length = 1.0f / sqrtf(length_squared);
  return vec3_make(
      forward.x * inverse_length, 0.0f, forward.z * inverse_length);
}

static void s_island_terrain_cache_update(
    IslandTerrain *system, i32 center_x, i32 center_y)
{
  system->center_x = center_x;
  system->center_y = center_y;
  system->cached_radius = system->radius;
  system->cached_cell_size = system->cell_size;
  system->cached_elevation = system->elevation;
  system->map_revision = s_map.revision;
}

static bool s_island_terrain_tiles_sync(
    IslandTerrain *system, i32 center_x, i32 center_y)
{
  IslandTerrainBounds old_bounds = {0};
  IslandTerrainBounds new_bounds;
  u32 new_tile_count;
  bool reset;
  bool membership_changed = false;
  bool elevation_changed;
  bool grass_types_changed = false;

  new_bounds = s_island_terrain_bounds(center_x, center_y, system->radius);

  if (!s_island_terrain_bounds_cell_count(&new_bounds, &new_tile_count) ||
      !s_island_terrain_tiles_reserve(new_tile_count) ||
      !s_island_terrain_grass_types_resolve(system, &grass_types_changed))
  {
    return false;
  }

  reset = system->center_x == INT32_MIN ||
          system->center_y == INT32_MIN ||
          system->cached_radius != system->radius ||
          system->cached_cell_size != system->cell_size ||
          system->map_revision != s_map.revision;

  elevation_changed = system->cached_elevation != system->elevation;

  if (reset)
  {
    if (!s_island_terrain_tiles_reset(system, &new_bounds))
    {
      system->center_x = INT32_MIN;
      return false;
    }
    membership_changed = true;
  }
  else if (system->center_x != center_x || system->center_y != center_y)
  {
    old_bounds = s_island_terrain_bounds(
        system->center_x, system->center_y, system->cached_radius);

    if (!s_island_terrain_bounds_equal(&old_bounds, &new_bounds))
    {
      s_island_terrain_tiles_remove_outside(system, &new_bounds);
      if (!s_island_terrain_tiles_add_difference(
              system, &old_bounds, &new_bounds))
      {
        system->center_x = INT32_MIN;
        return false;
      }
      membership_changed = true;
    }
  }

  if (s_runtime.tile_count != new_tile_count)
  {
    if (!s_island_terrain_tiles_reset(system, &new_bounds))
    {
      system->center_x = INT32_MIN;
      return false;
    }
    membership_changed = true;
  }

  if ((grass_types_changed || elevation_changed) && !reset &&
      !s_island_terrain_grass_update_all(system))
  {
    return false;
  }

  /*
   * IslandSystem publishes the procedural image during initialize, but the
   * TerrainSystem only makes it queryable after its first update. Retry the
   * grass patches until they can be fitted to the actual heightmap surface.
   */
  if (s_runtime.grass_height_sync_pending &&
      ldk_terrain_system_is_active())
  {
    s_runtime.grass_height_sync_pending = false;
    if (!s_island_terrain_grass_update_all(system))
    {
      return false;
    }
  }

  if (membership_changed || elevation_changed)
  {
    system->mesh_dirty = true;
  }

  s_island_terrain_cache_update(system, center_x, center_y);
  return true;
}

static bool s_island_terrain_material_asset_equal(
    LDKAssetMaterial a, LDKAssetMaterial b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static void s_island_terrain_material_release(LDKRenderer *renderer)
{
  if (!renderer)
  {
    return;
  }

  ldk_renderer_material_destroy(renderer, s_runtime.renderer_material);
  ldk_renderer_image_release(renderer, s_runtime.renderer_texture);
  ldk_renderer_image_release(renderer, s_runtime.renderer_normal_map);
  ldk_renderer_image_release(renderer, s_runtime.renderer_specular_map);
  s_runtime.material_asset = ldk_asset_material_null();
  s_runtime.material_revision = 0u;
  s_runtime.renderer_material = ldk_renderer_material_null();
  s_runtime.renderer_texture = ldk_renderer_texture_null();
  s_runtime.renderer_normal_map = ldk_renderer_texture_null();
  s_runtime.renderer_specular_map = ldk_renderer_texture_null();
}

static bool s_island_terrain_material_get(IslandTerrain *system,
    LDKRenderer *renderer, LDKAssetManager *assets,
    LDKResourceMaterial *out_material)
{
  const LDKAssetMaterialData *data;
  bool needs_resolve;

  if (!system || !renderer || !assets || !out_material)
  {
    return false;
  }

  data = ldk_asset_manager_material_get_const(assets, system->material);
  if (!data)
  {
    system->material = ldk_asset_material_null();
    s_island_terrain_material_release(renderer);
    *out_material = ldk_renderer_material_default_get(renderer);
    return ldk_renderer_material_is_valid(renderer, *out_material);
  }

  needs_resolve =
      !s_island_terrain_material_asset_equal(
          s_runtime.material_asset, system->material) ||
      s_runtime.material_revision != data->revision ||
      !ldk_renderer_material_is_valid(renderer, s_runtime.renderer_material);

  if (needs_resolve)
  {
    if (!ldk_renderer_material_resolve(renderer, assets, &data->descriptor,
            &s_runtime.renderer_material, &s_runtime.renderer_texture,
            &s_runtime.renderer_normal_map, &s_runtime.renderer_specular_map))
    {
      return false;
    }

    s_runtime.material_asset = system->material;
    s_runtime.material_revision = data->revision;
  }

  *out_material = s_runtime.renderer_material;
  return ldk_renderer_material_is_valid(renderer, *out_material);
}

static void s_island_terrain_quad_write(IslandTerrain *system,
    u32 quad_index, i32 tile_x, i32 tile_y, IslandTerrainSurface surface,
    u32 cutout_mask, float origin_x, float origin_z)
{
  IslandTerrainUVRect uv =
      s_island_terrain_atlas_uv(surface, cutout_mask);
  u32 color = s_island_terrain_surface_color(surface);
  u32 vertex = quad_index * 4u;
  u32 index = quad_index * 6u;
  float layer_elevation = system->elevation +
      (float)surface * system->cell_size *
          ISLAND_TERRAIN_LAYER_ELEVATION_STEP;
  float x0 = origin_x + (float)tile_x * system->cell_size;
  float x1 = x0 + system->cell_size;
  float z0 = origin_z + (float)tile_y * system->cell_size;
  float z1 = z0 + system->cell_size;

  s_runtime.vertices[vertex + 0u].position =
      vec3_make(x0, layer_elevation, z0);
  s_runtime.vertices[vertex + 1u].position =
      vec3_make(x1, layer_elevation, z0);
  s_runtime.vertices[vertex + 2u].position =
      vec3_make(x1, layer_elevation, z1);
  s_runtime.vertices[vertex + 3u].position =
      vec3_make(x0, layer_elevation, z1);

  for (u32 i = 0u; i < 4u; ++i)
  {
    s_runtime.vertices[vertex + i].normal =
        vec3_make(0.0f, 1.0f, 0.0f);
    s_runtime.vertices[vertex + i].color = LDK_RGBA32(color);
  }

  s_runtime.vertices[vertex + 0u].uv = vec2_make(uv.u0, uv.v0);
  s_runtime.vertices[vertex + 1u].uv = vec2_make(uv.u1, uv.v0);
  s_runtime.vertices[vertex + 2u].uv = vec2_make(uv.u1, uv.v1);
  s_runtime.vertices[vertex + 3u].uv = vec2_make(uv.u0, uv.v1);

  s_runtime.indices[index + 0u] = vertex + 0u;
  s_runtime.indices[index + 1u] = vertex + 2u;
  s_runtime.indices[index + 2u] = vertex + 1u;
  s_runtime.indices[index + 3u] = vertex + 0u;
  s_runtime.indices[index + 4u] = vertex + 3u;
  s_runtime.indices[index + 5u] = vertex + 2u;
}

static bool s_island_terrain_mesh_rebuild(
    IslandTerrain *system, LDKRenderer *renderer)
{
  LDKRendererMeshDesc desc = {0};
  float origin_x;
  float origin_z;
  u32 max_quad_count;
  u32 quad_count = 0u;
  u32 vertex_count;
  u32 index_count;
  bool updated;

  if (!system || !renderer)
  {
    return false;
  }

  if (s_runtime.tile_count == 0u)
  {
    system->has_geometry = false;
    system->mesh_dirty = false;
    return true;
  }

  if (s_runtime.tile_count >
      UINT32_MAX / (u32)ISLAND_TERRAIN_SURFACE_COUNT)
  {
    return false;
  }

  max_quad_count =
      s_runtime.tile_count * (u32)ISLAND_TERRAIN_SURFACE_COUNT;
  if (!s_island_terrain_geometry_reserve(max_quad_count))
  {
    return false;
  }

  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;

  for (u32 tile_index = 0u; tile_index < s_runtime.tile_count; ++tile_index)
  {
    const IslandTerrainTile *tile = &s_runtime.tiles[tile_index];
    u32 layers = s_island_terrain_layers_at(tile->x, tile->y);

    for (u32 surface_index = 0u;
         surface_index < (u32)ISLAND_TERRAIN_SURFACE_COUNT;
         ++surface_index)
    {
      IslandTerrainSurface surface = (IslandTerrainSurface)surface_index;
      u32 surface_bit = 1u << surface_index;
      u32 cutout_mask;

      if ((layers & surface_bit) == 0u)
      {
        continue;
      }

      cutout_mask = s_island_terrain_cutout_mask(
          surface, tile->x, tile->y);
      s_island_terrain_quad_write(system, quad_count, tile->x, tile->y,
          surface, cutout_mask, origin_x, origin_z);
      quad_count += 1u;
    }
  }

  if (quad_count == 0u || quad_count > UINT32_MAX / 4u ||
      quad_count > UINT32_MAX / 6u)
  {
    system->has_geometry = false;
    return false;
  }

  vertex_count = quad_count * 4u;
  index_count = quad_count * 6u;

  desc.vertices = s_runtime.vertices;
  desc.vertex_count = vertex_count;
  desc.indices = s_runtime.indices;
  desc.index_count = index_count;

  if (ldk_renderer_mesh_is_valid(renderer, system->mesh))
  {
    updated = ldk_renderer_mesh_update(renderer, system->mesh, &desc);
    if (!updated)
    {
      /* mesh_update() preserves the previous GPU buffers on failure. Keep the
       * old terrain visible and retry this rebuild on the next frame instead
       * of dropping the ground entirely. */
      system->has_geometry = true;
      return false;
    }
  }
  else
  {
    system->mesh = ldk_renderer_mesh_create(renderer, &desc);
    updated = ldk_renderer_mesh_is_valid(renderer, system->mesh);
    if (!updated)
    {
      system->mesh = ldk_renderer_mesh_null();
      system->has_geometry = false;
      return false;
    }
  }

  system->mesh_update_count += 1u;
  system->mesh_dirty = false;
  system->has_geometry = true;
  return true;
}

int island_terrain_system_initialize(void *data)
{
  IslandTerrain *system = data;
  u64 cell_count;

  if (!system || s_runtime.owner)
  {
    return -1;
  }

  s_island_terrain_generation_defaults(system);
  if (!s_island_terrain_generation_valid(system))
  {
    ldk_log_error("Invalid IslandTerrain generation settings.\n");
    return -1;
  }

  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.owner = system;
  s_runtime.dry_grass_type_id = LDK_GRASS_TYPE_ID_INVALID;
  s_runtime.grass_type_id = LDK_GRASS_TYPE_ID_INVALID;
  s_runtime.forest_grass_type_id = LDK_GRASS_TYPE_ID_INVALID;
  s_runtime.thorn_grass_type_id = LDK_GRASS_TYPE_ID_INVALID;
  s_runtime.shallow_grass_type_id = LDK_GRASS_TYPE_ID_INVALID;
  s_runtime.material_asset = ldk_asset_material_null();
  s_runtime.renderer_material = ldk_renderer_material_null();
  s_runtime.renderer_texture = ldk_renderer_texture_null();
  s_runtime.renderer_normal_map = ldk_renderer_texture_null();
  s_runtime.renderer_specular_map = ldk_renderer_texture_null();
  s_runtime.terrain_image = ldk_asset_image_null();

  s_island_map_release();
  system->map_loaded_from_cache = s_island_map_load(system);
  if (system->map_loaded_from_cache)
  {
    ldk_log_info("Loaded island map from %s.\n", ISLAND_MAP_FILE_NAME);
  }
  else
  {
    ldk_log_info("Generating island map with seed %u.\n", system->seed);
    if (!s_island_map_generate(system))
    {
      ldk_log_error("Failed to generate island map.\n");
      return -1;
    }

    if (!s_island_map_save(system))
    {
      ldk_log_warning("Failed to save island map to %s.\n",
          ISLAND_MAP_FILE_NAME);
    }
  }

#ifdef LDK_LDK_EDITOR
  (void)s_island_map_height_debug_write_ppm("island_heightmap.ppm");
#endif

  {
    LDKAssetManager *assets =
        (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    if (!assets)
    {
      ldk_log_error("AssetManager unavailable while creating island heightmap.\n");
      s_island_map_release();
      memset(&s_runtime, 0, sizeof(s_runtime));
      return -1;
    }
    s_runtime.terrain_image = ldk_asset_manager_image_create_format(
        assets, s_map.width, s_map.height, LDK_IMAGE_FORMAT_RGBA16_UNORM,
        s_map.data);
    if (x_handle_is_null(s_runtime.terrain_image.h) ||
        !ldk_terrain_heightmap_set(s_runtime.terrain_image))
    {
      ldk_log_error("Failed to publish island image to TerrainSystem.\n");
      if (!x_handle_is_null(s_runtime.terrain_image.h))
        ldk_asset_manager_image_unload(assets, s_runtime.terrain_image);
      s_island_map_release();
      memset(&s_runtime, 0, sizeof(s_runtime));
      return -1;
    }
  }

  cell_count = (u64)s_map.width * (u64)s_map.height;
  if (cell_count == 0u || cell_count > SIZE_MAX / sizeof(float))
  {
    ldk_terrain_heightmap_set(ldk_asset_image_null());
    {
      LDKAssetManager *assets =
          (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
      if (assets && !x_handle_is_null(s_runtime.terrain_image.h))
        ldk_asset_manager_image_unload(assets, s_runtime.terrain_image);
    }
    s_island_map_release();
    memset(&s_runtime, 0, sizeof(s_runtime));
    return -1;
  }
  s_runtime.grass_density_scales =
      (float *)malloc((size_t)cell_count * sizeof(float));
  if (!s_runtime.grass_density_scales)
  {
    ldk_terrain_heightmap_set(ldk_asset_image_null());
    {
      LDKAssetManager *assets =
          (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
      if (assets && !x_handle_is_null(s_runtime.terrain_image.h))
        ldk_asset_manager_image_unload(assets, s_runtime.terrain_image);
    }
    s_island_map_release();
    memset(&s_runtime, 0, sizeof(s_runtime));
    return -1;
  }
  for (u64 i = 0u; i < cell_count; ++i)
  {
    s_runtime.grass_density_scales[i] = 1.0f;
  }

  system->center_x = INT32_MIN;
  system->center_y = INT32_MIN;
  system->cached_radius = UINT32_MAX;
  system->cached_cell_size = 0.0f;
  system->cached_elevation = 0.0f;
  system->map_revision = 0u;
  system->active_tile_count = 0u;
  system->tiles_added = 0u;
  system->tiles_removed = 0u;
  system->mesh_update_count = 0u;
  system->mesh_dirty = true;
  system->has_geometry = false;
  system->mesh = ldk_renderer_mesh_null();
  return 0;
}

void island_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  IslandTerrain *system = data;
  LDKAssetManager *assets;
  LDKRenderer *renderer;
  LDKResourceMaterial material;
  Mat4 tracked_world;
  float origin_x;
  float origin_z;
  float center_world_x;
  float center_world_z;
  Vec3 forward;
  i32 center_x;
  i32 center_y;

  (void)dt;

  if (!system || s_runtime.owner != system || !group || group->count == 0u ||
      !s_map.cells || system->radius > INT32_MAX ||
      !isfinite(system->cell_size) || system->cell_size <= 0.0f ||
      !isfinite(system->elevation) || !isfinite(system->forward_offset))
  {
    return;
  }

  if (!ldk_transform_get_world_matrix(group->entities[0], &tracked_world))
  {
    return;
  }

  renderer = (LDKRenderer *)ldk_module_get(LDK_MODULE_RENDERER);
  assets = (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!renderer || !assets)
  {
    return;
  }

  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;

  forward = s_island_terrain_flat_forward(tracked_world);
  center_world_x = tracked_world.m[12] + forward.x * system->forward_offset;
  center_world_z = tracked_world.m[14] + forward.z * system->forward_offset;

  center_x = (i32)floorf((center_world_x - origin_x) / system->cell_size);
  center_y = (i32)floorf((center_world_z - origin_z) / system->cell_size);

  bool tiles_synced =
      s_island_terrain_tiles_sync(system, center_x, center_y);

  /* TerrainSystem owns the ground when present. IslandTerrain still streams
   * gameplay cells and grass, but its old atlas mesh becomes a fallback only. */
  if (ldk_terrain_system_is_active())
  {
    return;
  }

  if (tiles_synced && system->mesh_dirty)
  {
    (void)s_island_terrain_mesh_rebuild(system, renderer);
  }

  /* Streaming/grass/mesh rebuild failures are transient and must not make a
   * previously valid terrain mesh disappear. If synchronization or rebuild
   * failed, submit the last known-good GPU mesh and retry on a later frame. */
  if (!system->has_geometry ||
      !ldk_renderer_mesh_is_valid(renderer, system->mesh))
  {
    return;
  }

  if (!s_island_terrain_material_get(system, renderer, assets, &material))
  {
    return;
  }

  ldk_renderer_submit_mesh_with_flags(renderer, system->mesh, material,
      mat4_identity(), LDK_RENDERER_MESH_SUBMIT_FLAG_NONE);
}

void island_terrain_system_terminate(void *data)
{
  IslandTerrain *system = data;
  LDKRenderer *renderer;

  if (!system || s_runtime.owner != system)
  {
    return;
  }

  renderer = (LDKRenderer *)ldk_module_get(LDK_MODULE_RENDERER);
  if (renderer)
  {
    s_island_terrain_material_release(renderer);

    if (ldk_renderer_mesh_is_valid(renderer, system->mesh))
    {
      ldk_renderer_mesh_destroy(renderer, system->mesh);
    }
  }

  for (u32 i = 0u; i < s_runtime.tile_count; ++i)
  {
    s_island_terrain_tile_grass_remove(&s_runtime.tiles[i]);
  }

  free(s_runtime.tiles);
  free(s_runtime.grass_density_scales);
  free(s_runtime.vertices);
  free(s_runtime.indices);
  ldk_terrain_heightmap_set(ldk_asset_image_null());
  {
    LDKAssetManager *assets =
        (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    if (assets && !x_handle_is_null(s_runtime.terrain_image.h))
      ldk_asset_manager_image_unload(assets, s_runtime.terrain_image);
  }
  memset(&s_runtime, 0, sizeof(s_runtime));
  s_island_map_release();

  system->mesh = ldk_renderer_mesh_null();
  system->active_tile_count = 0u;
  system->mesh_dirty = false;
  system->has_geometry = false;
  system->map_loaded_from_cache = false;
}
