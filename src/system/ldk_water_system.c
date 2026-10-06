#include <system/ldk_water_system.h>

#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <stdx/stdx_math.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKWaterChunk
{
  LDKResourceMesh mesh;
  Vec3 bounds_min;
  Vec3 bounds_max;
} LDKWaterChunk;

typedef struct LDKWaterRuntime
{
  LDKWaterSystem *owner;
  LDKWaterChunk *chunks;
  u32 chunk_count;
  u64 geometry_hash;
  double time_seconds;
  LDKAssetImage image_assets[LDK_RENDERER_WATER_TEXTURE_COUNT];
  LDKResourceTexture textures[LDK_RENDERER_WATER_TEXTURE_COUNT];
} LDKWaterRuntime;

static LDKWaterRuntime s_runtime;

static LDKRendererWaterDesc s_water_desc(const LDKWaterSystem *system)
{
  LDKRendererWaterDesc desc = {0};
  desc.water_level = system->water_level;
  desc.shallow_color = system->shallow_color;
  desc.deep_color = system->deep_color;
  desc.depth_color_distance = system->depth_color_distance;
  desc.edge_fade_distance = system->edge_fade_distance;
  desc.waves[0] =
      (LDKRendererWaterWave){system->wave_0_height, system->wave_0_length,
          system->wave_0_speed, system->wave_0_direction_degrees};
  desc.waves[1] =
      (LDKRendererWaterWave){system->wave_1_height, system->wave_1_length,
          system->wave_1_speed, system->wave_1_direction_degrees};
  desc.waves[2] =
      (LDKRendererWaterWave){system->wave_2_height, system->wave_2_length,
          system->wave_2_speed, system->wave_2_direction_degrees};
  desc.detail_scale = system->detail_scale;
  desc.detail_strength = system->detail_strength;
  desc.detail_speed = system->detail_speed;
  desc.specular = system->specular;
  desc.shininess = system->shininess;
  desc.foam_color = system->foam_color;
  desc.foam_width = system->foam_width;
  desc.foam_strength = system->foam_strength;
  desc.foam_scale = system->foam_scale;
  desc.shore_range = system->shore_range;
  desc.shore_wave_length = system->shore_wave_length;
  desc.shore_wave_speed = system->shore_wave_speed;
  desc.shore_foam_strength = system->shore_foam_strength;
  memcpy(desc.textures, s_runtime.textures, sizeof(desc.textures));
  desc.noise_scale = system->noise_scale;
  desc.noise_speed = system->noise_speed;
  desc.distortion_strength = system->distortion_strength;
  desc.foam_speed = system->foam_speed;
  desc.foam_cutoff = system->foam_cutoff;
  desc.surface_foam_strength = system->surface_foam_strength;
  desc.caustics_scale = system->caustics_scale;
  desc.caustics_strength = system->caustics_strength;
  desc.caustics_speed = system->caustics_speed;
  desc.caustics_depth = system->caustics_depth;
  desc.time_seconds = (float)s_runtime.time_seconds;
  return desc;
}

