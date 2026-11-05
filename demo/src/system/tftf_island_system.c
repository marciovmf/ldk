#include "tftf_island_system.h"
#include "tftf_island_generator.h"
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
#define ISLAND_MAP_FILE_VERSION 3u
#define ISLAND_MAP_GENERATOR_VERSION TFTF_ISLAND_GENERATOR_VERSION

#define ISLAND_TERRAIN_COLOR_DEEP_WATER 0x426F7DFFu
#define ISLAND_TERRAIN_COLOR_SHALLOW_WATER 0x6696A0FFu
#define ISLAND_TERRAIN_COLOR_SAND 0xC8AA74FFu
#define ISLAND_TERRAIN_COLOR_GRASS 0x78945AFFu
#define ISLAND_TERRAIN_COLOR_GRASS_DARK 0x657F4DFFu
#define ISLAND_TERRAIN_COLOR_ROCK 0x77766FFFu

#define ISLAND_DECORATION_SLOT_COUNT 8u
#define ISLAND_BIOME_INVALID UINT8_MAX

#define ISLAND_MAP_CHANNEL_HEIGHT 0u
#define ISLAND_MAP_CHANNEL_DECORATION 1u
#define ISLAND_MAP_CHANNEL_RESOURCE 2u
#define ISLAND_MAP_CHANNEL_RESERVED 3u
#define ISLAND_MAP_CHANNEL_COUNT 4u

#define ISLAND_MAP_PROP_TYPE_SHIFT 8u
#define ISLAND_MAP_PROP_DENSITY_MASK 0xffu

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/* Optional TFTF legacy atlas only. Not used to classify any biome. */
typedef enum IslandTerrainSurface
{
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
  u8 biome;
  u8 reserved;
  u8 decoration_mask;
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
  TFTFIslandSystem *owner;

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
  LDKGrassTypeId grass_type_ids[255];

  /* TFTFIslandSystem can initialize before TerrainSystem has consumed the
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
    TFTFIslandSystem *system, IslandTerrainTile *tile);

static float s_hash_2d_rand01(int x, int y, uint32_t seed)
{
  uint32_t h = s_hash_2d_i32(x, y, seed);
  return (float)(h >> 8) * (1.0f / 16777215.0f);
}

static u32 s_island_biome_count(const TFTFIslandSystem *system)
{
  return system && system->biomes
      ? x_array_IslandBiomeDesc_count(system->biomes) : 0u;
}

static const IslandBiomeDesc *s_island_biome_get(
    const TFTFIslandSystem *system, u32 index)
{
  return index < s_island_biome_count(system)
      ? x_array_IslandBiomeDesc_get_const(system->biomes, index) : NULL;
}

void tftf_island_biome_initialize(void *element)
{
  IslandBiomeDesc *b = element;
  if (!b)
    return;
  memset(b, 0, sizeof(*b));
  x_smallstr_from_cstr(&b->name, "New Biome");
  b->gameplay_tag = 255u;
  b->legacy_atlas_layer = 4u;
  b->generator_surface_enabled = false;
  b->generator_surface = 3u;
  b->resource_center_bias = true;
  b->inherit_decoration_chance = true;
}

static void s_island_terrain_generation_defaults(TFTFIslandSystem *system)
{
  if (!system)
  {
    return;
  }
  if (system->land_coverage_percent == 0.0f)
  {
    system->land_coverage_percent = 33.0f;
    system->min_island_gap_tiles = 3.0f;
    system->max_island_gap_tiles = 5.0f;
    system->sea_level = 0.12f;
    system->land_height_scale = 2.0f;
    system->underwater_depth_scale = 0.5f;
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
}

static bool s_island_chance_is_valid(float chance)
{
  return isfinite(chance) && chance >= 0.0f && chance <= 1.0f;
}

static float s_island_decoration_chance_raw_get(
    const TFTFIslandSystem *system, u32 biome_index, u32 slot)
{
  const IslandBiomeDesc *b = s_island_biome_get(system, biome_index);
  if (!b)
  {
    return 0.0f;
  }
  switch (slot)
  {
  case 0u: return b->decoration_0_chance;
  case 1u: return b->decoration_1_chance;
  case 2u: return b->decoration_2_chance;
  case 3u: return b->decoration_3_chance;
  case 4u: return b->decoration_4_chance;
  case 5u: return b->decoration_5_chance;
  case 6u: return b->decoration_6_chance;
  case 7u: return b->decoration_7_chance;
  default: return 0.0f;
  }
}

static float s_island_decoration_chance_get(
    const TFTFIslandSystem *system, u32 biome_index, u32 slot)
{
  float chance = s_island_decoration_chance_raw_get(system, biome_index, slot);
  const IslandBiomeDesc *b = s_island_biome_get(system, biome_index);
  if (chance > 0.0f || slot >= ISLAND_DECORATION_SLOT_COUNT ||
      !b || !b->inherit_decoration_chance)
    return chance;
  /* Retain the generator's existing zero-slot frequency inheritance. */
  for (u32 i = 0u; i < ISLAND_DECORATION_SLOT_COUNT; ++i)
    chance = fmaxf(chance,
        s_island_decoration_chance_raw_get(system, biome_index, i));
  return chance;
}

