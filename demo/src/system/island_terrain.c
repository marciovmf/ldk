#include "island_terrain.h"
#include "tftf_terrain_atlas.h"

#include <component/ldk_transform.h>
#include <ldk.h>
#include <ldk_material_asset.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <stdx/stdx_math.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISLAND_MAP_FILE_NAME "map.bin"
#define ISLAND_MAP_FILE_MAGIC 0x4C444B4Du
#define ISLAND_MAP_FILE_VERSION 1u
#define ISLAND_MAP_GENERATOR_VERSION 1u

#define ISLAND_TERRAIN_COLOR_DEEP_WATER 0x426F7DFFu
#define ISLAND_TERRAIN_COLOR_SHALLOW_WATER 0x6696A0FFu
#define ISLAND_TERRAIN_COLOR_SAND 0xC8AA74FFu
#define ISLAND_TERRAIN_COLOR_GRASS 0x78945AFFu
#define ISLAND_TERRAIN_COLOR_GRASS_DARK 0x657F4DFFu
#define ISLAND_TERRAIN_COLOR_ROCK 0x77766FFFu

#define ISLAND_DECORATION_RULE_COUNT 7u
#define ISLAND_RESOURCE_RULE_COUNT 6u

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
  system->moisture_scale = 0.018f;
  system->land_octaves = 5u;
  system->terrain_octaves = 4u;
  system->moisture_octaves = 3u;
  system->land_noise_strength = 0.75f;
  system->deep_water_max = 0.00f;
  system->shallow_water_max = 0.06f;
  system->shore_max = 0.11f;
  system->mountain_min = 0.70f;
  system->dry_moisture_max = 0.25f;
  system->grass_moisture_max = 0.55f;

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
      !isfinite(system->moisture_scale) || system->moisture_scale <= 0.0f ||
      system->land_octaves == 0u || system->land_octaves > 32u ||
      system->terrain_octaves == 0u || system->terrain_octaves > 32u ||
      system->moisture_octaves == 0u || system->moisture_octaves > 32u ||
      !isfinite(system->land_noise_strength) ||
      system->land_noise_strength < 0.0f)
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
  ISLAND_HASH_FIELD(moisture_scale);
  ISLAND_HASH_FIELD(land_octaves);
  ISLAND_HASH_FIELD(terrain_octaves);
  ISLAND_HASH_FIELD(moisture_octaves);
  ISLAND_HASH_FIELD(land_noise_strength);
  ISLAND_HASH_FIELD(deep_water_max);
  ISLAND_HASH_FIELD(shallow_water_max);
  ISLAND_HASH_FIELD(shore_max);
  ISLAND_HASH_FIELD(mountain_min);
  ISLAND_HASH_FIELD(dry_moisture_max);
  ISLAND_HASH_FIELD(grass_moisture_max);
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
    const IslandTerrain *system, float terrain, float moisture)
{
  if (terrain >= system->mountain_min)
  {
    return ISLAND_BIOME_MOUNTAIN;
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

static u8 s_island_map_prop_select(u32 x, u32 y, float dist,
    IslandTerrainClass terrain_class, IslandBiome biome, u32 world_seed,
    const IslandMapPropRule *rules, u32 rule_count, bool use_center_bias)
{
  for (u32 rule_index = 0u; rule_index < rule_count; ++rule_index)
  {
    const IslandMapPropRule *rule = &rules[rule_index];
    float random;
    float value;

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
      return rule->value;
    }
  }

  return 0u;
}

static void s_island_map_release(void)
{
  free(s_map.cells);
  memset(&s_map, 0, sizeof(s_map));
}

static void s_island_map_take(IslandMapCell *cells, u32 width, u32 height)
{
  u32 revision = s_map.revision + 1u;

  if (revision == 0u)
  {
    revision = 1u;
  }

  free(s_map.cells);
  s_map.cells = cells;
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

static bool s_island_map_load(IslandTerrain *system)
{
  IslandMapFileHeader header;
  IslandMapCell *cells = NULL;
  FILE *file;
  u64 cell_count;
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
  if (cell_count == 0u || cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(*cells)))
  {
    goto done;
  }

  cells = (IslandMapCell *)malloc((size_t)cell_count * sizeof(*cells));
  if (!cells)
  {
    goto done;
  }

  if (fread(cells, sizeof(*cells), (size_t)cell_count, file) !=
      (size_t)cell_count)
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

  s_island_map_take(cells, header.width, header.height);
  cells = NULL;
  ok = true;

done:
  free(cells);
  fclose(file);
  return ok;
}

static bool s_island_map_save(const IslandTerrain *system)
{
  IslandMapFileHeader header;
  FILE *file;
  u64 cell_count;
  bool ok = false;

  if (!system || !s_map.cells || s_map.width == 0u || s_map.height == 0u)
  {
    return false;
  }

  cell_count = (u64)s_map.width * (u64)s_map.height;
  if (cell_count == 0u || cell_count > SIZE_MAX)
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
      (size_t)cell_count)
  {
    goto done;
  }

  ok = true;

done:
  fclose(file);
  return ok;
}

