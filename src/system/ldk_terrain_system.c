#include <system/ldk_terrain_system.h>

#include <ldk.h>
#include <ldk_image.h>
#include <ldk_material_asset.h>
#include <ldk_mesh.h>
#include <component/ldk_terrain_component.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <stdx/stdx_math.h>

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Diagnostic A/B switch (rebuild after changing it):
 * 0: normal terrain painting, with GPU uploads and shader composition;
 * 1: bypass painting GPU uploads AND shader composition;
 * 2: keep paint GPU uploads, bypass shader composition.
 * CPU paint data, Undo/Redo and scene serialization are untouched. */
#ifndef LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE
#define LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE 0
#endif
#if LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE < 0 || \
    LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE > 2
#error Unsupported LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE
#endif

static bool s_terrain_perf_log_slow(double milliseconds)
{
  static u32 samples = 0u;
  if (milliseconds < 5.0) return false;
  ++samples;
  return samples <= 24u || samples % 60u == 0u;
}

typedef struct LDKTerrainChunk
{
  LDKResourceMesh mesh;
  Vec3 bounds_min;
  Vec3 bounds_max;
  /* Inclusive coordinates in the heightmap source grid. */
  u32 source_x0;
  u32 source_y0;
  u32 source_x1;
  u32 source_y1;
} LDKTerrainChunk;

typedef struct LDKTerrainRuntime
{
  LDKTerrainSystem *owner;
  LDKEntity entity;
  Mat4 world;
  bool seen;
  LDKTerrain config;
  u64 terrain_revision;
  /* Per-entity rendering settings, refreshed from TerrainComponent before
   * geometry validation and cache hashing on every visit. */
  u32 chunk_size;
  u32 mesh_sample_step;
  LDKAssetImage heightmap_asset;
  LDKAssetImage direct_heightmap;
  LDKAssetMaterial configured_material;
  LDKAssetTextureAtlas configured_atlas;
  LDKAssetImage configured_noise;
  LDKTerrainChunk *chunks;
  u32 chunk_capacity;
  u32 chunk_count;
  LDKImageInfo source;
  /* Never keep pointers into the component's replaceable image. Terrain
   * height queries can run between Inspector edits and the next mesh refresh. */
  LDKImage *source_owned;
  LDKAssetImage source_asset;
  u64 source_revision;
  u64 geometry_hash;
  u64 appearance_hash;
  LDKImage *color_noise_image;
  LDKAssetImage color_noise_loaded_asset;
  u64 color_noise_revision;

  LDKAssetMaterial material_asset;
  u64 material_revision;
  LDKResourceMaterial material;
  LDKResourceTexture texture;
  LDKResourceTexture normal_map;
  LDKResourceTexture specular_map;
  LDKResourceTexture paint_textures[2];
  u64 paint_revision;
  u32 paint_width;
  u32 paint_height;
} LDKTerrainRuntime;

/* Each TerrainComponent owns its render cache. The active pointer is used
 * only while processing one entity; helper functions never retain it. */
static LDKTerrainRuntime **s_terrain_entries;
static u32 s_terrain_count;
static u32 s_terrain_capacity;
static LDKTerrainRuntime *s_current;
#define s_runtime (*s_current)
static LDKTerrainSystem *s_play_owner;
static LDKAssetImage s_direct_heightmap;
static bool s_direct_heightmap_valid;

static bool s_entity_equal(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static LDKTerrainRuntime *s_terrain_entry_find(LDKEntity entity)
{
  for (u32 i = 0u; i < s_terrain_count; ++i)
    if (s_entity_equal(s_terrain_entries[i]->entity, entity))
      return s_terrain_entries[i];
  return NULL;
}

static LDKTerrainRuntime *s_terrain_entry_get(
    LDKEntity entity, LDKTerrainSystem *system)
{
  LDKTerrainRuntime *entry = s_terrain_entry_find(entity);
  if (entry)
  {
    entry->owner = system;
    return entry;
  }
  if (s_terrain_count == s_terrain_capacity)
  {
    u32 capacity = s_terrain_capacity ? s_terrain_capacity * 2u : 4u;
    LDKTerrainRuntime **entries = realloc(s_terrain_entries,
        (size_t)capacity * sizeof(*entries));
    if (!entries) return NULL;
    s_terrain_entries = entries;
    s_terrain_capacity = capacity;
  }
  entry = calloc(1u, sizeof(*entry));
  if (!entry) return NULL;
  entry->owner = system;
  entry->entity = entity;
  entry->world = mat4_identity();
  entry->heightmap_asset = ldk_asset_image_null();
  entry->direct_heightmap = s_direct_heightmap_valid
      ? s_direct_heightmap : ldk_asset_image_null();
  entry->terrain_revision = UINT64_MAX;
  entry->material = ldk_renderer_material_null();
  entry->texture = ldk_renderer_texture_null();
  entry->normal_map = ldk_renderer_texture_null();
  entry->specular_map = ldk_renderer_texture_null();
  entry->paint_textures[0] = ldk_renderer_texture_null();
  entry->paint_textures[1] = ldk_renderer_texture_null();
  entry->material_asset = ldk_asset_material_null();
  ldk_terrain_init(&entry->config);
  s_terrain_entries[s_terrain_count++] = entry;
  return entry;
}


static u64 s_hash_bytes(u64 hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;
  for (size_t i = 0u; i < size; ++i)
  {
    hash ^= (u64)bytes[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static bool s_asset_image_equal(LDKAssetImage a, LDKAssetImage b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static bool s_asset_material_equal(LDKAssetMaterial a, LDKAssetMaterial b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

/* Keep sampling identical to GrassSystem: world XZ, repeating bilinear
 * grayscale, with smoothstep interpolation. This modulates only brightness. */
static float s_color_noise_texel(const LDKImageInfo *image, u32 x, u32 y)
{
  const u8 *pixel = image->pixels +
      ((size_t)y * image->width + x) *
          image->channel_count * image->bytes_per_channel;
  if (image->bytes_per_channel == 2u)
  {
    u16 value;
    memcpy(&value, pixel, sizeof(value));
    return (float)value * (1.0f / 65535.0f);
  }
  return (float)pixel[0] * (1.0f / 255.0f);
}

static float s_color_noise_sample(const LDKTerrainSystem *system,
    float x, float z)
{
  (void)system;
  LDKImageInfo image;
  if (!s_runtime.color_noise_image ||
      !ldk_image_get_info(s_runtime.color_noise_image, &image) ||
      !image.pixels || !image.width || !image.height ||
      !image.channel_count || !image.bytes_per_channel)
    return 0.5f;

  float scale = s_runtime.config.color_noise_scale;
  float u = x * scale - floorf(x * scale);
  float v = z * scale - floorf(z * scale);
  float fx = u * image.width;
  float fy = v * image.height;
  u32 ix = (u32)floorf(fx);
  u32 iy = (u32)floorf(fy);
  float tx = fx - floorf(fx);
  float ty = fy - floorf(fy);
  tx = tx * tx * (3.0f - 2.0f * tx);
  ty = ty * ty * (3.0f - 2.0f * ty);
  u32 nx = (ix + 1u) % image.width;
  u32 ny = (iy + 1u) % image.height;
  float a = s_color_noise_texel(&image, ix, iy);
  float b = s_color_noise_texel(&image, nx, iy);
  float c = s_color_noise_texel(&image, ix, ny);
  float d = s_color_noise_texel(&image, nx, ny);
  return (a + (b - a) * tx) +
      ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

static float s_surface_min_height(const LDKTerrainSystem *s, u32 i);
static float s_surface_blend_width(const LDKTerrainSystem *s, u32 i);
static bool s_surface_use_slope(const LDKTerrainSystem *s, u32 i);
static float s_surface_min_slope(const LDKTerrainSystem *s, u32 i);
static float s_surface_slope_blend(const LDKTerrainSystem *s, u32 i);
static rgba32 s_surface_color(const LDKTerrainSystem *s, u32 i);
static rgba32 s_surface_noise_low(const LDKTerrainSystem *s, u32 i);
static rgba32 s_surface_noise_high(const LDKTerrainSystem *s, u32 i);

static float s_color_channel(rgba32 color, u32 shift)
{
  return (float)(((u32)color >> shift) & 255u) / 255.0f;
}

static float s_palette_channel(rgba32 base, rgba32 low, rgba32 high,
    float noise, u32 shift)
{
  float a = s_color_channel(base, shift);
  float target = noise < 0.5f
      ? s_color_channel(low, shift) +
          (a - s_color_channel(low, shift)) * (noise * 2.0f)
      : a + (s_color_channel(high, shift) - a) * ((noise - 0.5f) * 2.0f);
  return target;
}

/* Compute a single vertex modulation from the same height-based surface
 * weights used by the terrain shader. The per-surface palette is evaluated
 * before mixing, preserving smooth biome transitions. */
static u32 s_color_noise_vertex_color(const LDKTerrainSystem *system,
    float x, float y, float z, Vec3 normal)
{
  if (!s_runtime.color_noise_image || s_runtime.config.color_noise_strength <= 0.0f)
    return 0xffffff00u;

  float noise = s_color_noise_sample(system, x, z);
  float slope_degrees = acosf(fmaxf(0.0f, fminf(1.0f, normal.y))) *
      (180.0f / STDXM_PI);
  float original[3] = {0};
  float varied[3] = {0};
  const u32 shifts[3] = {24u, 16u, 8u};

  /* Elevation layers first, then slope layers; mirror the fragment shader. */
  for (u32 pass = 0u; pass < 2u; ++pass)
  {
    for (u32 i = 0u; i < s_runtime.config.surface_count; ++i)
    {
      bool use_slope = s_surface_use_slope(system, i);
      if ((pass == 0u && use_slope) || (pass == 1u && !use_slope))
        continue;

      rgba32 base = s_surface_color(system, i);
      rgba32 low = s_surface_noise_low(system, i);
      rgba32 high = s_surface_noise_high(system, i);
      float t = 1.0f;
      if (use_slope)
      {
        float threshold = s_surface_min_slope(system, i);
        float width = s_surface_slope_blend(system, i);
        if (width > 0.000001f)
        {
          t = fmaxf(0.0f, fminf(1.0f,
              (slope_degrees - (threshold - width)) / width));
          t = t * t * (3.0f - 2.0f * t);
        }
        else
          t = slope_degrees >= threshold ? 1.0f : 0.0f;
      }
      else if (i > 0u)
      {
        float threshold = s_runtime.config.height_offset +
            s_surface_min_height(system, i) * s_runtime.config.height_scale;
        float width = s_surface_blend_width(system, i) * s_runtime.config.height_scale;
        if (width > 0.000001f)
        {
          t = fmaxf(0.0f, fminf(1.0f,
              (y - (threshold - width)) / width));
          t = t * t * (3.0f - 2.0f * t);
        }
        else
          t = y >= threshold ? 1.0f : 0.0f;
      }
      for (u32 k = 0; k < 3u; ++k)
      {
        float b = s_color_channel(base, shifts[k]);
        float v = s_palette_channel(base, low, high, noise, shifts[k]);
        original[k] += (b - original[k]) * t;
        varied[k] += (v - varied[k]) * t;
      }
    }
  }

  u32 packed = 0xffu;
  for (u32 k = 0; k < 3u; ++k)
  {
    float base = original[k] * original[k];
    float target = varied[k] * varied[k];
    float factor = (1.0f - s_runtime.config.color_noise_strength) +
        s_runtime.config.color_noise_strength *
            (base > 0.000001f ? target / base : 1.0f);
    float encoded = powf(fminf(fmaxf(factor * 0.5f, 0.0f), 1.0f),
        1.0f / 2.2f);
    packed |= (u32)(encoded * 255.0f + 0.5f) << shifts[k];
  }
  return packed;
}

static bool s_color_noise_refresh(LDKTerrainSystem *system,
    LDKAssetManager *assets)
{
  (void)system;
  LDKAssetHandle generic = {s_runtime.configured_noise.h};
  const LDKAssetInfo *info = !x_handle_is_null(generic.h)
      ? ldk_asset_get_info_const(assets, generic) : NULL;
  u64 revision = info ? info->source_revision : 0u;
  const LDKAssetImageData *data = !x_handle_is_null(generic.h)
      ? ldk_asset_manager_image_get_const(assets, s_runtime.configured_noise)
      : NULL;
  LDKImage *image = data && !data->is_missing ? data->image : NULL;
  if (!x_handle_is_null(generic.h) && !image)
    return false;
  if (s_asset_image_equal(s_runtime.color_noise_loaded_asset,
        s_runtime.configured_noise) &&
      s_runtime.color_noise_revision == revision &&
      s_runtime.color_noise_image == image)
    return true;
  s_runtime.color_noise_image = image;
  s_runtime.color_noise_loaded_asset = s_runtime.configured_noise;
  s_runtime.color_noise_revision = revision;
  s_runtime.geometry_hash = 0u;
  return true;
}

/* Surface definitions now belong to the shared Terrain Asset. */
static float s_surface_min_height(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].min_height;
}
static float s_surface_blend_width(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].blend_width;
}
static rgba32 s_surface_color(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].color;
}
static rgba32 s_surface_noise_low(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  const LDKTerrainSurface *surface = &s_runtime.config.surfaces[i];
  return surface->noise_low_color ? surface->noise_low_color : surface->color;
}
static rgba32 s_surface_noise_high(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  const LDKTerrainSurface *surface = &s_runtime.config.surfaces[i];
  return surface->noise_high_color ? surface->noise_high_color : surface->color;
}
static u64 s_surface_region_id(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].sprite_id;
}
static Vec2 s_surface_uv_scale(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].uv_scale;
}
static float s_surface_texture_weight(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].texture_weight;
}
static bool s_surface_use_slope(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].use_slope;
}
static float s_surface_min_slope(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].min_slope_degrees;
}
static float s_surface_slope_blend(const LDKTerrainSystem *s, u32 i)
{
  (void)s;
  return s_runtime.config.surfaces[i].slope_blend_degrees;
}