static bool s_island_terrain_generation_valid(const TFTFIslandSystem *system)
{
  u64 cell_count;
  u32 count = s_island_biome_count(system);
  if (!system || system->seed == 0u || system->map_size == 0u ||
      system->map_size > INT32_MAX || count == 0u || count > 255u)
  {
    if (count == 0u)
      ldk_log_error("TFTFIslandSystem has no configured biomes.\n");
    return false;
  }

  cell_count = (u64)system->map_size * (u64)system->map_size;
  if (cell_count > UINT32_MAX ||
      cell_count > (u64)(SIZE_MAX / sizeof(IslandMapCell)))
    return false;

  if (!isfinite(system->cell_size) || system->cell_size <= 0.0f ||
      !isfinite(system->land_coverage_percent) ||
      system->land_coverage_percent < 1.0f ||
      system->land_coverage_percent > 80.0f ||
      !isfinite(system->min_island_gap_tiles) ||
      system->min_island_gap_tiles < 0.0f ||
      !isfinite(system->max_island_gap_tiles) ||
      system->max_island_gap_tiles <
          system->min_island_gap_tiles ||
      !isfinite(system->sea_level) ||
      system->sea_level < 0.0f ||
      system->sea_level > 1.0f ||
      !isfinite(system->land_height_scale) ||
      system->land_height_scale <= 0.0f ||
      !isfinite(system->underwater_depth_scale) ||
      system->underwater_depth_scale <= 0.0f)
    return false;

  for (u32 i = 0u; i < count; ++i)
  {
    const IslandBiomeDesc *b = s_island_biome_get(system, i);
    if (!b || (b->generator_surface_enabled &&
          b->generator_surface >= TFTF_ISLAND_GENERATOR_SURFACE_COUNT) ||
        b->legacy_atlas_layer >= ISLAND_TERRAIN_SURFACE_COUNT ||
        b->gameplay_tag > 255u || b->resource_type > 255u ||
        !s_island_chance_is_valid(b->resource_chance))
    {
      ldk_log_error("TFTFIslandSystem: invalid biome settings at index %u.\n", i);
      return false;
    }
    for (u32 slot = 0u; slot < ISLAND_DECORATION_SLOT_COUNT; ++slot)
      if (!s_island_chance_is_valid(s_island_decoration_chance_raw_get(
              system, i, slot)))
        return false;
  }
  return true;
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

static u64 s_island_terrain_generation_hash(const TFTFIslandSystem *system)
{
  u64 hash = UINT64_C(14695981039346656037);
#define HASH_FIELD(field) s_island_hash_bytes(&hash, &system->field, sizeof(system->field))
#define HASH_BIOME(field) s_island_hash_bytes(&hash, &b->field, sizeof(b->field))
  HASH_FIELD(seed);
  HASH_FIELD(map_size);
  HASH_FIELD(cell_size);
  HASH_FIELD(land_coverage_percent);
  HASH_FIELD(min_island_gap_tiles);
  HASH_FIELD(max_island_gap_tiles);
  HASH_FIELD(sea_level);
  HASH_FIELD(land_height_scale);
  HASH_FIELD(underwater_depth_scale);
  {
    const u32 version = TFTF_ISLAND_GENERATOR_VERSION;
    s_island_hash_bytes(&hash, &version, sizeof(version));
  }
  u32 count = s_island_biome_count(system);
  s_island_hash_bytes(&hash, &count, sizeof(count));
  for (u32 i = 0u; i < count; ++i)
  {
    const IslandBiomeDesc *b = s_island_biome_get(system, i);
    HASH_BIOME(gameplay_tag);
    HASH_BIOME(legacy_atlas_layer);
    HASH_BIOME(generator_surface_enabled);
    HASH_BIOME(generator_surface);
    HASH_BIOME(resource_chance);
    HASH_BIOME(resource_type);
    HASH_BIOME(resource_center_bias);
    HASH_BIOME(inherit_decoration_chance);
    for (u32 slot = 0u; slot < ISLAND_DECORATION_SLOT_COUNT; ++slot)
    {
      float chance = s_island_decoration_chance_get(system, i, slot);
      s_island_hash_bytes(&hash, &chance, sizeof(chance));
    }
  }
#undef HASH_FIELD
#undef HASH_BIOME
  return hash;
}

static bool s_island_map_cell_tile_kind(
    const IslandMapCell *cell, TFTFIslandTileKind *out_kind)
{
  const IslandBiomeDesc *b = cell && s_runtime.owner
      ? s_island_biome_get(s_runtime.owner, cell->biome) : NULL;
  if (!b || !out_kind || b->gameplay_tag >= TFTF_ISLAND_TILE_COUNT)
    return false;
  *out_kind = (TFTFIslandTileKind)b->gameplay_tag;
  return true;
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

u8 tftf_island_map_decoration_mask_get(u16 packed)
{
  return (u8)(packed >> ISLAND_MAP_PROP_TYPE_SHIFT);
}

float tftf_island_map_density_get(u16 packed)
{
  return (float)(packed & ISLAND_MAP_PROP_DENSITY_MASK) * (1.0f / 255.0f);
}

static u16 s_island_map_decoration_pack(u8 mask, float density)
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
  return (u16)(((u16)mask << ISLAND_MAP_PROP_TYPE_SHIFT) |
      (u16)(density_u8 & ISLAND_MAP_PROP_DENSITY_MASK));
}

static u16 s_island_map_decoration_select(u32 x, u32 y,
    u32 biome_index, u32 world_seed, const TFTFIslandSystem *system)
{
  u8 mask = 0u;
  float density = 0.0f;
  for (u32 slot = 0u; slot < ISLAND_DECORATION_SLOT_COUNT; ++slot)
  {
    float chance = s_island_decoration_chance_get(system, biome_index, slot);
    u32 seed = s_hash_u32(0x64a2f9bdu ^
        (biome_index * ISLAND_DECORATION_SLOT_COUNT + slot));
    float random = s_hash_2d_rand01((int)x, (int)y, world_seed ^ seed);
    if (chance <= 0.0f || random >= chance)
    {
      continue;
    }
    mask |= (u8)(1u << slot);
    density = fmaxf(density, chance);
  }
  return mask ? s_island_map_decoration_pack(mask, density) : 0u;
}

static u16 s_island_map_resource_select(u32 x, u32 y, float dist,
    u32 biome_index, u32 world_seed, const TFTFIslandSystem *system)
{
  const IslandBiomeDesc *b = s_island_biome_get(system, biome_index);
  if (!b || b->resource_type == 0u || b->resource_chance <= 0.0f)
  {
    return 0u;
  }
  float random = s_hash_2d_rand01((int)x, (int)y,
      world_seed ^ s_hash_u32(0x44cc8821u + biome_index));
  float value = b->resource_center_bias ? random * dist : random;
  if (value >= b->resource_chance)
  {
    return 0u;
  }
  float density = b->resource_chance;
  if (b->resource_center_bias && dist > 1e-6f)
  {
    density /= dist;
  }
  return s_island_map_prop_pack((u8)b->resource_type, density);
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
  const IslandBiomeDesc *b = cell && s_runtime.owner
      ? s_island_biome_get(s_runtime.owner, cell->biome) : NULL;
  return b && (cell->resource_rule == 0u ||
      cell->resource_rule == b->resource_type);
}

static bool s_island_map_data_valid(const IslandMapCell *cells,
    const u16 *data, u64 cell_count)
{
  if (!cells || !data)
  {
    return false;
  }

  for (u64 i = 0u; i < cell_count; ++i)
  {
    const u16 *texel = &data[i * ISLAND_MAP_CHANNEL_COUNT];
    if (s_island_map_prop_type(texel[ISLAND_MAP_CHANNEL_RESOURCE]) !=
            cells[i].resource_rule ||
        s_island_map_prop_type(texel[ISLAND_MAP_CHANNEL_DECORATION]) !=
            cells[i].decoration_mask)
    {
      return false;
    }
  }
  return true;
}

#ifndef LDK_MONOLITHIC
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

static bool s_island_map_load(TFTFIslandSystem *system)
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
  if (!s_island_map_data_valid(cells, map_data, cell_count))
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

static bool s_island_map_save(const TFTFIslandSystem *system)
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

/* The geography generator is deliberately independent of the LDK. This
 * adapter owns the 4xRGBA16 map, biome/vegetation resolution and TFTF rules. */
static u32 s_tftf_island_generator_gameplay_tag(u32 surface)
{
  static const u32 tags[TFTF_ISLAND_GENERATOR_SURFACE_COUNT] = {
    TFTF_ISLAND_TILE_DEEP_WATER,
    TFTF_ISLAND_TILE_SHALLOW_WATER,
    TFTF_ISLAND_TILE_SHORE,
    TFTF_ISLAND_TILE_GRASS,
    TFTF_ISLAND_TILE_DRY,
    TFTF_ISLAND_TILE_FOREST,
    TFTF_ISLAND_TILE_GRASS, /* Existing scenes: Tall Grass falls back to Grass. */
    TFTF_ISLAND_TILE_THORNS,
    TFTF_ISLAND_TILE_MOUNTAIN
  };
  return surface < TFTF_ISLAND_GENERATOR_SURFACE_COUNT
      ? tags[surface] : UINT32_MAX;
}

static u8 s_tftf_island_biome_from_surface(
    const TFTFIslandSystem *system, u32 surface)
{
  /* Explicit mappings take precedence. A designer can add another Grass
   * biome for Tall Grass without depending on its name or its array order. */
  for (u32 i = 0u; i < s_island_biome_count(system); ++i)
  {
    const IslandBiomeDesc *b = s_island_biome_get(system, i);
    if (b->generator_surface_enabled && b->generator_surface == surface)
      return (u8)i;
  }
  const u32 tag = s_tftf_island_generator_gameplay_tag(surface);
  for (u32 i = 0u; i < s_island_biome_count(system); ++i)
  {
    const IslandBiomeDesc *b = s_island_biome_get(system, i);
    if (!b->generator_surface_enabled && b->gameplay_tag == tag)
      return (u8)i;
  }
  return ISLAND_BIOME_INVALID;
}

static bool s_island_map_generate(const TFTFIslandSystem *system)
{
  TFTFIslandGeneratorConfig config;
  TFTFIslandGeneratorMap generated = {0};
  IslandMapCell *cells = NULL;
  u16 *map_data = NULL;
  bool ok = false;
  const u64 count = (u64)system->map_size * system->map_size;
  if (!count || count > UINT32_MAX ||
      count > SIZE_MAX / sizeof(*cells) ||
      count > SIZE_MAX / (ISLAND_MAP_CHANNEL_COUNT * sizeof(u16)))
    return false;
  tftf_island_generator_config_defaults(&config);
  config.seed = system->seed;
  config.map_size = system->map_size;
  config.cell_size_meters = system->cell_size;
  config.land_percent = system->land_coverage_percent;
  config.min_satellite_gap_tiles =
      system->min_island_gap_tiles;
  config.max_satellite_gap_tiles =
      system->max_island_gap_tiles;
  if (!tftf_island_generator_generate(&config, &generated))
  {
    ldk_log_error("TFTF island geography generation failed (seed %u).\n",
        system->seed);
    return false;
  }
  cells = (IslandMapCell *)malloc((size_t)count * sizeof(*cells));
  map_data = (u16 *)calloc((size_t)count * ISLAND_MAP_CHANNEL_COUNT,
      sizeof(*map_data));
  if (!cells || !map_data) goto done;

  for (u32 y = 0u; y < system->map_size; ++y)
  {
    for (u32 x = 0u; x < system->map_size; ++x)
    {
      const u32 at = y * system->map_size + x;
      const TFTFIslandGeneratorCell *sample = &generated.cells[at];
      const u8 biome = s_tftf_island_biome_from_surface(
          system, sample->surface);
      if (biome == ISLAND_BIOME_INVALID)
      {
        ldk_log_error("TFTF Island: no configured biome for generator "
            "surface %u at (%u, %u).\n", sample->surface, x, y);
        goto done;
      }
      IslandMapCell *cell = &cells[at];
      u16 *texel = &map_data[(size_t)at * ISLAND_MAP_CHANNEL_COUNT];
      const float normalized = fminf(1.0f, fmaxf(0.0f,
          system->sea_level +
          sample->height * (sample->height < 0.0f
              ? system->underwater_depth_scale
              : system->land_height_scale)));
      texel[ISLAND_MAP_CHANNEL_HEIGHT] =
          (u16)(normalized * 65535.0f + 0.5f);
      /* High byte: 8-bit decorative vegetation mask. Low byte:
       * independent density noise generated by the island generator. */
      const u16 decoration = s_island_map_decoration_select(
          x, y, biome, system->seed, system);
      const u16 mask = decoration & 0xff00u;
      const u16 density = (u16)(fminf(1.0f, fmaxf(0.0f,
          sample->density)) * 255.0f + 0.5f);
      texel[ISLAND_MAP_CHANNEL_DECORATION] = mask | density;
      const float nx = ((float)x + 0.5f) / (float)system->map_size * 2.0f - 1.0f;
      const float ny = ((float)y + 0.5f) / (float)system->map_size * 2.0f - 1.0f;
      const float distance = sqrtf(nx * nx + ny * ny);
      const u16 resource = s_island_map_resource_select(
          x, y, distance, biome, system->seed, system);
      texel[ISLAND_MAP_CHANNEL_RESOURCE] = resource;
      cell->biome = biome;
      cell->reserved = 0u;
      cell->decoration_mask = s_island_map_prop_type(mask);
      cell->resource_rule = s_island_map_prop_type(resource);
    }
  }
  s_island_map_take(cells, map_data, system->map_size, system->map_size);
  cells = NULL;
  map_data = NULL;
  ldk_log_info("TFTF island generated: seed %u, satellites %u, "
      "escarpments %u, inaccessible cells %u.\n", system->seed,
      generated.satellite_count, generated.formation_count,
      generated.unreachable_land_cells);
  ok = true;
done:
  free(cells);
  free(map_data);
  tftf_island_generator_release(&generated);
  return ok;
}

bool tftf_island_map_data_get(IslandMapDataView *out_view)
{
  if (!out_view || !s_runtime.owner || !s_map.data || s_map.width == 0u ||
      s_map.height == 0u)
  {
    return false;
  }

  out_view->texels = s_map.data;
  out_view->width = s_map.width;
  out_view->height = s_map.height;
  out_view->revision = s_map.revision;
  out_view->seed = s_runtime.owner->seed;
  return true;
}

static IslandTerrainSurface s_island_map_cell_surface(
    const IslandMapCell *cell)
{
  const IslandBiomeDesc *b = cell && s_runtime.owner
      ? s_island_biome_get(s_runtime.owner, cell->biome) : NULL;
  return b ? (IslandTerrainSurface)b->legacy_atlas_layer
      : ISLAND_TERRAIN_SURFACE_GRASS;
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

static LDKGrassTypeId s_tftf_island_grass_type_id(u32 biome_index)
{
  if (biome_index >= s_island_biome_count(s_runtime.owner))
  {
    return LDK_GRASS_TYPE_ID_INVALID;
  }
  return s_runtime.grass_type_ids[biome_index];
}

static bool s_tftf_island_grass_types_resolve(
    const TFTFIslandSystem *system, bool *out_changed)
{
  if (!system)
  {
    return false;
  }
  bool changed = false;
  u32 count = s_island_biome_count(system);
  for (u32 i = 0u; i < count; ++i)
  {
    const IslandBiomeDesc *b = s_island_biome_get(system, i);
    LDKGrassTypeId id = LDK_GRASS_TYPE_ID_INVALID;
    if (x_smallstr_length(&b->grass_type_name) != 0u)
    {
      id = ldk_grass_get_id_by_name(x_smallstr_cstr(&b->grass_type_name));
    }
    if (id != s_runtime.grass_type_ids[i])
    {
      changed = true;
    }
    s_runtime.grass_type_ids[i] = id;
  }
  if (out_changed)
  {
    *out_changed = changed;
  }
  return true;
}

static bool s_island_terrain_tile_grass_desc(TFTFIslandSystem *system,
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
  type_id = s_tftf_island_grass_type_id(cell->biome);
  if (!ldk_grass_type_get(type_id, &type) || type.density == 0.0f)
  {
    return false;
  }

  ldk_grass_patch_desc_defaults(out_desc);
  const u32 cell_index = (u32)tile_y * s_map.width + (u32)tile_x;
  const u16 packed = s_map.data[(size_t)cell_index *
      ISLAND_MAP_CHANNEL_COUNT + ISLAND_MAP_CHANNEL_DECORATION];
  out_desc->density_scale =
      s_runtime.grass_density_scales[cell_index] *
      tftf_island_map_density_get(packed);
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
       * grass at the obsolete flat TFTFIslandSystem elevation: that puts it
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
    /* Legacy TFTFIslandSystem renderer fallback. */
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

bool tftf_island_world_to_cell(
    Vec3 world_position, i32 *out_x, i32 *out_y)
{
  TFTFIslandSystem *system = s_runtime.owner;
  float origin_x;
  float origin_z;
  i32 x;
  i32 y;

  if (!system || !out_x || !out_y || !s_map.cells ||
      !isfinite(world_position.x) || !isfinite(world_position.z) ||
      !isfinite(system->cell_size) || system->cell_size <= 0.0f)
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

bool tftf_island_cell_world_bounds(
    i32 cell_x, i32 cell_y, Vec2 *out_min, Vec2 *out_max)
{
  TFTFIslandSystem *system = s_runtime.owner;
  float origin_x;
  float origin_z;
  float x0;
  float z0;

  if (!system || !out_min || !out_max || !s_map.cells ||
      cell_x < 0 || cell_y < 0 || cell_x >= (i32)s_map.width ||
      cell_y >= (i32)s_map.height || !isfinite(system->cell_size) ||
      system->cell_size <= 0.0f)
  {
    return false;
  }

  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;
  x0 = origin_x + (float)cell_x * system->cell_size;
  z0 = origin_z + (float)cell_y * system->cell_size;

  *out_min = vec2_make(x0, z0);
  *out_max =
      vec2_make(x0 + system->cell_size, z0 + system->cell_size);
  return true;
}

bool tftf_island_tile_kind_at_cell(
    i32 cell_x, i32 cell_y, TFTFIslandTileKind *out_kind)
{
  if (!out_kind || !s_map.cells || cell_x < 0 || cell_y < 0 ||
      cell_x >= (i32)s_map.width || cell_y >= (i32)s_map.height)
  {
    return false;
  }

  return s_island_map_cell_tile_kind(
      &s_map.cells[(u32)cell_y * s_map.width + (u32)cell_x], out_kind);
}

bool tftf_island_tile_kind_at(
    Vec3 world_position, TFTFIslandTileKind *out_kind)
{
  i32 cell_x;
  i32 cell_y;

  if (!out_kind ||
      !tftf_island_world_to_cell(world_position, &cell_x, &cell_y))
  {
    return false;
  }

  return tftf_island_tile_kind_at_cell(cell_x, cell_y, out_kind);
}

bool tftf_island_biome_index_at_cell(
    i32 cell_x, i32 cell_y, u32 *out_index)
{
  if (!out_index || !s_map.cells || cell_x < 0 || cell_y < 0 ||
      cell_x >= (i32)s_map.width || cell_y >= (i32)s_map.height)
    return false;
  const IslandMapCell *cell = &s_map.cells[
      (u32)cell_y * s_map.width + (u32)cell_x];
  if (!s_island_biome_get(s_runtime.owner, cell->biome))
  {
    return false;
  }
  *out_index = cell->biome;
  return true;
}

bool tftf_island_biome_index_at(Vec3 world_position, u32 *out_index)
{
  i32 cell_x;
  i32 cell_y;
  return out_index && tftf_island_world_to_cell(
      world_position, &cell_x, &cell_y) &&
      tftf_island_biome_index_at_cell(cell_x, cell_y, out_index);
}

bool tftf_island_grass_state_at(Vec3 world_position,
    float *out_density, float *out_min_height)
{
  LDKGrassPatchDesc patch;
  LDKGrassTypeDesc type;
  LDKGrassTypeId type_id;
  i32 x;
  i32 y;

  if (!out_density || !out_min_height ||
      !s_tftf_island_grass_types_resolve(s_runtime.owner, NULL) ||
      !tftf_island_world_to_cell(world_position, &x, &y) ||
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

bool tftf_island_grass_density_multiply_at(
    Vec3 world_position, float multiplier)
{
  TFTFIslandSystem *system = s_runtime.owner;
  u32 scale_index;
  float previous_scale;
  i32 x;
  i32 y;

  if (!isfinite(multiplier) || multiplier < 0.0f || multiplier > 1.0f ||
      !tftf_island_world_to_cell(world_position, &x, &y))
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
    TFTFIslandSystem *system, IslandTerrainTile *tile)
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

static bool s_tftf_island_grass_update_all(TFTFIslandSystem *system)
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
    TFTFIslandSystem *system, const IslandTerrainBounds *bounds)
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
    TFTFIslandSystem *system, const IslandTerrainBounds *bounds)
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

static bool s_island_terrain_tiles_add_difference(TFTFIslandSystem *system,
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
    TFTFIslandSystem *system, i32 center_x, i32 center_y)
{
  system->center_x = center_x;
  system->center_y = center_y;
  system->cached_radius = system->radius;
  system->cached_cell_size = system->cell_size;
  system->cached_elevation = system->elevation;
  system->map_revision = s_map.revision;
}

static bool s_island_terrain_tiles_sync(
    TFTFIslandSystem *system, i32 center_x, i32 center_y)
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
      !s_tftf_island_grass_types_resolve(system, &grass_types_changed))
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
      !s_tftf_island_grass_update_all(system))
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
    if (!s_tftf_island_grass_update_all(system))
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

static bool s_island_terrain_material_get(TFTFIslandSystem *system,
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

static void s_island_terrain_quad_write(TFTFIslandSystem *system,
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
    TFTFIslandSystem *system, LDKRenderer *renderer)
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

int tftf_island_system_initialize(void *data)
{
  TFTFIslandSystem *system = data;
  u64 cell_count;

  if (!system || s_runtime.owner)
  {
    return -1;
  }

  s_island_terrain_generation_defaults(system);
  if (!s_island_terrain_generation_valid(system))
  {
    ldk_log_error("Invalid TFTFIslandSystem generation settings.\n");
    return -1;
  }

  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.owner = system;
  for (u32 i = 0u; i < 255u; ++i)
    s_runtime.grass_type_ids[i] = LDK_GRASS_TYPE_ID_INVALID;
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
      s_island_map_release();
      memset(&s_runtime, 0, sizeof(s_runtime));
      return -1;
    }

    if (!s_island_map_save(system))
    {
      ldk_log_warning("Failed to save island map to %s.\n",
          ISLAND_MAP_FILE_NAME);
    }
  }

#ifndef LDK_MONOLITHIC
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

void tftf_island_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  TFTFIslandSystem *system = data;
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

  /* TerrainSystem owns the ground when present. TFTFIslandSystem still streams
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

void tftf_island_system_terminate(void *data)
{
  TFTFIslandSystem *system = data;
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