LDK_API void ldk_water_system_defaults(LDKWaterSystem *system)
{
  if (!system)
  {
    return;
  }

  LDKRendererWaterDesc desc;
  ldk_renderer_water_desc_defaults(&desc);
  memset(system, 0, sizeof(*system));
  system->width = 128.0f;
  system->depth = 128.0f;
  system->cell_size = 0.5f;
  system->chunk_size = 32u;
  system->shallow_color = desc.shallow_color;
  system->deep_color = desc.deep_color;
  system->depth_color_distance = desc.depth_color_distance;
  system->edge_fade_distance = desc.edge_fade_distance;
  system->wave_0_height = desc.waves[0].height;
  system->wave_0_length = desc.waves[0].length;
  system->wave_0_speed = desc.waves[0].speed;
  system->wave_0_direction_degrees = desc.waves[0].direction_degrees;
  system->wave_1_height = desc.waves[1].height;
  system->wave_1_length = desc.waves[1].length;
  system->wave_1_speed = desc.waves[1].speed;
  system->wave_1_direction_degrees = desc.waves[1].direction_degrees;
  system->wave_2_height = desc.waves[2].height;
  system->wave_2_length = desc.waves[2].length;
  system->wave_2_speed = desc.waves[2].speed;
  system->wave_2_direction_degrees = desc.waves[2].direction_degrees;
  system->detail_scale = desc.detail_scale;
  system->detail_strength = desc.detail_strength;
  system->detail_speed = desc.detail_speed;
  system->specular = desc.specular;
  system->shininess = desc.shininess;
  system->foam_color = desc.foam_color;
  system->foam_width = desc.foam_width;
  system->foam_strength = desc.foam_strength;
  system->foam_scale = desc.foam_scale;
  system->shore_range = desc.shore_range;
  system->shore_wave_length = desc.shore_wave_length;
  system->shore_wave_speed = desc.shore_wave_speed;
  system->shore_foam_strength = desc.shore_foam_strength;
  system->normal_texture = ldk_asset_image_null();
  system->foam_texture = ldk_asset_image_null();
  system->noise_texture = ldk_asset_image_null();
  system->caustics_texture = ldk_asset_image_null();
  system->noise_scale = desc.noise_scale;
  system->noise_speed = desc.noise_speed;
  system->distortion_strength = desc.distortion_strength;
  system->foam_speed = desc.foam_speed;
  system->foam_cutoff = desc.foam_cutoff;
  system->surface_foam_strength = desc.surface_foam_strength;
  system->caustics_scale = desc.caustics_scale;
  system->caustics_strength = desc.caustics_strength;
  system->caustics_speed = desc.caustics_speed;
  system->caustics_depth = desc.caustics_depth;
}

static void s_textures_update(
    const LDKWaterSystem *system, LDKRenderer *renderer)
{
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  const LDKAssetImage images[LDK_RENDERER_WATER_TEXTURE_COUNT] = {
      system->normal_texture, system->foam_texture, system->noise_texture,
      system->caustics_texture};
  const char *labels[LDK_RENDERER_WATER_TEXTURE_COUNT] = {
      "normal", "foam", "noise", "caustics"};
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    bool changed = images[i].h.index != s_runtime.image_assets[i].h.index ||
                   images[i].h.version != s_runtime.image_assets[i].h.version;
    bool authored = images[i].h.index != X_HPOOL_NULL_INDEX;
    if (!changed && (!authored || ldk_renderer_texture_is_valid(
                                      renderer, s_runtime.textures[i])))
    {
      continue;
    }
    const LDKAssetImageData *image =
        authored && assets
            ? ldk_asset_manager_image_get_const(assets, images[i])
            : NULL;
    LDKResourceTexture next =
        image && !image->is_missing
            ? ldk_renderer_image_acquire(renderer, assets, images[i])
            : ldk_renderer_texture_null();
    if (changed && authored && assets &&
        !ldk_renderer_texture_is_valid(renderer, next))
    {
      ldk_log_error("WaterSystem: unavailable %s texture.\n", labels[i]);
    }
    ldk_renderer_image_release(renderer, s_runtime.textures[i]);
    s_runtime.image_assets[i] = images[i];
    s_runtime.textures[i] = next;
  }
}

static bool s_config_valid(const LDKWaterSystem *system)
{
  if (!system || !isfinite(system->center_x) || !isfinite(system->center_z) ||
      !isfinite(system->width) || !isfinite(system->depth) ||
      !isfinite(system->cell_size) || system->width <= 0.0f ||
      system->depth <= 0.0f || system->cell_size < 0.01f ||
      system->chunk_size == 0u || system->chunk_size > 128u ||
      !isfinite(fabsf(system->center_x) + system->width * 0.5f) ||
      !isfinite(fabsf(system->center_z) + system->depth * 0.5f))
  {
    return false;
  }

  double cells_x = ceil((double)system->width / system->cell_size);
  double cells_z = ceil((double)system->depth / system->cell_size);
  if (cells_x > UINT32_MAX || cells_z > UINT32_MAX)
  {
    return false;
  }

  LDKRendererWaterDesc desc = s_water_desc(system);
  return ldk_renderer_water_desc_is_valid(&desc);
}