static bool s_config_is_valid(const LDKTerrainSystem *system)
{
  return system && s_runtime.chunk_size > 0u &&
      s_runtime.chunk_size <= 256u &&
      s_runtime.mesh_sample_step > 0u &&
      s_runtime.mesh_sample_step <= 256u &&
      ldk_terrain_validate(&s_runtime.config);
}

/* TerrainComponent owns the configuration and heightmap. Keep an entity-local
 * rendering snapshot and rebuild only when its revision or image changes.
 * The direct procedural image API remains usable for flat default terrains. */
static bool s_config_refresh(LDKTerrainSystem *system, LDKAssetManager *assets,
    const LDKTerrainComponent *component)
{
  (void)system;
  LDKTerrain next;
  LDKAssetMaterial material = ldk_asset_material_null();
  LDKAssetTextureAtlas atlas = ldk_asset_texture_atlas_null();
  LDKAssetImage noise = ldk_asset_image_null();
  if (!component || !ldk_terrain_validate(&component->terrain)) return false;
  next = component->terrain;
  LDKAssetImage heightmap = component->heightmap
      ? ldk_asset_image_null() : s_runtime.direct_heightmap;
  const u64 revision = component->revision;
  if (s_asset_image_equal(s_runtime.heightmap_asset, heightmap))
  {
    if (s_runtime.terrain_revision == revision) return true;
    /* Sculpting changes component revisions but leaves all metadata and asset
     * references untouched. Avoid re-resolving materials and texture assets
     * on every mouse movement. Any real metadata change takes the slow path. */
    if (s_runtime.terrain_revision != UINT64_MAX &&
        memcmp(&s_runtime.config, &next, sizeof(next)) == 0)
    {
      s_runtime.terrain_revision = revision;
      return true;
    }
  }

  if (next.material_path.length)
  {
    LDKMaterialIOContext context = {0};
    context.assets = assets;
    material = ldk_asset_manager_material_load_shared(
        &context, next.material_path.buf, NULL);
    if (x_handle_is_null(material.h)) return false;
  }
  if (next.texture_atlas_path.length)
  {
    atlas = ldk_asset_manager_texture_atlas_load_shared(
        assets, next.texture_atlas_path.buf);
    if (x_handle_is_null(atlas.h)) return false;
  }
  if (next.color_noise_path.length)
  {
    noise = ldk_asset_manager_image_load_shared(
        assets, next.color_noise_path.buf);
    if (x_handle_is_null(noise.h)) return false;
  }
  s_runtime.config = next;
  s_runtime.configured_material = material;
  s_runtime.configured_atlas = atlas;
  s_runtime.configured_noise = noise;
  s_runtime.heightmap_asset = heightmap;
  s_runtime.terrain_revision = revision;
  /* Both geometry and material are validated by their own content hashes.
   * A vegetation-only metadata edit must not recreate terrain GPU objects. */
  return true;
}

