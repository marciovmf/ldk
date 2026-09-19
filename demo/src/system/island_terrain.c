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
#include <stdlib.h>
#include <string.h>

typedef struct IslandTerrainMap
{
  const u32 *colors;
  u32 width;
  u32 height;
  u32 revision;
} IslandTerrainMap;

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
} IslandTerrainRuntime;

static IslandTerrainMap s_map;
static IslandTerrainRuntime s_runtime;

#define ISLAND_TERRAIN_COLOR_DEEP_WATER 0x426F7DFFu
#define ISLAND_TERRAIN_COLOR_SHALLOW_WATER 0x6696A0FFu
#define ISLAND_TERRAIN_COLOR_SAND 0xC8AA74FFu
#define ISLAND_TERRAIN_COLOR_GRASS 0x78945AFFu
#define ISLAND_TERRAIN_COLOR_GRASS_DARK 0x657F4DFFu
#define ISLAND_TERRAIN_COLOR_ROCK 0x77766FFFu

typedef struct IslandTerrainUVRect
{
  float u0;
  float v0;
  float u1;
  float v1;
} IslandTerrainUVRect;

static LDKEditorIcon s_island_terrain_atlas_icon(u32 color)
{
  switch (color)
  {
  case ISLAND_TERRAIN_COLOR_DEEP_WATER:
    return TFTF_ICON_DEEP_WATER;
  case ISLAND_TERRAIN_COLOR_SHALLOW_WATER:
    return TFTF_ICON_SHALLOW_WATER;
  case ISLAND_TERRAIN_COLOR_SAND:
    return TFTF_ICON_SAND;
  case ISLAND_TERRAIN_COLOR_GRASS:
    return TFTF_ICON_GRASS;
  case ISLAND_TERRAIN_COLOR_GRASS_DARK:
    return TFTF_ICON_GRASS_DARK;
  case ISLAND_TERRAIN_COLOR_ROCK:
    return TFTF_ICON_ROCK;
  default:
    return TFTF_ICON_GRASS;
  }
}

static IslandTerrainUVRect s_island_terrain_atlas_uv(u32 color)
{
  const LDKRectf rect =
      tftf_icon_rects[s_island_terrain_atlas_icon(color)];
  const float half_texel_u = 0.5f / (float)TFTF_ICON_ATLAS_WIDTH;
  const float half_texel_v = 0.5f / (float)TFTF_ICON_ATLAS_HEIGHT;
  IslandTerrainUVRect uv;

  uv.u0 = rect.x + half_texel_u;
  uv.v0 = rect.y + half_texel_v;
  uv.u1 = rect.x + rect.w - half_texel_u;
  uv.v1 = rect.y + rect.h - half_texel_v;
  return uv;
}

bool island_terrain_map_set(const u32 *colors, u32 width, u32 height)
{
  if (!colors || width == 0u || height == 0u || width > INT32_MAX ||
      height > INT32_MAX)
  {
    return false;
  }

  s_map.colors = colors;
  s_map.width = width;
  s_map.height = height;
  s_map.revision += 1u;

  if (s_map.revision == 0u)
  {
    s_map.revision = 1u;
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

  if (!s_map.colors || radius > INT32_MAX)
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
  float length_squared =
      forward.x * forward.x + forward.z * forward.z;

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

  new_bounds =
      s_island_terrain_bounds(center_x, center_y, system->radius);

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
  s_runtime.material_asset = ldk_asset_material_null();
  s_runtime.material_revision = 0u;
  s_runtime.renderer_material = ldk_renderer_material_null();
  s_runtime.renderer_texture = ldk_renderer_texture_null();
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
      !ldk_renderer_material_is_valid(
          renderer, s_runtime.renderer_material);

  if (needs_resolve)
  {
    if (!ldk_renderer_material_resolve(renderer, assets, &data->descriptor,
            &s_runtime.renderer_material, &s_runtime.renderer_texture))
    {
      return false;
    }

    s_runtime.material_asset = system->material;
    s_runtime.material_revision = data->revision;
  }

  *out_material = s_runtime.renderer_material;
  return ldk_renderer_material_is_valid(renderer, *out_material);
}

static bool s_island_terrain_mesh_rebuild(
    IslandTerrain *system, LDKRenderer *renderer)
{
  LDKRendererMeshDesc desc = {0};
  float origin_x;
  float origin_z;
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

  if (!s_island_terrain_geometry_reserve(s_runtime.tile_count))
  {
    return false;
  }

  origin_x = -(float)s_map.width * system->cell_size * 0.5f;
  origin_z = -(float)s_map.height * system->cell_size * 0.5f;

  for (u32 tile_index = 0u; tile_index < s_runtime.tile_count; ++tile_index)
  {
    const IslandTerrainTile *tile = &s_runtime.tiles[tile_index];
    u32 vertex = tile_index * 4u;
    u32 index = tile_index * 6u;
    u32 color =
        s_map.colors[(u32)tile->y * s_map.width + (u32)tile->x];
    IslandTerrainUVRect uv = s_island_terrain_atlas_uv(color);
    float x0 = origin_x + (float)tile->x * system->cell_size;
    float x1 = x0 + system->cell_size;
    float z0 = origin_z + (float)tile->y * system->cell_size;
    float z1 = z0 + system->cell_size;

    s_runtime.vertices[vertex + 0u].position =
        vec3_make(x0, system->elevation, z0);
    s_runtime.vertices[vertex + 1u].position =
        vec3_make(x1, system->elevation, z0);
    s_runtime.vertices[vertex + 2u].position =
        vec3_make(x1, system->elevation, z1);
    s_runtime.vertices[vertex + 3u].position =
        vec3_make(x0, system->elevation, z1);

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

  vertex_count = s_runtime.tile_count * 4u;
  index_count = s_runtime.tile_count * 6u;

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

  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.owner = system;
  s_runtime.material_asset = ldk_asset_material_null();
  s_runtime.renderer_material = ldk_renderer_material_null();
  s_runtime.renderer_texture = ldk_renderer_texture_null();

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

  if (!system || s_runtime.owner != system || !group ||
      group->count == 0u || !s_map.colors ||
      system->radius > INT32_MAX ||
      !isfinite(system->cell_size) ||
      system->cell_size <= 0.0f ||
      !isfinite(system->elevation) ||
      !isfinite(system->forward_offset))
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
  center_world_x =
      tracked_world.m[12] + forward.x * system->forward_offset;
  center_world_z =
      tracked_world.m[14] + forward.z * system->forward_offset;

  center_x =
      (i32)floorf((center_world_x - origin_x) / system->cell_size);
  center_y =
      (i32)floorf((center_world_z - origin_z) / system->cell_size);

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

  if (!s_island_terrain_material_get(
          system, renderer, assets, &material))
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

  system->mesh = ldk_renderer_mesh_null();
  system->active_tile_count = 0u;
  system->mesh_dirty = false;
  system->has_geometry = false;
}