static u64 s_geometry_hash(const LDKWaterSystem *system)
{
  const float values[] = {system->center_x, system->center_z, system->width,
      system->depth, system->cell_size};
  const u8 *bytes = (const u8 *)values;
  u64 hash = UINT64_C(14695981039346656037);
  for (size_t i = 0u; i < sizeof(values); ++i)
  {
    hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
  }
  return (hash ^ system->chunk_size) * UINT64_C(1099511628211);
}

static void s_chunks_destroy(
    LDKRenderer *renderer, LDKWaterChunk *chunks, u32 count)
{
  if (renderer)
  {
    for (u32 i = 0u; i < count; ++i)
    {
      ldk_renderer_mesh_destroy(renderer, chunks[i].mesh);
    }
  }
  free(chunks);
}

static bool s_chunks_rebuild(LDKWaterSystem *system, LDKRenderer *renderer)
{
  u32 cells_x = (u32)ceil((double)system->width / system->cell_size);
  u32 cells_z = (u32)ceil((double)system->depth / system->cell_size);
  u32 chunks_x =
      (u32)(((u64)cells_x + system->chunk_size - 1u) / system->chunk_size);
  u32 chunks_z =
      (u32)(((u64)cells_z + system->chunk_size - 1u) / system->chunk_size);
  u64 count64 = (u64)chunks_x * chunks_z;
  if (!count64 || count64 > UINT32_MAX ||
      count64 > SIZE_MAX / sizeof(LDKWaterChunk))
  {
    return false;
  }

  LDKWaterChunk *chunks = calloc((size_t)count64, sizeof(*chunks));
  if (!chunks)
  {
    return false;
  }

  float origin_x = system->center_x - system->width * 0.5f;
  float origin_z = system->center_z - system->depth * 0.5f;
  u32 built = 0u;
  for (u32 z = 0u; z < chunks_z; ++z)
  {
    for (u32 x = 0u; x < chunks_x; ++x)
    {
      u32 start_x = x * system->chunk_size;
      u32 start_z = z * system->chunk_size;
      u32 nx = cells_x - start_x;
      u32 nz = cells_z - start_z;
      if (nx > system->chunk_size)
      {
        nx = system->chunk_size;
      }
      if (nz > system->chunk_size)
      {
        nz = system->chunk_size;
      }

      u32 row = nx + 1u;
      u32 vertex_count = row * (nz + 1u);
      u32 index_count = nx * nz * 6u;
      LDKMeshVertex *vertices = calloc(vertex_count, sizeof(*vertices));
      u32 *indices = malloc((size_t)index_count * sizeof(*indices));
      if (!vertices || !indices)
      {
        free(vertices);
        free(indices);
        s_chunks_destroy(renderer, chunks, built);
        return false;
      }

      for (u32 vz = 0u; vz <= nz; ++vz)
      {
        for (u32 vx = 0u; vx <= nx; ++vx)
        {
          LDKMeshVertex *vertex = &vertices[vz * row + vx];
          vertex->position = vec3_make(
              origin_x + fminf((float)(start_x + vx) * system->cell_size,
                             system->width),
              0.0f,
              origin_z + fminf((float)(start_z + vz) * system->cell_size,
                             system->depth));
          vertex->normal = vec3_make(0.0f, 1.0f, 0.0f);
          vertex->color = LDK_RGBA32(0xffffffffu);
        }
      }

      u32 ii = 0u;
      for (u32 iz = 0u; iz < nz; ++iz)
      {
        for (u32 ix = 0u; ix < nx; ++ix)
        {
          u32 v0 = iz * row + ix;
          u32 v1 = v0 + 1u;
          u32 v3 = v0 + row;
          u32 v2 = v3 + 1u;
          indices[ii++] = v0;
          indices[ii++] = v2;
          indices[ii++] = v1;
          indices[ii++] = v0;
          indices[ii++] = v3;
          indices[ii++] = v2;
        }
      }

      LDKRendererMeshDesc desc = {0};
      desc.vertices = vertices;
      desc.vertex_count = vertex_count;
      desc.indices = indices;
      desc.index_count = index_count;
      LDKWaterChunk *chunk = &chunks[built];
      chunk->bounds_min = vertices[0].position;
      chunk->bounds_max = vertices[vertex_count - 1u].position;
      chunk->mesh = ldk_renderer_mesh_create(renderer, &desc);
      free(vertices);
      free(indices);
      if (!ldk_renderer_mesh_is_valid(renderer, chunk->mesh))
      {
        s_chunks_destroy(renderer, chunks, built);
        return false;
      }
      ++built;
    }
  }

  /* Commit only after every new chunk exists. A failed rebuild leaves the
   * previous water surface and its GPU resources intact for the next retry. */
  s_chunks_destroy(renderer, s_runtime.chunks, s_runtime.chunk_count);
  s_runtime.chunks = chunks;
  s_runtime.chunk_count = built;
  s_runtime.geometry_hash = s_geometry_hash(system);
  system->chunk_count = built;
  system->mesh_update_count += 1u;
  system->has_geometry = true;
  return true;
}