static bool s_source_refresh(LDKTerrainSystem *system,
    LDKAssetManager *assets, const LDKTerrainComponent *component)
{
  (void)system;
  const LDKAssetImage image = s_runtime.heightmap_asset;
  if (component && component->heightmap)
  {
    LDKImageInfo embedded = {0};
    if (!ldk_image_get_info(component->heightmap, &embedded) ||
        embedded.width != s_runtime.config.width ||
        embedded.height != s_runtime.config.height) return false;
    /* Deep copy: an editor brush stroke replaces and frees the old image,
     * while runtime height queries may still use this cached source. */
    LDKImage *owned = ldk_image_create_format(
        embedded.width, embedded.height, embedded.format, embedded.pixels);
    LDKImageInfo stable = {0};
    if (!owned || !ldk_image_get_info(owned, &stable))
    {
      ldk_image_destroy(owned);
      return false;
    }
    LDKImage *previous = s_runtime.source_owned;
    s_runtime.source_owned = owned;
    s_runtime.source = stable;
    s_runtime.source_asset = ldk_asset_image_null();
    s_runtime.source_revision = component->heightmap_revision;
    ldk_image_destroy(previous);
    return true;
  }
  const bool has_image = !x_handle_is_null(image.h);
  if (!has_image && s_runtime.config.heightmap_path.length)
    return false;
  if (!has_image)
  {
    ldk_image_destroy(s_runtime.source_owned);
    s_runtime.source_owned = NULL;
    memset(&s_runtime.source, 0, sizeof(s_runtime.source));
    s_runtime.source.width = s_runtime.config.width;
    s_runtime.source.height = s_runtime.config.height;
    s_runtime.source_asset = ldk_asset_image_null();
    s_runtime.source_revision = component ? component->heightmap_revision : 0u;
    return true;
  }
  const LDKAssetImageData *data =
      ldk_asset_manager_image_get_const(assets, image);
  LDKImageInfo source = {0};
  if (!data || !data->image || data->is_missing ||
      !ldk_image_get_info(data->image, &source) ||
      source.channel_count < 4u ||
      (source.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
       source.format != LDK_IMAGE_FORMAT_RGBA16_UNORM) ||
      !source.width || !source.height)
  {
    return false;
  }
  /* Procedural heightmaps may specify a different grid than the default
   * component. Match their dimensions without changing component data. */
  s_runtime.config.width = source.width;
  s_runtime.config.height = source.height;
  LDKAssetHandle generic = {image.h};
  const LDKAssetInfo *info = ldk_asset_get_info_const(assets, generic);
  ldk_image_destroy(s_runtime.source_owned);
  s_runtime.source_owned = NULL;
  s_runtime.source = source;
  s_runtime.source_asset = image;
  s_runtime.source_revision = info ? info->source_revision : 0u;
  return true;
}