static bool s_island_map_generate(const IslandTerrain *system)
{
  IslandMapCell *cells;
  IslandMapPropRule decorations[ISLAND_DECORATION_RULE_COUNT];
  IslandMapPropRule resources[ISLAND_RESOURCE_RULE_COUNT];
  u64 cell_count;

  if (!system)
  {
    return false;
  }

  cell_count = (u64)system->map_size * (u64)system->map_size;
  if (cell_count == 0u || cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(*cells)))
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
  if (!cells)
  {
    return false;
  }

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
      IslandTerrainClass terrain_class =
          s_island_terrain_class_select(system, land);
      IslandBiome biome = ISLAND_BIOME_GRASS;
      IslandMapCell *cell =
          &cells[(u64)y * (u64)system->map_size + (u64)x];

      if (terrain_class == ISLAND_TERRAIN_CLASS_LAND)
      {
        float terrain = s_fbm_compose_2d(
            (float)x * system->terrain_scale,
            (float)y * system->terrain_scale, system->seed,
            system->terrain_octaves);
        float moisture = s_fbm_compose_2d(
            (float)x * system->moisture_scale,
            (float)y * system->moisture_scale, system->seed,
            system->moisture_octaves);

        biome = s_island_biome_select(system, terrain, moisture);
      }

      cell->terrain_class = (u8)terrain_class;
      cell->biome = (u8)biome;
      cell->decoration_rule = s_island_map_prop_select(x, y, dist,
          terrain_class, biome, system->seed, decorations,
          (u32)ARRAY_COUNT(decorations), false);
      cell->resource_rule = s_island_map_prop_select(x, y, dist,
          terrain_class, biome, system->seed, resources,
          (u32)ARRAY_COUNT(resources), true);
    }
  }

  s_island_map_take(cells, system->map_size, system->map_size);
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

static void s_island_terrain_tiles_reset(
    IslandTerrain *system, const IslandTerrainBounds *bounds)
{
  system->tiles_removed += s_runtime.tile_count;
  s_runtime.tile_count = 0u;

  if (!bounds->valid)
  {
    system->active_tile_count = 0u;
    return;
  }

  for (i32 y = bounds->min_y; y <= bounds->max_y; ++y)
  {
    for (i32 x = bounds->min_x; x <= bounds->max_x; ++x)
    {
      IslandTerrainTile *tile = &s_runtime.tiles[s_runtime.tile_count++];

      tile->x = x;
      tile->y = y;
      system->tiles_added += 1u;
    }
  }

  system->active_tile_count = s_runtime.tile_count;
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
      system->tiles_removed += 1u;
    }
  }

  s_runtime.tile_count = write_index;
  system->active_tile_count = write_index;
}

static void s_island_terrain_tiles_add_difference(IslandTerrain *system,
    const IslandTerrainBounds *old_bounds,
    const IslandTerrainBounds *new_bounds)
{
  if (!new_bounds->valid)
  {
    return;
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
      system->tiles_added += 1u;
    }
  }

  system->active_tile_count = s_runtime.tile_count;
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

  new_bounds = s_island_terrain_bounds(center_x, center_y, system->radius);

  if (!s_island_terrain_bounds_cell_count(&new_bounds, &new_tile_count) ||
      !s_island_terrain_tiles_reserve(new_tile_count))
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
    s_island_terrain_tiles_reset(system, &new_bounds);
    membership_changed = true;
  }
  else if (system->center_x != center_x || system->center_y != center_y)
  {
    old_bounds = s_island_terrain_bounds(
        system->center_x, system->center_y, system->cached_radius);

    if (!s_island_terrain_bounds_equal(&old_bounds, &new_bounds))
    {
      s_island_terrain_tiles_remove_outside(system, &new_bounds);
      s_island_terrain_tiles_add_difference(
          system, &old_bounds, &new_bounds);
      membership_changed = true;
    }
  }

  if (s_runtime.tile_count != new_tile_count)
  {
    s_island_terrain_tiles_reset(system, &new_bounds);
    membership_changed = true;
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
  }
  else
  {
    system->mesh = ldk_renderer_mesh_create(renderer, &desc);
    updated = ldk_renderer_mesh_is_valid(renderer, system->mesh);
  }

  if (!updated)
  {
    system->mesh = ldk_renderer_mesh_null();
    system->has_geometry = false;
    return false;
  }

  system->mesh_update_count += 1u;
  system->mesh_dirty = false;
  system->has_geometry = true;
  return true;
}

int island_terrain_system_initialize(void *data)
{
  IslandTerrain *system = data;

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
  s_runtime.material_asset = ldk_asset_material_null();
  s_runtime.renderer_material = ldk_renderer_material_null();
  s_runtime.renderer_texture = ldk_renderer_texture_null();
  s_runtime.renderer_normal_map = ldk_renderer_texture_null();
  s_runtime.renderer_specular_map = ldk_renderer_texture_null();

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

  if (!s_island_terrain_tiles_sync(system, center_x, center_y))
  {
    return;
  }

  if (system->mesh_dirty &&
      !s_island_terrain_mesh_rebuild(system, renderer))
  {
    return;
  }

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

  free(s_runtime.tiles);
  free(s_runtime.vertices);
  free(s_runtime.indices);
  memset(&s_runtime, 0, sizeof(s_runtime));
  s_island_map_release();

  system->mesh = ldk_renderer_mesh_null();
  system->active_tile_count = 0u;
  system->mesh_dirty = false;
  system->has_geometry = false;
  system->map_loaded_from_cache = false;
}