LDK_API bool ldk_water_system_is_active(void)
{
  return s_runtime.owner != NULL;
}

LDK_API bool ldk_water_height_at_world(
    float world_x, float world_z, float *out_height)
{
  const LDKWaterSystem *system = s_runtime.owner;
  if (!out_height || !s_config_valid(system) || !isfinite(world_x) ||
      !isfinite(world_z) ||
      fabsf(world_x - system->center_x) > system->width * 0.5f ||
      fabsf(world_z - system->center_z) > system->depth * 0.5f)
  {
    return false;
  }

  LDKRendererWaterDesc desc = s_water_desc(system);
  double height = system->water_level;
  for (u32 i = 0u; i < LDK_RENDERER_WATER_WAVE_COUNT; ++i)
  {
    const LDKRendererWaterWave *wave = &desc.waves[i];
    double angle = (double)wave->direction_degrees * 0.017453292519943295;
    double position = cos(angle) * world_x + sin(angle) * world_z;
    double phase = 6.283185307179586 *
                   (position - (double)wave->speed * desc.time_seconds) /
                   wave->length;
    height += wave->height * sin(phase);
  }
  *out_height = (float)height;
  return true;
}

LDK_API int ldk_water_system_initialize(void *data)
{
  LDKWaterSystem *system = data;
  if (!system || s_runtime.owner)
  {
    return -1;
  }

  LDKWaterSystem empty = {0};
  bool zero_initialized = memcmp(system, &empty, sizeof(*system)) == 0;
  /* Assigning an image must not suppress scalar defaults on a new system. */
  empty.normal_texture = system->normal_texture;
  empty.foam_texture = system->foam_texture;
  empty.noise_texture = system->noise_texture;
  empty.caustics_texture = system->caustics_texture;
  LDKWaterSystem defaults;
  ldk_water_system_defaults(&defaults);
  if (zero_initialized || memcmp(system, &empty, sizeof(*system)) == 0)
  {
    if (!zero_initialized)
    {
      defaults.normal_texture = system->normal_texture;
      defaults.foam_texture = system->foam_texture;
      defaults.noise_texture = system->noise_texture;
      defaults.caustics_texture = system->caustics_texture;
    }
    *system = defaults;
  }
  if (system->width == 0.0f)
  {
    system->width = defaults.width;
  }
  if (system->depth == 0.0f)
  {
    system->depth = defaults.depth;
  }
  if (system->cell_size == 0.0f)
  {
    system->cell_size = defaults.cell_size;
  }
  if (system->chunk_size == 0u)
  {
    system->chunk_size = defaults.chunk_size;
  }
  if (system->depth_color_distance == 0.0f)
  {
    system->depth_color_distance = defaults.depth_color_distance;
  }
  if (system->detail_scale == 0.0f)
  {
    system->detail_scale = defaults.detail_scale;
  }
  if (system->foam_scale == 0.0f)
  {
    system->foam_scale = defaults.foam_scale;
  }
  if (system->noise_scale == 0.0f)
  {
    system->noise_scale = defaults.noise_scale;
  }
  if (system->caustics_scale == 0.0f)
  {
    system->caustics_scale = defaults.caustics_scale;
  }
  if (system->caustics_depth == 0.0f)
  {
    system->caustics_depth = defaults.caustics_depth;
  }
  if (system->shininess == 0.0f)
  {
    system->shininess = defaults.shininess;
  }
  if (system->shore_wave_length == 0.0f)
  {
    system->shore_wave_length = defaults.shore_wave_length;
  }
  if (system->wave_0_length == 0.0f)
  {
    system->wave_0_length = defaults.wave_0_length;
  }
  if (system->wave_1_length == 0.0f)
  {
    system->wave_1_length = defaults.wave_1_length;
  }
  if (system->wave_2_length == 0.0f)
  {
    system->wave_2_length = defaults.wave_2_length;
  }
  if (!s_config_valid(system))
  {
    ldk_log_error("Invalid WaterSystem configuration.\n");
    return -1;
  }

  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.owner = system;
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    s_runtime.image_assets[i] = ldk_asset_image_null();
  }
  system->chunk_count = 0u;
  system->visible_chunk_count = 0u;
  system->mesh_update_count = 0u;
  system->has_geometry = false;
  return 0;
}