static float s_texel_channel(u32 x, u32 y, LDKTerrainChannel channel)
{
  const LDKImageInfo *info = &s_runtime.source;
  if (!info->pixels || x >= info->width || y >= info->height ||
      channel > LDK_TERRAIN_CHANNEL_A)
    return 0.0f;

  size_t index = ((size_t)y * info->width + x) * info->channel_count +
      (size_t)channel;
  if (info->format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
    return (float)((const u16 *)info->pixels)[index] * (1.0f / 65535.0f);
  return (float)info->pixels[index] * (1.0f / 255.0f);
}

static float s_vertex_normalized_height(
    const LDKTerrainSystem *system, u32 vx, u32 vy)
{
  (void)system;
  float total = 0.0f;
  u32 count = 0u;
  for (i32 oy = -1; oy <= 0; ++oy)
  {
    i64 y = (i64)vy + oy;
    if (y < 0 || y >= (i64)s_runtime.source.height)
      continue;
    for (i32 ox = -1; ox <= 0; ++ox)
    {
      i64 x = (i64)vx + ox;
      if (x < 0 || x >= (i64)s_runtime.source.width)
        continue;
      total += s_texel_channel((u32)x, (u32)y, s_runtime.config.height_channel);
      ++count;
    }
  }
  return count ? total / (float)count : 0.0f;
}

static float s_vertex_world_height(
    const LDKTerrainSystem *system, u32 x, u32 y)
{
  return s_runtime.config.height_offset +
      s_vertex_normalized_height(system, x, y) * s_runtime.config.height_scale;
}

static void s_world_origin(
    const LDKTerrainSystem *system, float *out_x, float *out_z)
{
  (void)system;
  float x = s_runtime.config.origin_x;
  float z = s_runtime.config.origin_z;
  if (s_runtime.config.center_on_origin)
  {
    x -= (float)s_runtime.source.width * s_runtime.config.cell_size * 0.5f;
    z -= (float)s_runtime.source.height * s_runtime.config.cell_size * 0.5f;
  }
  *out_x = x;
  *out_z = z;
}

/* Map the rendered mesh grid onto the original heightmap grid. The final
 * cell can be shorter when the source dimensions are not divisible by step.
 * This preserves the original terrain extents and keeps chunk edges aligned. */
static u32 s_mesh_source_index(u32 mesh_index, u32 step, u32 source_limit)
{
  u64 index = (u64)mesh_index * (u64)step;
  return index >= (u64)source_limit ? source_limit : (u32)index;
}

static u32 s_mesh_cell_count(u32 source_cells, u32 step)
{
  return source_cells / step + (source_cells % step != 0u ? 1u : 0u);
}

static Vec3 s_vertex_normal(const LDKTerrainSystem *system,
    u32 mesh_x, u32 mesh_y, u32 mesh_cells_x, u32 mesh_cells_y)
{
  u32 left  = mesh_x > 0u ? mesh_x - 1u : mesh_x;
  u32 right = mesh_x < mesh_cells_x ? mesh_x + 1u : mesh_x;
  u32 down  = mesh_y > 0u ? mesh_y - 1u : mesh_y;
  u32 up    = mesh_y < mesh_cells_y ? mesh_y + 1u : mesh_y;

  u32 step = s_runtime.mesh_sample_step;
  u32 sx = s_mesh_source_index(mesh_x, step, s_runtime.source.width);
  u32 sy = s_mesh_source_index(mesh_y, step, s_runtime.source.height);
  u32 sx0 = s_mesh_source_index(left, step, s_runtime.source.width);
  u32 sx1 = s_mesh_source_index(right, step, s_runtime.source.width);
  u32 sy0 = s_mesh_source_index(down, step, s_runtime.source.height);
  u32 sy1 = s_mesh_source_index(up, step, s_runtime.source.height);

  float h0 = s_vertex_world_height(system, sx0, sy);
  float h1 = s_vertex_world_height(system, sx1, sy);
  float z0 = s_vertex_world_height(system, sx, sy0);
  float z1 = s_vertex_world_height(system, sx, sy1);
  float dx = (float)(sx1 - sx0) * s_runtime.config.cell_size;
  float dz = (float)(sy1 - sy0) * s_runtime.config.cell_size;
  Vec3 n = vec3_make(dx > 0.0f ? -(h1 - h0) / dx : 0.0f,
      1.0f, dz > 0.0f ? -(z1 - z0) / dz : 0.0f);
  float len = sqrtf(n.x*n.x + n.y*n.y + n.z*n.z);
  return len > 1e-8f ? vec3_mul(n, 1.0f / len) : vec3_make(0, 1, 0);
}


static void s_chunks_destroy(LDKRenderer *renderer)
{
  if (renderer)
  {
    for (u32 i = 0u; i < s_runtime.chunk_count; ++i)
    {
      if (ldk_renderer_mesh_is_valid(renderer, s_runtime.chunks[i].mesh))
        ldk_renderer_mesh_destroy(renderer, s_runtime.chunks[i].mesh);
    }
  }
  free(s_runtime.chunks);
  s_runtime.chunks = NULL;
  s_runtime.chunk_capacity = 0u;
  s_runtime.chunk_count = 0u;
}

static u64 s_geometry_hash(const LDKTerrainSystem *s)
{
  u64 h = UINT64_C(14695981039346656037);
  h=s_hash_bytes(h,&s_runtime.config.height_channel,sizeof(s_runtime.config.height_channel));
  h=s_hash_bytes(h,&s_runtime.config.cell_size,sizeof(s_runtime.config.cell_size));
  h=s_hash_bytes(h,&s_runtime.config.height_scale,sizeof(s_runtime.config.height_scale));
  h=s_hash_bytes(h,&s_runtime.config.height_offset,sizeof(s_runtime.config.height_offset));
  h=s_hash_bytes(h,&s_runtime.config.center_on_origin,sizeof(s_runtime.config.center_on_origin));
  h=s_hash_bytes(h,&s_runtime.config.origin_x,sizeof(s_runtime.config.origin_x));
  h=s_hash_bytes(h,&s_runtime.config.origin_z,sizeof(s_runtime.config.origin_z));
  h=s_hash_bytes(h,&s_runtime.chunk_size,sizeof(s_runtime.chunk_size));
  h=s_hash_bytes(h,&s_runtime.mesh_sample_step,sizeof(s_runtime.mesh_sample_step));
  h=s_hash_bytes(h,&s_runtime.heightmap_asset,sizeof(s_runtime.heightmap_asset));
  /* Deliberately exclude source_revision. This hash describes the mesh's
   * topology and nonlocal vertex inputs, not the heightmap's pixel content.
   * s_terrain_visit compares source_revision separately, then updates only
   * the dirty chunks. Including it here made every brush stroke fail the
   * incremental eligibility check and destroy every terrain chunk. */
  h=s_hash_bytes(h,&s_runtime.configured_noise,sizeof(s_runtime.configured_noise));
  h=s_hash_bytes(h,&s_runtime.color_noise_revision,sizeof(s_runtime.color_noise_revision));
  h=s_hash_bytes(h,&s_runtime.config.color_noise_scale,sizeof(s_runtime.config.color_noise_scale));
  h=s_hash_bytes(h,&s_runtime.config.color_noise_strength,sizeof(s_runtime.config.color_noise_strength));
  for (u32 i = 0; i < s_runtime.config.surface_count; ++i)
  {
    rgba32 base = s_surface_color(s, i);
    rgba32 low = s_surface_noise_low(s, i);
    rgba32 high = s_surface_noise_high(s, i);
    float min_height = s_surface_min_height(s, i);
    float blend_width = s_surface_blend_width(s, i);
    bool use_slope = s_surface_use_slope(s, i);
    float min_slope = s_surface_min_slope(s, i);
    float slope_blend = s_surface_slope_blend(s, i);
    h = s_hash_bytes(h, &base, sizeof(base));
    h = s_hash_bytes(h, &low, sizeof(low));
    h = s_hash_bytes(h, &high, sizeof(high));
    h = s_hash_bytes(h, &min_height, sizeof(min_height));
    h = s_hash_bytes(h, &blend_width, sizeof(blend_width));
    h = s_hash_bytes(h, &use_slope, sizeof(use_slope));
    h = s_hash_bytes(h, &min_slope, sizeof(min_slope));
    h = s_hash_bytes(h, &slope_blend, sizeof(slope_blend));
  }
  return h;
}

static u64 s_appearance_hash(const LDKTerrainSystem *s,
    const LDKAssetTextureAtlasData *atlas)
{
  u64 h = UINT64_C(14695981039346656037);
  h=s_hash_bytes(h,&s_runtime.configured_material,sizeof(s_runtime.configured_material));
  h=s_hash_bytes(h,&s_runtime.configured_atlas,sizeof(s_runtime.configured_atlas));
  if (atlas)
  {
    h=s_hash_bytes(h,&atlas->revision,sizeof(atlas->revision));
    h=s_hash_bytes(h,&atlas->image,sizeof(atlas->image));
  }
  h=s_hash_bytes(h,&s_runtime.config.surface_count,sizeof(s_runtime.config.surface_count));
  for (u32 i=0;i<s_runtime.config.surface_count;++i)
  {
    float a=s_surface_min_height(s,i), b=s_surface_blend_width(s,i);
    float w=s_surface_texture_weight(s,i);
    rgba32 c=s_surface_color(s,i);
    u64 region=s_surface_region_id(s,i);
    Vec2 u=s_surface_uv_scale(s,i);
    bool use_slope=s_surface_use_slope(s,i);
    float min_slope=s_surface_min_slope(s,i);
    float slope_blend=s_surface_slope_blend(s,i);
    h=s_hash_bytes(h,&a,sizeof(a)); h=s_hash_bytes(h,&b,sizeof(b));
    h=s_hash_bytes(h,&w,sizeof(w)); h=s_hash_bytes(h,&c,sizeof(c));
    h=s_hash_bytes(h,&region,sizeof(region));
    h=s_hash_bytes(h,&u,sizeof(u));
    h=s_hash_bytes(h,&use_slope,sizeof(use_slope));
    h=s_hash_bytes(h,&min_slope,sizeof(min_slope));
    h=s_hash_bytes(h,&slope_blend,sizeof(slope_blend));
  }
  h=s_hash_bytes(h,&s_runtime.config.height_scale,sizeof(s_runtime.config.height_scale));
  h=s_hash_bytes(h,&s_runtime.config.height_offset,sizeof(s_runtime.config.height_offset));
  return h;
}

/* Build one replacement mesh without disturbing the existing chunk if GPU
 * allocation fails. Used for initial creation and localized brush updates. */
static bool s_chunk_mesh_build(LDKTerrainSystem *system,
    LDKRenderer *renderer, LDKTerrainChunk *chunk,
    u32 start_x, u32 start_y, u32 cells_x, u32 cells_y,
    u32 mesh_cells_x, u32 mesh_cells_y,
    float origin_x, float origin_z)
{
  const u32 step = s_runtime.mesh_sample_step;
  const u32 vw = cells_x + 1u;
  const u32 vh = cells_y + 1u;
  const u64 vc64 = (u64)vw * vh;
  const u64 ic64 = (u64)cells_x * cells_y * 6u;
  if (vc64 > UINT32_MAX || ic64 > UINT32_MAX) return false;

  const u32 vc = (u32)vc64;
  const u32 ic = (u32)ic64;
  LDKMeshVertex *vertices = calloc((size_t)vc, sizeof(*vertices));
  u32 *indices = malloc((size_t)ic * sizeof(*indices));
  if (!vertices || !indices)
  {
    free(vertices);
    free(indices);
    return false;
  }

  Vec3 bmin = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
  Vec3 bmax = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (u32 y = 0u; y < vh; ++y)
  {
    for (u32 x = 0u; x < vw; ++x)
    {
      u32 mx = start_x + x;
      u32 my = start_y + y;
      u32 sx = s_mesh_source_index(mx, step, s_runtime.source.width);
      u32 sy = s_mesh_source_index(my, step, s_runtime.source.height);
      u32 vi = y * vw + x;
      float wx = origin_x + (float)sx * s_runtime.config.cell_size;
      float wz = origin_z + (float)sy * s_runtime.config.cell_size;
      float wy = s_vertex_world_height(system, sx, sy);
      vertices[vi].position = vec3_make(wx, wy, wz);
      vertices[vi].normal = s_vertex_normal(system, mx, my,
          mesh_cells_x, mesh_cells_y);
      vertices[vi].uv = vec2_make(wx, wz);
      vertices[vi].color = LDK_RGBA32(
          s_color_noise_vertex_color(system, wx, wy, wz,
              vertices[vi].normal));
      if (wx < bmin.x) bmin.x = wx;
      if (wy < bmin.y) bmin.y = wy;
      if (wz < bmin.z) bmin.z = wz;
      if (wx > bmax.x) bmax.x = wx;
      if (wy > bmax.y) bmax.y = wy;
      if (wz > bmax.z) bmax.z = wz;
    }
  }

  u32 ii = 0u;
  for (u32 y = 0u; y < cells_y; ++y)
  {
    for (u32 x = 0u; x < cells_x; ++x)
    {
      u32 v0 = y * vw + x;
      u32 v1 = v0 + 1u;
      u32 v3 = (y + 1u) * vw + x;
      u32 v2 = v3 + 1u;
      indices[ii++] = v0; indices[ii++] = v2; indices[ii++] = v1;
      indices[ii++] = v0; indices[ii++] = v3; indices[ii++] = v2;
    }
  }

  LDKRendererMeshDesc desc = {0};
  desc.vertices = vertices;
  desc.vertex_count = vc;
  desc.indices = indices;
  desc.index_count = ic;
  /* A local brush stroke does not change this chunk's topology. Reuse its
   * existing GPU buffers instead of destroying and reallocating both on
   * every mouse movement. Full topology changes clear the chunk cache and
   * arrive here with a null mesh, so still create new resources then. */
  const bool has_mesh = ldk_renderer_mesh_is_valid(renderer, chunk->mesh);
  LDKResourceMesh mesh = has_mesh ? chunk->mesh
      : ldk_renderer_mesh_create(renderer, &desc);
  const bool uploaded = has_mesh
      ? ldk_renderer_mesh_update(renderer, mesh, &desc)
      : ldk_renderer_mesh_is_valid(renderer, mesh);
  free(vertices);
  free(indices);
  if (!uploaded) return false;

  chunk->mesh = mesh;
  chunk->bounds_min = bmin;
  chunk->bounds_max = bmax;
  chunk->source_x0 = s_mesh_source_index(
      start_x, step, s_runtime.source.width);
  chunk->source_y0 = s_mesh_source_index(
      start_y, step, s_runtime.source.height);
  chunk->source_x1 = s_mesh_source_index(
      start_x + cells_x, step, s_runtime.source.width);
  chunk->source_y1 = s_mesh_source_index(
      start_y + cells_y, step, s_runtime.source.height);
  return true;
}

static bool s_chunks_rebuild(LDKTerrainSystem *system, LDKRenderer *renderer)
{
  const u32 step = s_runtime.mesh_sample_step;
  const u32 mesh_cells_x = s_mesh_cell_count(s_runtime.source.width, step);
  const u32 mesh_cells_y = s_mesh_cell_count(s_runtime.source.height, step);
  const u32 chunks_x = 1u + (mesh_cells_x - 1u) / s_runtime.chunk_size;
  const u32 chunks_y = 1u + (mesh_cells_y - 1u) / s_runtime.chunk_size;
  const u64 total64 = (u64)chunks_x * chunks_y;
  if (!total64 || total64 > UINT32_MAX) return false;

  s_chunks_destroy(renderer);
  s_runtime.chunks = calloc((size_t)total64, sizeof(*s_runtime.chunks));
  if (!s_runtime.chunks) return false;
  s_runtime.chunk_capacity = (u32)total64;
  for (u32 i = 0u; i < (u32)total64; ++i)
    s_runtime.chunks[i].mesh = ldk_renderer_mesh_null();

  float origin_x, origin_z;
  s_world_origin(system, &origin_x, &origin_z);
  for (u32 cy = 0u; cy < chunks_y; ++cy)
  {
    for (u32 cx = 0u; cx < chunks_x; ++cx)
    {
      u32 start_x = cx * s_runtime.chunk_size;
      u32 start_y = cy * s_runtime.chunk_size;
      u32 cells_x = s_runtime.chunk_size;
      u32 cells_y = s_runtime.chunk_size;
      if (cells_x > mesh_cells_x - start_x)
        cells_x = mesh_cells_x - start_x;
      if (cells_y > mesh_cells_y - start_y)
        cells_y = mesh_cells_y - start_y;

      LDKTerrainChunk *chunk = &s_runtime.chunks[s_runtime.chunk_count];
      if (!s_chunk_mesh_build(system, renderer, chunk,
              start_x, start_y, cells_x, cells_y, mesh_cells_x,
              mesh_cells_y, origin_x, origin_z))
      {
        s_chunks_destroy(renderer);
        return false;
      }
      ++s_runtime.chunk_count;
    }
  }
  ++system->mesh_update_count;
  s_runtime.geometry_hash = s_geometry_hash(system);
  return true;
}

/* A vertex samples a few neighboring height texels for its averaged height
 * and normal. Expand the dirty rectangle by a conservative step+2 texels
 * before intersecting it with each chunk's source-coordinate bounds. */
static bool s_chunks_update_region(LDKTerrainSystem *system,
    LDKRenderer *renderer, LDKTerrainHeightmapRegion region)
{
  const u32 step = s_runtime.mesh_sample_step;
  const u32 mesh_cells_x = s_mesh_cell_count(s_runtime.source.width, step);
  const u32 mesh_cells_y = s_mesh_cell_count(s_runtime.source.height, step);
  const u32 chunks_x = 1u + (mesh_cells_x - 1u) / s_runtime.chunk_size;
  const u32 chunks_y = 1u + (mesh_cells_y - 1u) / s_runtime.chunk_size;
  if (!s_runtime.chunks ||
      (u64)chunks_x * chunks_y != s_runtime.chunk_count)
    return false;

  const u32 margin = step + 2u;
  const u32 x0 = region.x > margin ? region.x - margin : 0u;
  const u32 y0 = region.y > margin ? region.y - margin : 0u;
  const u32 xlast = region.x + region.width - 1u;
  const u32 ylast = region.y + region.height - 1u;
  const u32 x1 = (u64)xlast + margin >= s_runtime.source.width
      ? s_runtime.source.width - 1u : xlast + margin;
  const u32 y1 = (u64)ylast + margin >= s_runtime.source.height
      ? s_runtime.source.height - 1u : ylast + margin;

  float origin_x, origin_z;
  s_world_origin(system, &origin_x, &origin_z);
  const u64 perf_start = ldk_os_time_ticks_get();
  u32 updated_count = 0u;
  bool updated = false;
  for (u32 cy = 0u; cy < chunks_y; ++cy)
  {
    for (u32 cx = 0u; cx < chunks_x; ++cx)
    {
      LDKTerrainChunk *chunk = &s_runtime.chunks[cy * chunks_x + cx];
      if (chunk->source_x1 < x0 || chunk->source_x0 > x1 ||
          chunk->source_y1 < y0 || chunk->source_y0 > y1)
        continue;

      const u32 start_x = cx * s_runtime.chunk_size;
      const u32 start_y = cy * s_runtime.chunk_size;
      u32 cells_x = s_runtime.chunk_size;
      u32 cells_y = s_runtime.chunk_size;
      if (cells_x > mesh_cells_x - start_x)
        cells_x = mesh_cells_x - start_x;
      if (cells_y > mesh_cells_y - start_y)
        cells_y = mesh_cells_y - start_y;
      if (!s_chunk_mesh_build(system, renderer, chunk,
              start_x, start_y, cells_x, cells_y, mesh_cells_x,
              mesh_cells_y, origin_x, origin_z))
        return false;
      updated = true;
      ++updated_count;
    }
  }
  if (updated) ++system->mesh_update_count;
  const double elapsed = ldk_os_time_ticks_interval_get_milliseconds(
      perf_start, ldk_os_time_ticks_get());
  if (s_terrain_perf_log_slow(elapsed))
    ldk_log_warning("[TerrainPerf] sculpt mesh region: %.2f ms, "
        "chunks=%u/%u, rect=(%u,%u %ux%u)\n", elapsed,
        updated_count, s_runtime.chunk_count, region.x, region.y,
        region.width, region.height);
  return true;
}

/* Copy only edited texels into the stable source image used by height queries.
 * The source's address must not change during an incremental update. */
static bool s_source_update_region(const LDKTerrainComponent *component,
    LDKTerrainHeightmapRegion region)
{
  LDKImageInfo embedded = {0};
  LDKImageInfo updated = {0};
  if (!component || !component->heightmap || !s_runtime.source_owned ||
      !ldk_image_get_info(component->heightmap, &embedded) ||
      !ldk_image_pixels_region_set(s_runtime.source_owned, &embedded,
          region.x, region.y, region.width, region.height) ||
      !ldk_image_get_info(s_runtime.source_owned, &updated))
    return false;
  s_runtime.source = updated;
  s_runtime.source_revision = component->heightmap_revision;
  return true;
}


static void s_material_release(LDKRenderer *renderer)
{
  if (!renderer) return;
  ldk_renderer_material_destroy(renderer,s_runtime.material);
  ldk_renderer_image_release(renderer,s_runtime.texture);
  ldk_renderer_image_release(renderer,s_runtime.normal_map);
  ldk_renderer_image_release(renderer,s_runtime.specular_map);
  s_runtime.material=ldk_renderer_material_null();
  s_runtime.texture=ldk_renderer_texture_null();
  s_runtime.normal_map=ldk_renderer_texture_null();
  s_runtime.specular_map=ldk_renderer_texture_null();
  s_runtime.material_asset=ldk_asset_material_null();
  s_runtime.material_revision=0u;
}

static void s_surface_descs(const LDKTerrainSystem *system,
    const LDKAssetTextureAtlasData *atlas_data, const LDKImageInfo *image,
    LDKRendererTerrainSurfaceDesc out[LDK_TERRAIN_SURFACE_COUNT],
    u32 *out_invalid_mask)
{
  memset(out, 0, sizeof(*out) * LDK_TERRAIN_SURFACE_COUNT);
  if (out_invalid_mask) *out_invalid_mask = 0u;
  for (u32 i = 0; i < s_runtime.config.surface_count; ++i)
  {
    out[i].min_height = s_runtime.config.height_offset +
        s_surface_min_height(system, i) * s_runtime.config.height_scale;
    out[i].blend_width = s_surface_blend_width(system, i) * s_runtime.config.height_scale;
    out[i].color = s_surface_color(system, i);
    out[i].uv_scale = s_surface_uv_scale(system, i);
    out[i].atlas_rect = vec4_make(0, 0, 1, 1);
    out[i].texture_weight = 0.0f;
    out[i].use_slope = s_surface_use_slope(system, i);
    out[i].min_slope_degrees = s_surface_min_slope(system, i);
    out[i].slope_blend_degrees = s_surface_slope_blend(system, i);
    u64 id = s_surface_region_id(system, i);
    if (!id || !atlas_data || !image) continue;
    const LDKTextureAtlasRegion *region =
        ldk_texture_atlas_region_find_const(&atlas_data->atlas, id);
    float uv[4];
    if (!region || !ldk_texture_atlas_region_uv(
        region, image->width, image->height, uv))
    {
      /* Invalid/missing sprite: terrain still renders its color. */
      if (out_invalid_mask) *out_invalid_mask |= 1u << i;
      continue;
    }
    out[i].atlas_rect = vec4_make(uv[0], uv[1], uv[2], uv[3]);
    out[i].texture_weight = s_surface_texture_weight(system, i);
  }
}

static bool s_material_resolve(LDKTerrainSystem *system,
    LDKRenderer *renderer, LDKAssetManager *assets)
{
  const LDKAssetTextureAtlasData *atlas_data =
      ldk_asset_manager_texture_atlas_get_const(assets, s_runtime.configured_atlas);
  const LDKAssetImageData *image_data = atlas_data
      ? ldk_asset_manager_image_get_const(assets, atlas_data->image) : NULL;
  LDKImageInfo image = {0};
  bool has_image = image_data && image_data->image && !image_data->is_missing &&
      ldk_image_get_info(image_data->image, &image);

  const LDKAssetMaterialData *data =
      ldk_asset_manager_material_get_const(assets, s_runtime.configured_material);
  u64 revision = data ? data->revision : 0u;
  u64 hash = s_appearance_hash(system, atlas_data);
  if (ldk_renderer_material_is_valid(renderer, s_runtime.material) &&
      s_runtime.appearance_hash == hash &&
      s_asset_material_equal(s_runtime.material_asset, s_runtime.configured_material) &&
      s_runtime.material_revision == revision)
    return true;

  if (!x_handle_is_null(s_runtime.configured_atlas.h) && !atlas_data)
    ldk_log_error("Terrain texture atlas handle is invalid.\n");
  else if (atlas_data && !has_image)
    ldk_log_error("Terrain atlas image is unavailable.\n");

  LDKRendererTerrainSurfaceDesc surfaces[LDK_TERRAIN_SURFACE_COUNT];
  u32 invalid_mask = 0u;
  s_surface_descs(system, atlas_data, has_image ? &image : NULL,
      surfaces, &invalid_mask);
  for (u32 i = 0u; i < s_runtime.config.surface_count; ++i)
  {
    if (invalid_mask & (1u << i))
      ldk_log_error("Terrain surface %u: sprite ID %" PRIu64
          " not found or outside atlas image.\n", i, s_surface_region_id(system, i));
  }
  s_material_release(renderer);

  /* The material controls sampling and shading. The atlas selects only the
   * image and named regions, never filters or GPU resource parameters. */
  if (data || has_image)
  {
    LDKMaterialDesc desc;
    if (data)
      desc = data->descriptor;
    else if (!ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED, &desc))
      return false;

    if (has_image)
    {
      if (desc.type == LDK_MATERIAL_TYPE_VERTEX_COLOR ||
          desc.type == LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT)
      {
        rgba32 tint = desc.args.vertex_color.color;
        LDKMaterialType new_type = desc.type == LDK_MATERIAL_TYPE_VERTEX_COLOR
            ? LDK_MATERIAL_TYPE_TEXTURED : LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
        LDKMaterialDesc textured;
        if (!ldk_material_desc_defaults(new_type, &textured)) return false;
        textured.args.textured.color = tint;
        textured.alpha_mode = desc.alpha_mode;
        textured.alpha_cutoff = desc.alpha_cutoff;
        textured.surface = desc.surface;
        desc = textured;
      }
      desc.args.textured.texture = atlas_data->image;
    }
    if (!ldk_renderer_terrain_material_resolve(renderer, assets, &desc,
        surfaces, s_runtime.config.surface_count, &s_runtime.material, &s_runtime.texture,
        &s_runtime.normal_map, &s_runtime.specular_map))
      return false;
    s_runtime.material_asset = s_runtime.configured_material;
    s_runtime.material_revision = revision;
  }
  else
  {
    LDKRendererMaterialDesc desc = {0};
    desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
    desc.color = LDK_RGBA32(0xffffffffu);
    desc.shininess = 32.0f;
    desc.terrain = true;
    desc.terrain_surface_count = s_runtime.config.surface_count;
    memcpy(desc.terrain_surfaces, surfaces,
        (size_t)s_runtime.config.surface_count * sizeof(*surfaces));
    s_runtime.material = ldk_renderer_material_create(renderer, &desc);
    if (!ldk_renderer_material_is_valid(renderer, s_runtime.material))
      return false;
  }
  s_runtime.appearance_hash = hash;
  return true;
}

/* A Surface's paint occupies one channel in two terrain-owned RGBA8
 * textures. Updating these weights never touches terrain geometry. */
static bool s_paint_update(const LDKTerrainComponent *component,
    LDKRenderer *renderer)
{
  if (!component || !renderer || !ldk_renderer_material_is_valid(
          renderer, s_runtime.material)) return false;
  const u32 width = component->terrain.width;
  const u32 height = component->terrain.height;
  const bool present = component->surface_paint != NULL;
#if LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE == 1
  /* A/B baseline: neither sample paint in the shader nor upload its masks. */
  return ldk_renderer_terrain_material_paint_set(renderer, s_runtime.material,
      ldk_renderer_texture_null(), ldk_renderer_texture_null(),
      vec4_make(0.0f, 0.0f, 1.0f, 1.0f));
#endif
  if (!present || s_runtime.paint_width != width ||
      s_runtime.paint_height != height)
  {
    for (u32 i = 0u; i < 2u; ++i)
    {
      ldk_renderer_texture_destroy(renderer, s_runtime.paint_textures[i]);
      s_runtime.paint_textures[i] = ldk_renderer_texture_null();
    }
    s_runtime.paint_width = width;
    s_runtime.paint_height = height;
    s_runtime.paint_revision = 0u;
  }
  if (present && s_runtime.paint_revision != component->surface_paint_revision)
  {
    const u64 perf_start = ldk_os_time_ticks_get();
    bool partial = component->surface_paint_dirty_valid &&
        s_runtime.paint_revision ==
            component->surface_paint_dirty_base_revision &&
        ldk_renderer_texture_is_valid(renderer, s_runtime.paint_textures[0]) &&
        ldk_renderer_texture_is_valid(renderer, s_runtime.paint_textures[1]);
    const u32 x = partial ? component->surface_paint_dirty_region.x : 0u;
    const u32 y = partial ? component->surface_paint_dirty_region.y : 0u;
    const u32 rw = partial ? component->surface_paint_dirty_region.width : width;
    const u32 rh = partial ? component->surface_paint_dirty_region.height : height;
    if (!rw || !rh || x >= width || y >= height ||
        rw > width - x || rh > height - y) return false;
    const u64 count = (u64)rw * rh;
    if (!count || count > SIZE_MAX / 8u || count > UINT32_MAX / 4u)
      return false;
    const size_t bytes = (size_t)count * 4u;
    u8 *buffer = malloc(bytes * 2u);
    if (!buffer) return false;
    for (u32 row = 0u; row < rh; ++row)
      for (u32 col = 0u; col < rw; ++col)
      {
        const size_t dest = ((size_t)row * rw + col) * 4u;
        const size_t src = ((size_t)(y + row) * width + x + col) * 8u;
        memcpy(buffer + dest, component->surface_paint + src, 4u);
        memcpy(buffer + bytes + dest, component->surface_paint + src + 4u, 4u);
      }
    LDKRendererTextureOptions options;
    ldk_renderer_texture_options_defaults(&options);
    options.min_filter = LDK_RHI_FILTER_LINEAR;
    options.mag_filter = LDK_RHI_FILTER_LINEAR;
    options.mip_filter = LDK_RHI_FILTER_NEAREST;
    LDKRendererTextureDesc desc = {0};
    desc.width = width;
    desc.height = height;
    desc.channel_count = 4u;
    desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
    desc.pixels = buffer;
    desc.byte_count = bytes;
    desc.options = &options;
    bool ok = true;
    for (u32 i = 0u; i < 2u; ++i)
    {
      if (!ldk_renderer_texture_is_valid(renderer,
              s_runtime.paint_textures[i]))
      {
        desc.pixels = buffer + bytes * i;
        s_runtime.paint_textures[i] = ldk_renderer_texture_create(
            renderer, &desc);
        ok = ok && ldk_renderer_texture_is_valid(
            renderer, s_runtime.paint_textures[i]);
      }
      else
      {
        bool updated = partial
            ? ldk_renderer_texture_rgba8_region_update(renderer,
                s_runtime.paint_textures[i], x, y, rw, rh,
                buffer + bytes * i)
            : ldk_renderer_texture_update(renderer,
                s_runtime.paint_textures[i], buffer + bytes * i, bytes);
        ok = updated && ok;
      }
    }
    free(buffer);
    const double elapsed = ldk_os_time_ticks_interval_get_milliseconds(
        perf_start, ldk_os_time_ticks_get());
    if (s_terrain_perf_log_slow(elapsed))
      ldk_log_warning("[TerrainPerf] paint texture update: %.2f ms, "
          "path=%s, rect=(%u,%u %ux%u), mode=%d\n", elapsed,
          partial ? "region" : "full", x, y, rw, rh,
          LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE);
    if (!ok) return false;
    s_runtime.paint_revision = component->surface_paint_revision;
  }
  const float origin_x = s_runtime.config.origin_x -
      (s_runtime.config.center_on_origin
          ? (float)width * s_runtime.config.cell_size * 0.5f : 0.0f);
  const float origin_z = s_runtime.config.origin_z -
      (s_runtime.config.center_on_origin
          ? (float)height * s_runtime.config.cell_size * 0.5f : 0.0f);
  Vec4 uv = vec4_make(origin_x, origin_z,
      1.0f / (width * s_runtime.config.cell_size),
      1.0f / (height * s_runtime.config.cell_size));
#if LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE == 2
  /* Upload remains active; material does not expose maps to the shader. */
  return ldk_renderer_terrain_material_paint_set(renderer, s_runtime.material,
      ldk_renderer_texture_null(), ldk_renderer_texture_null(), uv);
#else
  return ldk_renderer_terrain_material_paint_set(renderer, s_runtime.material,
      s_runtime.paint_textures[0], s_runtime.paint_textures[1], uv);
#endif
}