LDK_API void ldk_water_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  (void)group;
  LDKWaterSystem *system = data;
  if (!system || s_runtime.owner != system)
  {
    return;
  }
  system->visible_chunk_count = 0u;
  if (!s_config_valid(system))
  {
    return;
  }
  if (isfinite(dt) && dt > 0.0f)
  {
    s_runtime.time_seconds += dt;
  }

  LDKRenderer *renderer = ldk_module_get(LDK_MODULE_RENDERER);
  if (!renderer || !renderer->is_initialized)
  {
    return;
  }
  s_textures_update(system, renderer);
  if (!system->has_geometry ||
      s_runtime.geometry_hash != s_geometry_hash(system))
  {
    if (!s_chunks_rebuild(system, renderer) && !system->has_geometry)
    {
      return;
    }
  }

  LDKRendererWaterDesc desc = s_water_desc(system);
  float amplitude =
      system->wave_0_height + system->wave_1_height + system->wave_2_height;
  for (u32 i = 0u; i < s_runtime.chunk_count; ++i)
  {
    LDKWaterChunk *chunk = &s_runtime.chunks[i];
    Vec3 bounds_min = chunk->bounds_min;
    Vec3 bounds_max = chunk->bounds_max;
    bounds_min.y = system->water_level - amplitude;
    bounds_max.y = system->water_level + amplitude;
    if (ldk_renderer_view_bounds_visible(
            renderer, LDK_RENDERER_VIEW_ALL, bounds_min, bounds_max) &&
        ldk_renderer_submit_water(
            renderer, LDK_RENDERER_VIEW_ALL, chunk->mesh, &desc))
    {
      system->visible_chunk_count += 1u;
    }
  }
}

LDK_API void ldk_water_system_terminate(void *data)
{
  LDKWaterSystem *system = data;
  if (!system || s_runtime.owner != system)
  {
    return;
  }
  LDKRenderer *renderer = ldk_module_get(LDK_MODULE_RENDERER);
  s_chunks_destroy(renderer, s_runtime.chunks, s_runtime.chunk_count);
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    ldk_renderer_image_release(renderer, s_runtime.textures[i]);
  }
  memset(&s_runtime, 0, sizeof(s_runtime));
  system->chunk_count = 0u;
  system->visible_chunk_count = 0u;
  system->has_geometry = false;
}