/* Release one entity's resources. Must be called with its cache current. */
static void s_terrain_entry_destroy(LDKTerrainRuntime *entry,
    LDKRenderer *renderer)
{
  if (!entry) return;
  s_current = entry;
  if (renderer)
  {
    s_material_release(renderer);
    for (u32 i = 0u; i < 2u; ++i)
      ldk_renderer_texture_destroy(renderer, entry->paint_textures[i]);
    s_chunks_destroy(renderer);
  }
  else
  {
    free(entry->chunks);
  }
  ldk_image_destroy(entry->source_owned);
  free(entry);
  s_current = NULL;
}

LDK_API void ldk_terrain_system_editor_clear(void)
{
  LDKRenderer *renderer = ldk_module_get(LDK_MODULE_RENDERER);
  for (u32 i = 0u; i < s_terrain_count; ++i)
    s_terrain_entry_destroy(s_terrain_entries[i], renderer);
  free(s_terrain_entries);
  s_terrain_entries = NULL;
  s_terrain_count = s_terrain_capacity = 0u;
  s_current = NULL;
}

/* The .terrain authoring format lives in local coordinates. Render the mesh
 * with the entity's world Transform, and cull its world-space AABB per view.
 */
static void s_terrain_chunk_world_bounds(const LDKTerrainChunk *chunk,
    Mat4 world, Vec3 *minimum, Vec3 *maximum)
{
  *minimum = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
  *maximum = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (u32 i = 0u; i < 8u; ++i)
  {
    Vec3 corner = vec3_make(
        i & 1u ? chunk->bounds_max.x : chunk->bounds_min.x,
        i & 2u ? chunk->bounds_max.y : chunk->bounds_min.y,
        i & 4u ? chunk->bounds_max.z : chunk->bounds_min.z);
    Vec3 point = mat4_mul_point(world, corner);
    minimum->x = fminf(minimum->x, point.x);
    minimum->y = fminf(minimum->y, point.y);
    minimum->z = fminf(minimum->z, point.z);
    maximum->x = fmaxf(maximum->x, point.x);
    maximum->y = fmaxf(maximum->y, point.y);
    maximum->z = fmaxf(maximum->z, point.z);
  }
}

/* A heightfield must be upright for world X/Z height queries. Yaw and
 * nonuniform scale are supported. Tilted terrain still renders, but does
 * not masquerade as a single-valued heightfield to gameplay callers. */
static bool s_terrain_upright(Mat4 world)
{
  const float epsilon = 0.00001f;
  return fabsf(world.m[1]) < epsilon &&
      fabsf(world.m[4]) < epsilon &&
      fabsf(world.m[6]) < epsilon &&
      fabsf(world.m[9]) < epsilon;
}

static bool s_terrain_sample_entry(LDKTerrainRuntime *entry,
    float world_x, float world_z, float *height, Vec3 *normal)
{
  if (!entry || !entry->source.width || !entry->source.height ||
      !s_terrain_upright(entry->world)) return false;
  const Mat4 *m = &entry->world;
  float det = m->m[0] * m->m[10] - m->m[8] * m->m[2];
  if (!isfinite(det) || fabsf(det) < 0.00000001f) return false;
  float dx = world_x - m->m[12];
  float dz = world_z - m->m[14];
  float local_x = (dx * m->m[10] - dz * m->m[8]) / det;
  float local_z = (dz * m->m[0] - dx * m->m[2]) / det;
  float local_height;
  Vec3 local_normal;
  if (!ldk_terrain_surface_sample(&entry->config,
          entry->source.pixels ? &entry->source : NULL,
          local_x, local_z, &local_height,
          normal ? &local_normal : NULL)) return false;
  if (height)
    *height = mat4_mul_point(entry->world,
        vec3_make(local_x, local_height, local_z)).y;
  if (normal)
  {
    bool inverse_ok = false;
    Mat4 inverse = mat4_inverse_full(entry->world, &inverse_ok);
    if (!inverse_ok) return false;
    /* inverse-transpose linear component, column-major. */
    Vec3 transformed = vec3_make(
        inverse.m[0] * local_normal.x + inverse.m[1] * local_normal.y +
            inverse.m[2] * local_normal.z,
        inverse.m[4] * local_normal.x + inverse.m[5] * local_normal.y +
            inverse.m[6] * local_normal.z,
        inverse.m[8] * local_normal.x + inverse.m[9] * local_normal.y +
            inverse.m[10] * local_normal.z);
    *normal = vec3_norm(transformed);
  }
  return true;
}

LDK_API bool ldk_terrain_heightmap_set(LDKAssetImage image)
{
  /* Procedural input is used by terrain components without embedded samples. */
  s_direct_heightmap = image;
  s_direct_heightmap_valid = true;
  for (u32 i = 0u; i < s_terrain_count; ++i)
  {
    LDKTerrainRuntime *entry = s_terrain_entries[i];
    entry->direct_heightmap = image;
    entry->terrain_revision = UINT64_MAX;
  }
  return true;
}

LDK_API bool ldk_terrain_system_is_active(void)
{
  return s_terrain_count != 0u;
}

LDK_API u64 ldk_terrain_geometry_revision_get(void)
{
  u64 hash = UINT64_C(14695981039346656037);
  if (!s_terrain_count) return 0u;
  for (u32 i = 0u; i < s_terrain_count; ++i)
  {
    const LDKTerrainRuntime *entry = s_terrain_entries[i];
    hash = s_hash_bytes(hash, &entry->entity, sizeof(entry->entity));
    hash = s_hash_bytes(hash, &entry->geometry_hash,
        sizeof(entry->geometry_hash));
    hash = s_hash_bytes(hash, &entry->world, sizeof(entry->world));
  }
  return hash ? hash : 1u;
}

LDK_API bool ldk_terrain_entity_height_at_world(
    LDKEntity entity, float world_x, float world_z, float *height)
{
  if (!height || !isfinite(world_x) || !isfinite(world_z)) return false;
  return s_terrain_sample_entry(
      s_terrain_entry_find(entity), world_x, world_z, height, NULL);
}

LDK_API bool ldk_terrain_entity_normal_at_world(
    LDKEntity entity, float world_x, float world_z, Vec3 *normal)
{
  if (!normal || !isfinite(world_x) || !isfinite(world_z)) return false;
  return s_terrain_sample_entry(
      s_terrain_entry_find(entity), world_x, world_z, NULL, normal);
}

LDK_API bool ldk_terrain_height_at_world(
    float world_x, float world_z, float *height)
{
  if (!height || !isfinite(world_x) || !isfinite(world_z)) return false;
  bool found = false;
  float highest = -FLT_MAX;
  for (u32 i = 0u; i < s_terrain_count; ++i)
  {
    float value;
    if (s_terrain_sample_entry(s_terrain_entries[i], world_x, world_z,
            &value, NULL) && (!found || value > highest))
    {
      highest = value;
      found = true;
    }
  }
  if (found) *height = highest;
  return found;
}

LDK_API bool ldk_terrain_normal_at_world(
    float world_x, float world_z, Vec3 *normal)
{
  if (!normal || !isfinite(world_x) || !isfinite(world_z)) return false;
  bool found = false;
  float highest = -FLT_MAX;
  for (u32 i = 0u; i < s_terrain_count; ++i)
  {
    float value;
    Vec3 candidate;
    if (s_terrain_sample_entry(s_terrain_entries[i], world_x, world_z,
            &value, &candidate) && (!found || value > highest))
    {
      highest = value;
      *normal = candidate;
      found = true;
    }
  }
  return found;
}

typedef struct LDKTerrainTraversal
{
  LDKTerrainSystem *system;
  LDKRenderer *renderer;
  LDKAssetManager *assets;
} LDKTerrainTraversal;

static bool s_terrain_visit(LDKEntity entity, void *user)
{
  LDKTerrainTraversal *context = (LDKTerrainTraversal *)user;
  const LDKTerrainComponent *component = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TERRAIN);
  if (!component ||
      !ldk_ecs_component_is_enabled(entity, LDK_COMPONENT_TYPE_TERRAIN) ||
      !ldk_scenegraph_update_entity(entity)) return true;
  Mat4 world;
  if (!ldk_transform_get_world_matrix(entity, &world)) return true;
  const u64 perf_start = ldk_os_time_ticks_get();
  LDKTerrainRuntime *entry = s_terrain_entry_get(entity, context->system);
  if (!entry) return false;
  s_current = entry;
  entry->seen = true;
  entry->world = world;
  entry->chunk_size = component->chunk_size;
  entry->mesh_sample_step = component->mesh_sample_step;
  LDKTerrainSystem *system = context->system;
  LDKAssetManager *assets = context->assets;
  LDKRenderer *renderer = context->renderer;
  if (!s_config_refresh(system, assets, component) || !s_config_is_valid(system) ||
      !s_color_noise_refresh(system, assets)) return true;

  LDKAssetHandle generic = {entry->heightmap_asset.h};
  const LDKAssetInfo *info = !x_handle_is_null(generic.h)
      ? ldk_asset_get_info_const(assets, generic) : NULL;
  u64 revision = component->heightmap ? component->heightmap_revision
      : (info ? info->source_revision : component->heightmap_revision);
  bool source_changed =
      !s_asset_image_equal(entry->source_asset, entry->heightmap_asset) ||
      entry->source_revision != revision ||
      !entry->source.width || !entry->source.height ||
      ((component->heightmap || !x_handle_is_null(generic.h)) &&
          !entry->source.pixels) ||
      (x_handle_is_null(generic.h) &&
          (entry->source.width != entry->config.width ||
           entry->source.height != entry->config.height));
  bool partial_geometry = false;
  bool full_geometry = false;
  if (source_changed)
  {
    /* Incremental edit is safe only if the renderer has the exact revision
     * from before the first unconsumed brush region, and no other geometry
     * property changed. Otherwise refresh and rebuild the full terrain. */
    bool partial = component->heightmap && entry->source_owned &&
        entry->chunks && component->heightmap_dirty_valid &&
        entry->source_revision == component->heightmap_dirty_base_revision &&
        entry->geometry_hash == s_geometry_hash(system) &&
        entry->source.width == component->terrain.width &&
        entry->source.height == component->terrain.height &&
        !entry->source_asset.h.index &&
        !entry->source_asset.h.version;
    if (partial)
    {
      LDKTerrainHeightmapRegion region = component->heightmap_dirty_region;
      partial = s_source_update_region(component, region) &&
          s_chunks_update_region(system, renderer, region);
      if (partial) entry->geometry_hash = s_geometry_hash(system);
    }
    partial_geometry = partial;
    if (!partial)
    {
      if (!s_source_refresh(system, assets, component))
      {
        s_chunks_destroy(renderer);
        memset(&entry->source, 0, sizeof(entry->source));
        entry->source_asset = ldk_asset_image_null();
        return true;
      }
      entry->geometry_hash = 0u;
    }
  }
  u64 gh = s_geometry_hash(system);
  if (!entry->chunks || entry->geometry_hash != gh)
  {
    full_geometry = true;
    if (!s_chunks_rebuild(system, renderer)) return true;
  }
  if (source_changed && component->heightmap_dirty_valid)
  {
    /* This terrain has exactly one render cache. A subsequent brush starts
     * a new rectangle from the revision we just rendered. */
    LDKTerrainComponent *mutable_component = ldk_ecs_component_get(
        entity, LDK_COMPONENT_TYPE_TERRAIN);
    if (mutable_component &&
        mutable_component->heightmap_revision == entry->source_revision)
      mutable_component->heightmap_dirty_valid = false;
  }
  const u64 perf_geometry_done = ldk_os_time_ticks_get();
  if (!s_material_resolve(system, renderer, assets) ||
      !s_paint_update(component, renderer)) return true;
  const u64 perf_material_done = ldk_os_time_ticks_get();
  if (component->surface_paint_dirty_valid &&
      entry->paint_revision == component->surface_paint_revision)
  {
    LDKTerrainComponent *mutable_component = ldk_ecs_component_get(
        entity, LDK_COMPONENT_TYPE_TERRAIN);
    if (mutable_component &&
        mutable_component->surface_paint_revision == entry->paint_revision)
      mutable_component->surface_paint_dirty_valid = false;
  }
  system->has_geometry = true;
  if (!system->data_width) system->data_width = entry->source.width;
  if (!system->data_height) system->data_height = entry->source.height;
  system->chunk_count += entry->chunk_count;

  u32 flags = component->casts_shadows
      ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
      : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE;
  for (u32 view_index = 0u; view_index < renderer->view_count; ++view_index)
  {
    const LDKRendererView *view = &renderer->views[view_index];
    if (!view->submitted) continue;
    for (u32 i = 0u; i < entry->chunk_count; ++i)
    {
      const LDKTerrainChunk *chunk = &entry->chunks[i];
      Vec3 bmin, bmax;
      if (component->cull)
      {
        s_terrain_chunk_world_bounds(chunk, world, &bmin, &bmax);
        if (!ldk_renderer_view_bounds_visible(
                renderer, view->id, bmin, bmax)) continue;
      }
      if (ldk_renderer_submit_mesh_to_view_with_flags(renderer,
              view->id, chunk->mesh, entry->material, world, flags))
        system->visible_chunk_count++;
    }
  }
  const u64 perf_end = ldk_os_time_ticks_get();
  const double elapsed = ldk_os_time_ticks_interval_get_milliseconds(
      perf_start, perf_end);
  if (s_terrain_perf_log_slow(elapsed))
    ldk_log_warning("[TerrainPerf] terrain visit: %.2f ms, "
        "config+height+mesh=%.2f ms, material+paint=%.2f ms, "
        "submit=%.2f ms, source=%u, partial_mesh=%u, "
        "full_mesh=%u, paint_revision=%llu, mode=%d\n", elapsed,
        ldk_os_time_ticks_interval_get_milliseconds(perf_start,
            perf_geometry_done),
        ldk_os_time_ticks_interval_get_milliseconds(perf_geometry_done,
            perf_material_done),
        ldk_os_time_ticks_interval_get_milliseconds(perf_material_done,
            perf_end),
        source_changed ? 1u : 0u, partial_geometry ? 1u : 0u,
        full_geometry ? 1u : 0u,
        (unsigned long long)component->surface_paint_revision,
        LDK_TERRAIN_DIAGNOSTIC_PAINT_MODE);
  return true;
}

static void s_terrain_process(LDKTerrainSystem *system)
{
  if (!system) return;
  LDKRenderer *renderer = ldk_module_get(LDK_MODULE_RENDERER);
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!renderer || !assets || !renderer->is_initialized) return;
  system->data_width = system->data_height = 0u;
  system->chunk_count = system->visible_chunk_count = 0u;
  system->has_geometry = false;
  for (u32 i = 0u; i < s_terrain_count; ++i)
    s_terrain_entries[i]->seen = false;
  LDKTerrainTraversal traversal = {system, renderer, assets};
  (void)ldk_ecs_entity_foreach(s_terrain_visit, &traversal);
  /* Delete cache entries for deleted/disabled entities, retaining resources
   * of unrelated game entities and all other valid terrain components. */
  for (u32 i = 0u; i < s_terrain_count; )
  {
    if (s_terrain_entries[i]->seen) {++i; continue;}
    s_terrain_entry_destroy(s_terrain_entries[i], renderer);
    s_terrain_entries[i] = s_terrain_entries[--s_terrain_count];
  }
  s_current = NULL;
}

int ldk_terrain_system_initialize(void *data)
{
  LDKTerrainSystem *system = (LDKTerrainSystem *)data;
  if (!system || s_play_owner) return -1;
  ldk_terrain_system_editor_clear();
  s_play_owner = system;
  system->mesh_update_count = 0u;
  return 0;
}

void ldk_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  (void)group;
  (void)dt;
  LDKTerrainSystem *system = (LDKTerrainSystem *)data;
  if (system && system == s_play_owner) s_terrain_process(system);
}

void ldk_terrain_system_on_editor_update(u64 system_id, void *data, float dt)
{
  (void)system_id;
  (void)dt;
  if (!s_play_owner) s_terrain_process((LDKTerrainSystem *)data);
}

void ldk_terrain_system_terminate(void *data)
{
  LDKTerrainSystem *system = (LDKTerrainSystem *)data;
  if (!system || s_play_owner != system) return;
  ldk_terrain_system_editor_clear();
  s_play_owner = NULL;
  system->chunk_count = system->visible_chunk_count = 0u;
  system->has_geometry = false;
}
