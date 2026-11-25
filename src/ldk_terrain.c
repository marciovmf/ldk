#include <ldk_terrain.h>

#include <module/ldk_asset_source.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_tml.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_path_valid(const LDKAssetPath *path);

static bool s_terrain_grass_type_valid(const LDKTerrainVegetationLayer *layer)
{
  const LDKGrassTypeDesc *type = &layer->type;
  if (type->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    if (!s_path_valid(&type->mesh_path) ||
        (type->kind > LDK_GRASS_LAYER_TREE) ||
        !s_path_valid(&type->material_paths[0]) ||
        !isfinite(type->scale_min.x) ||
        !isfinite(type->scale_min.y) ||
        !isfinite(type->scale_min.z) ||
        !isfinite(type->scale_max.x) ||
        !isfinite(type->scale_max.y) ||
        !isfinite(type->scale_max.z) ||
        !isfinite(type->rotation_min.x) ||
        !isfinite(type->rotation_min.y) ||
        !isfinite(type->rotation_min.z) ||
        !isfinite(type->rotation_max.x) ||
        !isfinite(type->rotation_max.y) ||
        !isfinite(type->rotation_max.z) ||
        type->scale_min.x <= 0.0f ||
        type->scale_min.y <= 0.0f ||
        type->scale_min.z <= 0.0f ||
        type->scale_max.x < type->scale_min.x ||
        type->scale_max.y < type->scale_min.y ||
        type->scale_max.z < type->scale_min.z ||
        type->rotation_max.x < type->rotation_min.x ||
        type->rotation_max.y < type->rotation_min.y ||
        type->rotation_max.z < type->rotation_min.z)
      return false;
  }
  for (u32 slot = 0u; slot < LDK_GRASS_MESH_MATERIAL_SLOT_COUNT; ++slot)
    if (!s_path_valid(&type->material_paths[slot])) return false;
  return x_smallstr_length(&type->name) > 0u &&
      type->blade_preset >= LDK_GRASS_BLADE_PRESET_THIN &&
      type->blade_preset <= LDK_GRASS_BLADE_PRESET_CUSTOM_7 &&
      isfinite(type->density) && type->density >= 0.0f &&
      isfinite(type->clumping) && type->clumping >= 0.0f &&
      type->clumping <= 1.0f &&
      isfinite(type->clump_radius) && type->clump_radius >= 0.0f &&
      isfinite(type->min_height) && type->min_height > 0.0f &&
      isfinite(type->max_height) &&
      type->max_height >= type->min_height &&
      isfinite(type->blade_width) && type->blade_width > 0.0f &&
      isfinite(type->curvature) && type->curvature >= 0.0f &&
      isfinite(type->wind_strength) && type->wind_strength >= 0.0f &&
      isfinite(type->interaction_recovery_time) &&
      type->interaction_recovery_time >= 0.0f &&
      type->interaction_recovery_time <= 16.0f;
}

static bool s_path_valid(const LDKAssetPath *path)
{
  LDKAssetPath normalized;
  if (!path || path->length > LDK_ASSET_PATH_MAX_LENGTH ||
      path->buf[path->length] != '\0')
  {
    return false;
  }
  if (!path->length)
  {
    return path->buf[0] == '\0';
  }
  return ldk_asset_path_set(&normalized, path->buf) &&
      normalized.length == path->length &&
      strcmp(normalized.buf, path->buf) == 0;
}

static bool s_optional_path_set(LDKAssetPath *dst, const char *path)
{
  LDKAssetPath candidate = {0};
  if (!dst || !path || (path[0] && !ldk_asset_path_set(&candidate, path)))
  {
    return false;
  }
  *dst = candidate;
  return true;
}

void ldk_terrain_init(LDKTerrain *terrain)
{
  if (!terrain)
  {
    return;
  }
  memset(terrain, 0, sizeof(*terrain));
  terrain->width = 64u;
  terrain->height = 64u;
  terrain->height_channel = LDK_TERRAIN_CHANNEL_R;
  terrain->cell_size = 1.0f;
  terrain->height_scale = 1.0f;
  terrain->center_on_origin = true;
  terrain->color_noise_scale = 1.0f;
  terrain->surface_count = 1u;
  terrain->next_vegetation_layer_id = 1u;
  for (u32 i = 0u; i < LDK_TERRAIN_SURFACE_COUNT; ++i)
  {
    LDKTerrainSurface *surface = &terrain->surfaces[i];
    snprintf(surface->name, sizeof(surface->name), "Surface %u", i);
    surface->color = LDK_RGBA32(0xffffffffu);
    surface->noise_low_color = surface->color;
    surface->noise_high_color = surface->color;
    surface->uv_scale = vec2_make(1.0f, 1.0f);
  }
}

bool ldk_terrain_validate(const LDKTerrain *terrain)
{
  if (!terrain || !s_path_valid(&terrain->heightmap_path) ||
      !s_path_valid(&terrain->material_path) ||
      !s_path_valid(&terrain->texture_atlas_path) ||
      !s_path_valid(&terrain->color_noise_path) ||
      terrain->height_channel > LDK_TERRAIN_CHANNEL_A ||
      !terrain->width || !terrain->height ||
      !isfinite(terrain->cell_size) || terrain->cell_size <= 0.0f ||
      !isfinite(terrain->height_scale) || terrain->height_scale <= 0.0f ||
      !isfinite(terrain->height_offset) ||
      !isfinite(terrain->origin_x) || !isfinite(terrain->origin_z) ||
      !isfinite(terrain->color_noise_scale) ||
      !isfinite(terrain->color_noise_strength) ||
      terrain->color_noise_strength < 0.0f ||
      terrain->color_noise_strength > 1.0f ||
      (terrain->color_noise_path.length &&
          terrain->color_noise_scale <= 0.0f) ||
      !terrain->surface_count ||
      terrain->surface_count > LDK_TERRAIN_SURFACE_COUNT ||
      terrain->vegetation_layer_count > LDK_TERRAIN_VEGETATION_LAYER_COUNT ||
      !terrain->next_vegetation_layer_id ||
      terrain->next_vegetation_layer_id > INT64_MAX)
  {
    return false;
  }

  float previous = 0.0f;
  for (u32 i = 0u; i < terrain->surface_count; ++i)
  {
    const LDKTerrainSurface *surface = &terrain->surfaces[i];
    if (!memchr(surface->name, '\0', sizeof(surface->name)) ||
        !s_path_valid(&surface->paint_mask_path) ||
        !isfinite(surface->min_height) ||
        surface->min_height < 0.0f || surface->min_height > 1.0f ||
        (!surface->use_slope && surface->min_height < previous) ||
        !isfinite(surface->blend_width) ||
        surface->blend_width < 0.0f || surface->blend_width > 1.0f ||
        surface->sprite_id > INT64_MAX ||
        !isfinite(surface->uv_scale.x) ||
        !isfinite(surface->uv_scale.y) ||
        !isfinite(surface->texture_weight) ||
        surface->texture_weight < 0.0f ||
        surface->texture_weight > 1.0f ||
        !isfinite(surface->min_slope_degrees) ||
        surface->min_slope_degrees < 0.0f ||
        surface->min_slope_degrees > 90.0f ||
        !isfinite(surface->slope_blend_degrees) ||
        surface->slope_blend_degrees < 0.0f ||
        surface->slope_blend_degrees > 90.0f ||
        (i == 0u && surface->use_slope))
    {
      return false;
    }
    if (!surface->use_slope)
    {
      previous = surface->min_height;
    }
  }

  for (u32 i = 0u; i < terrain->vegetation_layer_count; ++i)
  {
    const LDKTerrainVegetationLayer *layer =
        &terrain->vegetation_layers[i];
    if (!layer->id || layer->id >= terrain->next_vegetation_layer_id ||
        !isfinite(layer->density_scale) || layer->density_scale < 0.0f ||
        !s_terrain_grass_type_valid(layer))
    {
      return false;
    }
    for (u32 j = 0u; j < i; ++j)
    {
      if (layer->id == terrain->vegetation_layers[j].id)
      {
        return false;
      }
    }
  }
  return true;
}

/* Heightfield sampling is shared by TerrainSystem queries and terrain-linked
 * vegetation. Terrain vertices use the average of up to four neighboring
 * source pixels; mesh triangles split along tx == tz within each cell. */
static float s_height_texel(const LDKTerrain *terrain,
    const LDKImageInfo *source, u32 x, u32 z)
{
  if (!source || !source->pixels || x >= source->width || z >= source->height)
  {
    return 0.0f;
  }
  size_t index = ((size_t)z * source->width + x) * 4u +
      (u32)terrain->height_channel;
  if (source->format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
  {
    u16 v;
    memcpy(&v, source->pixels + index * sizeof(v), sizeof(v));
    return (float)v * (1.0f / 65535.0f);
  }
  return (float)source->pixels[index] * (1.0f / 255.0f);
}

static float s_height_vertex(const LDKTerrain *terrain,
    const LDKImageInfo *source, u32 x, u32 z)
{
  float value = 0.0f;
  u32 count = 0u;
  if (source && source->pixels)
  {
    for (i32 dz = -1; dz <= 0; ++dz)
    {
      i64 zz = (i64)z + dz;
      if (zz < 0 || zz >= (i64)source->height)
      {
        continue;
      }
      for (i32 dx = -1; dx <= 0; ++dx)
      {
        i64 xx = (i64)x + dx;
        if (xx < 0 || xx >= (i64)source->width)
        {
          continue;
        }
        value += s_height_texel(terrain, source, (u32)xx, (u32)zz);
        ++count;
      }
    }
  }
  return terrain->height_offset +
      (count ? value / (float)count : 0.0f) * terrain->height_scale;
}

bool ldk_terrain_heightmap_range_expand(LDKTerrain *terrain,
    u16 *rgba16_pixels, float required_min, float required_max)
{
  if (!terrain || !rgba16_pixels || !ldk_terrain_validate(terrain) ||
      !isfinite(required_min) || !isfinite(required_max) ||
      required_min > required_max ||
      (size_t)terrain->width > SIZE_MAX / (size_t)terrain->height / 4u)
  {
    return false;
  }

  const double old_offset = terrain->height_offset;
  const double old_scale = terrain->height_scale;
  double low = old_offset;
  double high = old_offset + old_scale;
  if ((double)required_min >= low && (double)required_max <= high)
  {
    return true;
  }

  /* Double the available interval in the direction that is exhausted.
   * Avoid re-quantizing the entire image on every subsequent brush frame. */
  for (u32 attempt = 0u; attempt < 128u &&
      ((double)required_min < low || (double)required_max > high);
      ++attempt)
  {
    const double span = high - low;
    if ((double)required_min < low)
    {
      low -= span;
    }
    else
    {
      high += span;
    }
  }
  if (!isfinite(low) || !isfinite(high) ||
      (double)required_min < low || (double)required_max > high ||
      high - low > FLT_MAX || low < -FLT_MAX || low > FLT_MAX)
  {
    return false;
  }

  LDKTerrain updated = *terrain;
  updated.height_offset = (float)low;
  updated.height_scale = (float)(high - low);
  const double new_offset = updated.height_offset;
  const double new_scale = updated.height_scale;
  if (!isfinite(new_scale) || new_scale <= 0.0 ||
      (double)required_min < new_offset ||
      (double)required_max > new_offset + new_scale)
  {
    return false;
  }

  /* Surface thresholds are stored as fractions of the height interval.
   * Adjust them to keep their world-space elevations unchanged. Slope-based
   * surface definitions use degrees and must not be changed. */
  for (u32 i = 1u; i < updated.surface_count; ++i)
  {
    if (updated.surfaces[i].use_slope)
    {
      continue;
    }
    const double threshold = old_offset + old_scale *
        terrain->surfaces[i].min_height;
    const double blend = old_scale * terrain->surfaces[i].blend_width;
    updated.surfaces[i].min_height = (float)fmax(0.0, fmin(1.0,
        (threshold - new_offset) / new_scale));
    updated.surfaces[i].blend_width = (float)fmax(0.0, fmin(1.0,
        blend / new_scale));
  }
  if (!ldk_terrain_validate(&updated))
  {
    return false;
  }

  const size_t pixel_count = (size_t)terrain->width * terrain->height;
  for (size_t i = 0u; i < pixel_count; ++i)
  {
    size_t at = i * 4u + (u32)terrain->height_channel;
    const double old_value = (double)rgba16_pixels[at] / 65535.0;
    const double world_height = old_offset + old_scale * old_value;
    const double new_value = fmax(0.0, fmin(1.0,
        (world_height - new_offset) / new_scale));
    rgba16_pixels[at] = (u16)lround(new_value * 65535.0);
  }
  *terrain = updated;
  return true;
}

bool ldk_terrain_surface_sample(const LDKTerrain *terrain,
    const LDKImageInfo *source, float x, float z,
    float *out_height, Vec3 *out_normal)
{
  if (!terrain || (!out_height && !out_normal) ||
      !isfinite(x) || !isfinite(z) ||
      !terrain->width || !terrain->height ||
      !isfinite(terrain->cell_size) || terrain->cell_size <= 0.0f ||
      !isfinite(terrain->height_scale) || !isfinite(terrain->height_offset) ||
      !isfinite(terrain->origin_x) || !isfinite(terrain->origin_z) ||
      (source && (source->width != terrain->width ||
          source->height != terrain->height || !source->pixels ||
          source->channel_count != 4u ||
          (source->format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
           source->format != LDK_IMAGE_FORMAT_RGBA16_UNORM))))
  {
    return false;
  }

  float ox = terrain->origin_x;
  float oz = terrain->origin_z;
  if (terrain->center_on_origin)
  {
    ox -= (float)terrain->width * terrain->cell_size * 0.5f;
    oz -= (float)terrain->height * terrain->cell_size * 0.5f;
  }
  float lx = (x - ox) / terrain->cell_size;
  float lz = (z - oz) / terrain->cell_size;
  if (lx < 0.0f || lz < 0.0f ||
      lx > (float)terrain->width || lz > (float)terrain->height)
  {
    return false;
  }
  u32 ix = (u32)floorf(lx);
  u32 iz = (u32)floorf(lz);
  float tx = lx - (float)ix;
  float tz = lz - (float)iz;
  if (ix >= terrain->width) { ix = terrain->width - 1u; tx = 1.0f; }
  if (iz >= terrain->height) { iz = terrain->height - 1u; tz = 1.0f; }

  float h00 = s_height_vertex(terrain, source, ix, iz);
  float h10 = s_height_vertex(terrain, source, ix + 1u, iz);
  float h01 = s_height_vertex(terrain, source, ix, iz + 1u);
  float h11 = s_height_vertex(terrain, source, ix + 1u, iz + 1u);
  if (out_height)
  {
    *out_height = tx >= tz
        ? h00 + tx * (h10 - h00) + tz * (h11 - h10)
        : h00 + tz * (h01 - h00) + tx * (h11 - h01);
  }
  if (out_normal)
  {
    float inv = 1.0f / terrain->cell_size;
    float dx = (tx >= tz ? h10 - h00 : h11 - h01) * inv;
    float dz = (tx >= tz ? h11 - h10 : h01 - h00) * inv;
    Vec3 n = vec3_make(-dx, 1.0f, -dz);
    float len = vec3_len(n);
    *out_normal = len > 1e-8f
        ? vec3_mul(n, 1.0f / len) : vec3_make(0.0f, 1.0f, 0.0f);
  }
  return true;
}

bool ldk_terrain_copy(LDKTerrain *dst, const LDKTerrain *src)
{
  if (!dst || !ldk_terrain_validate(src))
  {
    return false;
  }
  *dst = *src;
  return true;
}

bool ldk_terrain_import_heightmap(LDKTerrain *terrain,
    const char *image_path, const LDKImage *image,
    LDKTerrainChannel channel)
{
  LDKImageInfo info;
  LDKTerrain candidate;
  if (!terrain || !image ||
      channel > LDK_TERRAIN_CHANNEL_A ||
      !ldk_image_get_info(image, &info) ||
      !info.width || !info.height || !info.pixels ||
      info.channel_count != 4u ||
      (info.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
          info.format != LDK_IMAGE_FORMAT_RGBA16_UNORM))
  {
    return false;
  }
  ldk_terrain_init(&candidate);
  if (!s_optional_path_set(&candidate.heightmap_path,
          image_path ? image_path : ""))
  {
    return false;
  }
  candidate.width = info.width;
  candidate.height = info.height;
  candidate.height_channel = channel;
  if (!ldk_terrain_validate(&candidate))
  {
    return false;
  }
  *terrain = candidate;
  return true;
}

/* Integrate source pixels over each destination pixel's footprint.
 * Averaging the complete footprint avoids aliasing when a large heightmap
 * is reduced to a much smaller editable grid. The output is RGBA16 to give
 * the sculpt brush sub-8-bit increments even for an RGBA8 input. */
LDKImage *ldk_terrain_heightmap_resample(
    const LDKImage *source, u32 target_width, u32 target_height)
{
  LDKImageInfo src;
  u16 *pixels;
  LDKImage *result;
  size_t count;
  if (!source || !target_width || !target_height ||
      !ldk_image_get_info(source, &src) || !src.pixels ||
      !src.width || !src.height || src.channel_count != 4u ||
      (src.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
       src.format != LDK_IMAGE_FORMAT_RGBA16_UNORM) ||
      (u64)target_width * target_height > SIZE_MAX / (4u * sizeof(u16)))
    return NULL;
  count = (size_t)target_width * target_height * 4u;
  pixels = malloc(count * sizeof(u16));
  if (!pixels) return NULL;
  for (u32 y = 0u; y < target_height; ++y)
  {
    double y0 = (double)y * src.height / target_height;
    double y1 = (double)(y + 1u) * src.height / target_height;
    u32 sy0 = (u32)y0;
    u32 sy1 = (u32)ceil(y1);
    if (sy1 > src.height) sy1 = src.height;
    for (u32 x = 0u; x < target_width; ++x)
    {
      double x0 = (double)x * src.width / target_width;
      double x1 = (double)(x + 1u) * src.width / target_width;
      u32 sx0 = (u32)x0;
      u32 sx1 = (u32)ceil(x1);
      if (sx1 > src.width) sx1 = src.width;
      double sums[4] = {0.0, 0.0, 0.0, 0.0};
      double area = (x1 - x0) * (y1 - y0);
      for (u32 sy = sy0; sy < sy1; ++sy)
      {
        double dy = fmin(y1, (double)sy + 1.0) -
            fmax(y0, (double)sy);
        if (dy <= 0.0) continue;
        for (u32 sx = sx0; sx < sx1; ++sx)
        {
          double dx = fmin(x1, (double)sx + 1.0) -
              fmax(x0, (double)sx);
          if (dx <= 0.0) continue;
          size_t index = ((size_t)sy * src.width + sx) * 4u;
          for (u32 c = 0u; c < 4u; ++c)
          {
            u16 value;
            if (src.format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
              memcpy(&value, src.pixels + (index + c) * sizeof(u16),
                  sizeof(value));
            else
              value = (u16)src.pixels[index + c] * 257u;
            sums[c] += (double)value * dx * dy;
          }
        }
      }
      size_t dst = ((size_t)y * target_width + x) * 4u;
      for (u32 c = 0u; c < 4u; ++c)
      {
        double normalized = fmin(65535.0,
            fmax(0.0, sums[c] / area));
        pixels[dst + c] = (u16)floor(normalized + 0.5);
      }
    }
  }
  result = ldk_image_create_format(target_width, target_height,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  free(pixels);
  return result;
}

bool ldk_terrain_vegetation_layer_add(LDKTerrain *terrain,
    const LDKGrassTypeDesc *type, float density_scale,
    u32 seed, u64 *out_id)
{
  LDKTerrainVegetationLayer layer = {0};
  if (!terrain || !ldk_terrain_validate(terrain) || !type ||
      terrain->vegetation_layer_count >=
          LDK_TERRAIN_VEGETATION_LAYER_COUNT ||
      terrain->next_vegetation_layer_id == INT64_MAX ||
      !isfinite(density_scale) || density_scale < 0.0f)
  {
    return false;
  }
  layer.id = terrain->next_vegetation_layer_id;
  layer.type = *type;
  layer.seed = seed;
  layer.density_scale = density_scale;
  if (!s_terrain_grass_type_valid(&layer)) return false;
  terrain->vegetation_layers[terrain->vegetation_layer_count++] = layer;
  ++terrain->next_vegetation_layer_id;
  if (out_id) *out_id = layer.id;
  return true;
}

bool ldk_terrain_vegetation_layer_remove(
    LDKTerrain *terrain, u64 layer_id)
{
  if (!terrain || !ldk_terrain_validate(terrain) || !layer_id)
  {
    return false;
  }
  for (u32 i = 0u; i < terrain->vegetation_layer_count; ++i)
  {
    if (terrain->vegetation_layers[i].id == layer_id)
    {
      const u32 remaining = terrain->vegetation_layer_count - i - 1u;
      memmove(&terrain->vegetation_layers[i],
          &terrain->vegetation_layers[i + 1u],
          remaining * sizeof(terrain->vegetation_layers[0]));
      --terrain->vegetation_layer_count;
      memset(&terrain->vegetation_layers[terrain->vegetation_layer_count],
          0, sizeof(terrain->vegetation_layers[0]));
      return true;
    }
  }
  return false;
}

static void s_write_string(XStrBuilder *out, const char *text)
{
  x_strbuilder_append_char(out, '"');
  for (; *text; ++text)
  {
    switch (*text)
    {
    case '\\': x_strbuilder_append(out, "\\\\"); break;
    case '"': x_strbuilder_append(out, "\\\""); break;
    case '\n': x_strbuilder_append(out, "\\n"); break;
    case '\r': x_strbuilder_append(out, "\\r"); break;
    case '\t': x_strbuilder_append(out, "\\t"); break;
    default: x_strbuilder_append_char(out, *text); break;
    }
  }
  x_strbuilder_append_char(out, '"');
}

/* TML distinguishes integer and floating-point tokens. Force a decimal point
 * for integral-valued floats, including arrays, without losing precision. */
static void s_write_float(XStrBuilder *out, float value)
{
  char buffer[48];
  snprintf(buffer, sizeof(buffer), "%.9g", (double)value);
  x_strbuilder_append(out, buffer);
  if (!strpbrk(buffer, ".eE"))
  {
    x_strbuilder_append(out, ".0");
  }
}

static void s_write_float_entry(
    XStrBuilder *out, const char *key, float value)
{
  x_strbuilder_append(out, key);
  s_write_float(out, value);
  x_strbuilder_append_char(out, '\n');
}

static void s_write_vec3_entry(
    XStrBuilder *out, const char *key, Vec3 value)
{
  x_strbuilder_append(out, key);
  s_write_float(out, value.x);
  x_strbuilder_append(out, ", ");
  s_write_float(out, value.y);
  x_strbuilder_append(out, ", ");
  s_write_float(out, value.z);
  x_strbuilder_append_char(out, '\n');
}

static void s_write_string_entry(
    XStrBuilder *out, const char *key, const char *value)
{
  x_strbuilder_append(out, key);
  s_write_string(out, value);
  x_strbuilder_append_char(out, '\n');
}

/* Base64 holds the logical heightfield, not a PNG representation. The TML
 * remains a self-contained, text-based asset and is platform independent.
 * Each encoded sample is u16 little endian regardless of host endianness. */
static const char s_terrain_base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int s_terrain_base64_value(char ch)
{
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9') return ch - '0' + 52;
  if (ch == '+') return 62;
  if (ch == '/') return 63;
  return -1;
}

static bool s_terrain_heightmap_valid(const LDKTerrain *terrain,
    const LDKImageInfo *image)
{
  return image && image->pixels && image->width == terrain->width &&
      image->height == terrain->height && image->channel_count == 4u &&
      (image->format == LDK_IMAGE_FORMAT_RGBA16_UNORM ||
       image->format == LDK_IMAGE_FORMAT_RGBA8_UNORM) &&
      image->bytes_per_channel ==
          (image->format == LDK_IMAGE_FORMAT_RGBA16_UNORM ? 2u : 1u) &&
      image->byte_count == (u64)image->width * image->height * 4u *
          image->bytes_per_channel;
}

static bool s_terrain_heightmap_encode(XStrBuilder *out,
    const LDKTerrain *terrain, const LDKImageInfo *image)
{
  u64 samples = (u64)terrain->width * terrain->height;
  if (!s_terrain_heightmap_valid(terrain, image) ||
      samples > (UINT32_MAX - 3u) / 4u * 3u / 2u)
    return false;
  /* An encoded string may be large; stream output in bounded segments. */
  char encoded[4096];
  size_t used = 0u;
  u32 accumulator = 0u, collected = 0u;
  x_strbuilder_append(out, "  heightmap_encoding: \"u16le-base64\"\n  heightmap_samples: \"");
  for (u64 i = 0u; i < samples; ++i)
  {
    size_t index = ((size_t)i * 4u + (size_t)terrain->height_channel) *
        image->bytes_per_channel;
    u16 h;
    if (image->bytes_per_channel == 2u)
      memcpy(&h, image->pixels + index, sizeof(h));
    else
      h = (u16)image->pixels[index] * 257u;
    const u8 bytes[2] = {(u8)(h & 255u), (u8)(h >> 8u)};
    for (u32 j = 0u; j < 2u; ++j)
    {
      accumulator = (accumulator << 8u) | bytes[j];
      collected += 8u;
      while (collected >= 6u)
      {
        collected -= 6u;
        encoded[used++] = s_terrain_base64[(accumulator >> collected) & 63u];
      }
      if (used >= sizeof(encoded) - 8u)
      {
        x_strbuilder_append_substring(out, encoded, used);
        used = 0u;
      }
    }
  }
  if (collected)
  {
    encoded[used++] = s_terrain_base64[(accumulator << (6u - collected)) & 63u];
  }
  const u64 byte_count = samples * 2u;
  const u32 padding = (u32)((3u - byte_count % 3u) % 3u);
  for (u32 i = 0u; i < padding; ++i) encoded[used++] = '=';
  if (used) x_strbuilder_append_substring(out, encoded, used);
  x_strbuilder_append(out, "\"\n");
  return true;
}

static bool s_terrain_heightmap_decode(const LDKTerrain *terrain,
    TMLString encoded, LDKImage **out_image)
{
  const u64 samples = (u64)terrain->width * terrain->height;
  const u64 bytes = samples * 2u;
  const u64 expected = 4u * ((bytes + 2u) / 3u);
  if (!samples || bytes > SIZE_MAX || expected != encoded.size ||
      samples > SIZE_MAX / (4u * sizeof(u16))) return false;
  u16 *pixels = calloc((size_t)samples * 4u, sizeof(u16));
  if (!pixels) return false;
  u64 produced = 0u;
  u32 value = 0u, bits = 0u;
  const u32 padding = (u32)((3u - bytes % 3u) % 3u);
  u8 pair[2] = {0};
  for (u32 i = 0u; i < encoded.size; ++i)
  {
    char c = encoded.data[i];
    int digit = s_terrain_base64_value(c);
    if ((u64)i >= expected - padding)
    {
      if (c != '=') goto invalid;
      continue;
    }
    if (digit < 0) goto invalid;
    value = (value << 6u) | (u32)digit;
    bits += 6u;
    if (bits >= 8u)
    {
      bits -= 8u;
      if (produced >= bytes) goto invalid;
      pair[produced & 1u] = (u8)((value >> bits) & 255u);
      if (produced & 1u)
      {
        const u16 h = (u16)pair[0] | ((u16)pair[1] << 8u);
        const size_t base = (size_t)(produced / 2u) * 4u;
        pixels[base + 0u] = h;
        pixels[base + 1u] = h;
        pixels[base + 2u] = h;
        pixels[base + 3u] = terrain->height_channel == LDK_TERRAIN_CHANNEL_A
            ? h : 65535u;
      }
      ++produced;
    }
  }
  if (produced != bytes || (bits && (value & ((1u << bits) - 1u))))
    goto invalid;
  LDKImage *image = ldk_image_create_format(terrain->width, terrain->height,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  free(pixels);
  if (!image) return false;
  *out_image = image;
  return true;
invalid:
  free(pixels);
  return false;
}


/* R8 vegetation masks are stored at terrain-grid resolution. Keep the
 * encoding separate from heightmap R16 so a scene does not pay for redundant
 * RGBA channels or external PNG assets. */
static bool s_terrain_density_encode(XStrBuilder *out, const u8 *bytes,
    u64 count)
{
  if (!out || !bytes || !count || count > (UINT32_MAX / 4u) * 3u - 2u)
    return false;
  char encoded[4096];
  size_t used = 0u;
  u32 accumulator = 0u, bits = 0u;
  x_strbuilder_append(out, "      density_encoding: \"u8-base64\"\n"
      "      density_samples: \"");
  for (u64 i = 0u; i < count; ++i)
  {
    accumulator = (accumulator << 8u) | bytes[i];
    bits += 8u;
    while (bits >= 6u)
    {
      bits -= 6u;
      encoded[used++] = s_terrain_base64[(accumulator >> bits) & 63u];
    }
    if (used >= sizeof(encoded) - 8u)
    {
      x_strbuilder_append_substring(out, encoded, used);
      used = 0u;
    }
  }
  if (bits)
    encoded[used++] = s_terrain_base64[(accumulator << (6u - bits)) & 63u];
  for (u32 i = 0u; i < (3u - count % 3u) % 3u; ++i)
    encoded[used++] = '=';
  if (used) x_strbuilder_append_substring(out, encoded, used);
  x_strbuilder_append(out, "\"\n");
  return true;
}

static bool s_terrain_density_decode(TMLString encoded, u64 count,
    u8 **out_bytes)
{
  if (!out_bytes || !count || count > SIZE_MAX ||
      count > (UINT32_MAX / 4u) * 3u - 2u ||
      encoded.size != 4u * ((count + 2u) / 3u)) return false;
  u8 *bytes = malloc((size_t)count);
  if (!bytes) return false;
  const u32 padding = (u32)((3u - count % 3u) % 3u);
  u64 written = 0u;
  u32 bits = 0u, accumulator = 0u;
  bool ok = true;
  for (u32 i = 0u; i < encoded.size; ++i)
  {
    char c = encoded.data[i];
    if (i >= encoded.size - padding)
    {
      if (c != '=') {ok = false; break;}
      continue;
    }
    int digit = s_terrain_base64_value(c);
    if (digit < 0) {ok = false; break;}
    accumulator = (accumulator << 6u) | (u32)digit;
    bits += 6u;
    if (bits >= 8u)
    {
      bits -= 8u;
      if (written >= count) {ok = false; break;}
      bytes[written++] = (u8)((accumulator >> bits) & 255u);
    }
  }
  if (!ok || written != count ||
      (bits && (accumulator & ((1u << bits) - 1u))))
  {
    free(bytes);
    return false;
  }
  *out_bytes = bytes;
  return true;
}

static bool s_terrain_to_tml_internal(const LDKTerrain *terrain,
    const LDKImageInfo *heightmap,
    const u8 *const masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    char **out_text)
{
  XStrBuilder *out;
  const char *text;
  if (!out_text || !ldk_terrain_validate(terrain) ||
      (heightmap && terrain->heightmap_path.length))
  {
    return false;
  }
  *out_text = NULL;
  out = x_strbuilder_create();
  if (!out)
  {
    return false;
  }

  x_strbuilder_append(out, "terrain:\n  version: 1\n");
  s_write_string_entry(out, "  heightmap: ", terrain->heightmap_path.buf);
  x_strbuilder_append_format(out, "  height_channel: %u\n"
      "  resolution: %u, %u\n", (u32)terrain->height_channel,
      terrain->width, terrain->height);
  if (heightmap && !s_terrain_heightmap_encode(out, terrain, heightmap))
  {
    x_strbuilder_destroy(out);
    return false;
  }
  s_write_float_entry(out, "  cell_size: ", terrain->cell_size);
  s_write_float_entry(out, "  height_scale: ", terrain->height_scale);
  s_write_float_entry(out, "  height_offset: ", terrain->height_offset);
  x_strbuilder_append_format(out, "  center_on_origin: %s\n",
      terrain->center_on_origin ? "true" : "false");
  x_strbuilder_append(out, "  origin: ");
  s_write_float(out, terrain->origin_x);
  x_strbuilder_append(out, ", ");
  s_write_float(out, terrain->origin_z);
  x_strbuilder_append_char(out, '\n');
  s_write_string_entry(out, "  material: ", terrain->material_path.buf);
  s_write_string_entry(out, "  texture_atlas: ",
      terrain->texture_atlas_path.buf);
  s_write_string_entry(out, "  color_noise: ",
      terrain->color_noise_path.buf);
  s_write_float_entry(out, "  color_noise_scale: ",
      terrain->color_noise_scale);
  s_write_float_entry(out, "  color_noise_strength: ",
      terrain->color_noise_strength);

  x_strbuilder_append(out, "  surfaces:\n");
  for (u32 i = 0u; i < terrain->surface_count; ++i)
  {
    const LDKTerrainSurface *s = &terrain->surfaces[i];
    s_write_string_entry(out, "    - name: ", s->name);
    s_write_float_entry(out, "      min_height: ", s->min_height);
    s_write_float_entry(out, "      blend_width: ", s->blend_width);
    x_strbuilder_append_format(out, "      color: %u\n"
        "      noise_low_color: %u\n"
        "      noise_high_color: %u\n"
        "      sprite_id: %llu\n", s->color,
        s->noise_low_color, s->noise_high_color,
        (unsigned long long)s->sprite_id);
    x_strbuilder_append(out, "      uv_scale: ");
    s_write_float(out, s->uv_scale.x);
    x_strbuilder_append(out, ", ");
    s_write_float(out, s->uv_scale.y);
    x_strbuilder_append_char(out, '\n');
    s_write_float_entry(out, "      texture_weight: ", s->texture_weight);
    x_strbuilder_append_format(out, "      use_slope: %s\n",
        s->use_slope ? "true" : "false");
    s_write_float_entry(out, "      min_slope_degrees: ",
        s->min_slope_degrees);
    s_write_float_entry(out, "      slope_blend_degrees: ",
        s->slope_blend_degrees);
    s_write_string_entry(out, "      paint_mask: ", s->paint_mask_path.buf);
  }

  x_strbuilder_append_format(out,
      "  next_vegetation_id: %llu\n  vegetation:\n",
      (unsigned long long)terrain->next_vegetation_layer_id);
  for (u32 i = 0u; i < terrain->vegetation_layer_count; ++i)
  {
    const LDKTerrainVegetationLayer *layer =
        &terrain->vegetation_layers[i];
    x_strbuilder_append_format(out, "    - id: %llu\n",
        (unsigned long long)layer->id);
    s_write_float_entry(out, "      density_scale: ",
        layer->density_scale);
    x_strbuilder_append_format(out, "      seed: %u\n", layer->seed);
    const LDKGrassTypeDesc *type = &layer->type;
    x_strbuilder_append(out, "      type_settings:\n");
    x_strbuilder_append_format(out, "        kind: %u\n",
        (u32)type->kind);
    s_write_string_entry(out, "        mesh: ", type->mesh_path.buf);
    for (u32 slot = 0u; slot < LDK_GRASS_MESH_MATERIAL_SLOT_COUNT; ++slot)
    {
      char key[32];
      snprintf(key, sizeof(key), "        material_%u: ", slot);
      s_write_string_entry(out, key, type->material_paths[slot].buf);
    }
    x_strbuilder_append_format(out, "        mesh_index: %u\n",
        type->mesh_index);
    s_write_vec3_entry(out, "        scale_min: ", type->scale_min);
    s_write_vec3_entry(out, "        scale_max: ", type->scale_max);
    s_write_vec3_entry(out, "        rotation_min: ", type->rotation_min);
    s_write_vec3_entry(out, "        rotation_max: ", type->rotation_max);
    x_strbuilder_append_format(out, "        align_to_normal: %s\n",
        type->align_to_normal ? "true" : "false");
    x_strbuilder_append_format(out, "        follow_terrain: %s\n",
        type->follow_terrain ? "true" : "false");
    s_write_string_entry(out, "        name: ", x_smallstr_cstr(&type->name));
    x_strbuilder_append_format(out, "        blade_preset: %u\n",
        (u32)type->blade_preset);
    s_write_float_entry(out, "        density: ", type->density);
    s_write_float_entry(out, "        clumping: ", type->clumping);
    s_write_float_entry(out, "        clump_radius: ", type->clump_radius);
    s_write_float_entry(out, "        min_height: ", type->min_height);
    s_write_float_entry(out, "        max_height: ", type->max_height);
    x_strbuilder_append_format(out, "        use_noise_height: %s\n",
        type->use_noise_height ? "true" : "false");
    s_write_float_entry(out, "        blade_width: ", type->blade_width);
    s_write_float_entry(out, "        curvature: ", type->curvature);
    s_write_float_entry(out, "        wind_strength: ", type->wind_strength);
    s_write_float_entry(out, "        interaction_recovery_time: ",
        type->interaction_recovery_time);
    x_strbuilder_append_format(out, "        bottom_color: %u\n"
        "        top_color: %u\n"
        "        use_noise_color: %s\n"
        "        use_custom_noise_palette: %s\n"
        "        noise_color_stop_0_color: %u\n"
        "        noise_color_stop_1_color: %u\n"
        "        noise_color_stop_2_color: %u\n"
        "        noise_color_stop_3_color: %u\n",
        type->bottom_color, type->top_color,
        type->use_noise_color ? "true" : "false",
        type->use_custom_noise_palette ? "true" : "false",
        type->noise_color_stop_0_color,
        type->noise_color_stop_1_color,
        type->noise_color_stop_2_color,
        type->noise_color_stop_3_color);
    if (masks && masks[i] && !s_terrain_density_encode(out, masks[i],
            (u64)terrain->width * terrain->height))
    {
      x_strbuilder_destroy(out);
      return false;
    }
  }

  text = x_strbuilder_to_string(out);
  if (text)
  {
    size_t size = strlen(text) + 1u;
    *out_text = malloc(size);
    if (*out_text)
    {
      memcpy(*out_text, text, size);
    }
  }
  x_strbuilder_destroy(out);
  return *out_text != NULL;
}

bool ldk_terrain_to_tml(const LDKTerrain *terrain, char **out_text)
{
  return s_terrain_to_tml_internal(terrain, NULL, NULL, out_text);
}

bool ldk_terrain_to_tml_with_heightmap(const LDKTerrain *terrain,
    const LDKImageInfo *heightmap, char **out_text)
{
  return s_terrain_to_tml_internal(terrain, heightmap, NULL, out_text);
}

static bool s_read_string(const TMLDocument *doc, const TMLNode *node,
    const char *key, char *buffer, size_t capacity)
{
  TMLString value;
  if (!buffer || !capacity ||
      !tml_node_get_string(doc, node, key, &value) ||
      value.size >= capacity || memchr(value.data, '\0', value.size))
  {
    return false;
  }
  memcpy(buffer, value.data, value.size);
  buffer[value.size] = '\0';
  return true;
}

static bool s_read_path(const TMLDocument *doc, const TMLNode *node,
    const char *key, LDKAssetPath *out_path)
{
  char buffer[LDK_ASSET_PATH_MAX_LENGTH + 1u];
  return s_read_string(doc, node, key, buffer, sizeof(buffer)) &&
      s_optional_path_set(out_path, buffer);
}

bool ldk_terrain_to_tml_with_component_data(const LDKTerrain *terrain,
    const LDKImageInfo *heightmap,
    const u8 *const masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    char **out_text)
{
  return s_terrain_to_tml_internal(terrain, heightmap, masks, out_text);
}

static bool s_read_float(const TMLDocument *doc, const TMLNode *node,
    const char *key, float *out_value)
{
  f64 value;
  if (!tml_node_get_f64(doc, node, key, &value) || !isfinite(value) ||
      value > FLT_MAX || value < -FLT_MAX)
  {
    return false;
  }
  *out_value = (float)value;
  return true;
}

static bool s_read_u32(const TMLDocument *doc, const TMLNode *node,
    const char *key, u32 *out_value)
{
  i64 value;
  if (!tml_node_get_i64(doc, node, key, &value) ||
      value < 0 || value > UINT32_MAX)
  {
    return false;
  }
  *out_value = (u32)value;
  return true;
}

static bool s_read_u64(const TMLDocument *doc, const TMLNode *node,
    const char *key, u64 *out_value)
{
  i64 value;
  if (!tml_node_get_i64(doc, node, key, &value) || value < 0)
  {
    return false;
  }
  *out_value = (u64)value;
  return true;
}

static bool s_read_bool(const TMLDocument *doc, const TMLNode *node,
    const char *key, bool *out_value)
{
  u8 value;
  if (!tml_node_get_bool(doc, node, key, &value))
  {
    return false;
  }
  *out_value = value != 0u;
  return true;
}

static bool s_read_vec2(const TMLDocument *doc, const TMLNode *node,
    const char *key, Vec2 *out_value)
{
  TMLF64Slice values;
  if (!tml_node_get_f64_array(doc, node, key, &values) ||
      values.count != 2u || !isfinite(values.data[0]) ||
      !isfinite(values.data[1]) ||
      fabs(values.data[0]) > FLT_MAX ||
      fabs(values.data[1]) > FLT_MAX)
  {
    return false;
  }
  out_value->x = (float)values.data[0];
  out_value->y = (float)values.data[1];
  return true;
}

static bool s_read_vec3(const TMLDocument *doc, const TMLNode *node,
    const char *key, Vec3 *out_value)
{
  TMLF64Slice values;
  if (!tml_node_get_f64_array(doc, node, key, &values) ||
      values.count != 3u) return false;
  for (u32 i = 0u; i < 3u; ++i)
    if (!isfinite(values.data[i]) || fabs(values.data[i]) > FLT_MAX)
      return false;
  *out_value = vec3_make((float)values.data[0],
      (float)values.data[1], (float)values.data[2]);
  return true;
}

static bool s_terrain_from_tml_node_internal(LDKTerrain *terrain,
    LDKImage **out_image,
    u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    const TMLDocument *doc, const TMLNode *root)
{
  const TMLNode *surfaces;
  const TMLNode *vegetation;
  LDKTerrain candidate;
  Vec2 origin = {0};
  i64 version;
  u32 channel;
  TMLI64Slice resolution;
  bool valid;
  if (!terrain || !doc || !root || !out_image) return false;
  if (masks)
    for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
      masks[i] = NULL;
  ldk_terrain_init(&candidate);
  valid = root && root->name.size == sizeof("terrain") - 1u &&
      memcmp(root->name.data, "terrain", sizeof("terrain") - 1u) == 0 &&
      tml_node_get_i64(doc, root, "version", &version) &&
      version == 1 &&
      s_read_path(doc, root, "heightmap",
          &candidate.heightmap_path) &&
      s_read_u32(doc, root, "height_channel", &channel) &&
      channel <= LDK_TERRAIN_CHANNEL_A &&
      tml_node_get_i64_array(doc, root, "resolution",
          &resolution) && resolution.count == 2u &&
      resolution.data[0] > 0 && resolution.data[0] <= UINT32_MAX &&
      resolution.data[1] > 0 && resolution.data[1] <= UINT32_MAX;
  if (valid)
  {
    candidate.height_channel = (LDKTerrainChannel)channel;
    candidate.width = (u32)resolution.data[0];
    candidate.height = (u32)resolution.data[1];
    valid = s_read_float(doc, root, "cell_size",
            &candidate.cell_size) &&
        s_read_float(doc, root, "height_scale",
            &candidate.height_scale) &&
        s_read_float(doc, root, "height_offset",
            &candidate.height_offset) &&
        s_read_bool(doc, root, "center_on_origin",
            &candidate.center_on_origin) &&
        s_read_vec2(doc, root, "origin", &origin) &&
        s_read_path(doc, root, "material",
            &candidate.material_path) &&
        s_read_path(doc, root, "texture_atlas",
            &candidate.texture_atlas_path) &&
        s_read_path(doc, root, "color_noise",
            &candidate.color_noise_path) &&
        s_read_float(doc, root, "color_noise_scale",
            &candidate.color_noise_scale) &&
        s_read_float(doc, root, "color_noise_strength",
            &candidate.color_noise_strength);
    candidate.origin_x = origin.x;
    candidate.origin_z = origin.y;
  }

  surfaces = valid
      ? tml_node_find_child(doc, root, "surfaces") : NULL;
  vegetation = valid
      ? tml_node_find_child(doc, root, "vegetation") : NULL;
  valid = valid && surfaces && vegetation &&
      surfaces->child_count >= 1u &&
      surfaces->child_count <= LDK_TERRAIN_SURFACE_COUNT &&
      vegetation->child_count <= LDK_TERRAIN_VEGETATION_LAYER_COUNT &&
      s_read_u64(doc, root, "next_vegetation_id",
          &candidate.next_vegetation_layer_id);
  if (valid)
  {
    candidate.surface_count = surfaces->child_count;
    candidate.vegetation_layer_count = vegetation->child_count;
  }
  for (u32 i = 0u; valid && i < candidate.surface_count; ++i)
  {
    const TMLNode *node = tml_node_child_at(doc, surfaces, i);
    LDKTerrainSurface *s = &candidate.surfaces[i];
    valid = node &&
        s_read_string(doc, node, "name", s->name,
            sizeof(s->name)) &&
        s_read_float(doc, node, "min_height", &s->min_height) &&
        s_read_float(doc, node, "blend_width", &s->blend_width) &&
        s_read_u32(doc, node, "color", &s->color) &&
        s_read_u32(doc, node, "noise_low_color",
            &s->noise_low_color) &&
        s_read_u32(doc, node, "noise_high_color",
            &s->noise_high_color) &&
        s_read_u64(doc, node, "sprite_id", &s->sprite_id) &&
        s_read_vec2(doc, node, "uv_scale", &s->uv_scale) &&
        s_read_float(doc, node, "texture_weight",
            &s->texture_weight) &&
        s_read_bool(doc, node, "use_slope", &s->use_slope) &&
        s_read_float(doc, node, "min_slope_degrees",
            &s->min_slope_degrees) &&
        s_read_float(doc, node, "slope_blend_degrees",
            &s->slope_blend_degrees) &&
        s_read_path(doc, node, "paint_mask", &s->paint_mask_path);
  }
  for (u32 i = 0u; valid && i < candidate.vegetation_layer_count; ++i)
  {
    const TMLNode *node = tml_node_child_at(doc, vegetation, i);
    LDKTerrainVegetationLayer *layer = &candidate.vegetation_layers[i];
    valid = node &&
        s_read_u64(doc, node, "id", &layer->id) &&
        s_read_float(doc, node, "density_scale",
            &layer->density_scale) &&
        s_read_u32(doc, node, "seed", &layer->seed);
    /* Do not silently drop an older external mask when converting to the
     * one-owned-R8-mask model. It must first be imported by an older build. */
    if (valid && tml_node_find_entry(doc, node, "density_map"))
    {
      TMLString old_path = {0};
      valid = tml_node_get_string(doc, node, "density_map", &old_path) &&
          (old_path.size == 0u ||
              tml_node_find_entry(doc, node, "density_samples") != NULL);
    }
    if (!valid) break;
    const TMLNode *settings = tml_node_find_child(doc, node,
        "type_settings");
    /* Previously a name was stored in 'type' beside type_settings. Accept
     * that complete representation on load but write one description only. */
    if (!settings) { valid = false; break; }
    LDKGrassTypeDesc *type = &layer->type;
    ldk_grass_type_desc_defaults(type);
    char name[X_SMALLSTR_MAX_LENGTH + 1u] = {0};
    u32 preset = 0u;
    valid = (s_read_string(doc, settings, "name", name, sizeof(name)) ||
        s_read_string(doc, node, "type", name, sizeof(name))) &&
        x_smallstr_init(&type->name, name) >= 0 &&
        s_read_u32(doc, settings, "blade_preset", &preset) &&
        preset <= LDK_GRASS_BLADE_PRESET_CUSTOM_7 &&
        s_read_float(doc, settings, "density", &type->density) &&
        s_read_float(doc, settings, "clumping", &type->clumping) &&
        s_read_float(doc, settings, "clump_radius",
            &type->clump_radius) &&
        s_read_float(doc, settings, "min_height", &type->min_height) &&
        s_read_float(doc, settings, "max_height", &type->max_height) &&
        s_read_bool(doc, settings, "use_noise_height",
            &type->use_noise_height) &&
        s_read_float(doc, settings, "blade_width", &type->blade_width) &&
        s_read_float(doc, settings, "curvature", &type->curvature) &&
        s_read_float(doc, settings, "wind_strength", &type->wind_strength) &&
        s_read_float(doc, settings, "interaction_recovery_time",
            &type->interaction_recovery_time) &&
        s_read_u32(doc, settings, "bottom_color", &type->bottom_color) &&
        s_read_u32(doc, settings, "top_color", &type->top_color) &&
        s_read_bool(doc, settings, "use_noise_color",
            &type->use_noise_color) &&
        s_read_bool(doc, settings, "use_custom_noise_palette",
            &type->use_custom_noise_palette) &&
        s_read_u32(doc, settings, "noise_color_stop_0_color",
            &type->noise_color_stop_0_color) &&
        s_read_u32(doc, settings, "noise_color_stop_1_color",
            &type->noise_color_stop_1_color) &&
        s_read_u32(doc, settings, "noise_color_stop_2_color",
            &type->noise_color_stop_2_color) &&
        s_read_u32(doc, settings, "noise_color_stop_3_color",
            &type->noise_color_stop_3_color);
    type->blade_preset = (LDKGrassBladePreset)preset;
    /* Old files used decoration: true; the old single cutout texture is
     * intentionally not a material and has no meaning for arbitrary meshes. */
    if (valid && tml_node_find_entry(doc, settings, "kind"))
    {
      u32 kind = 0u;
      valid = s_read_u32(doc, settings, "kind", &kind) &&
          kind <= LDK_GRASS_LAYER_TREE;
      if (valid) type->kind = (LDKGrassLayerKind)kind;
    }
    else if (valid && tml_node_find_entry(doc, settings, "decoration"))
    {
      bool old_decoration = false;
      valid = s_read_bool(doc, settings, "decoration", &old_decoration);
      if (valid && old_decoration) type->kind = LDK_GRASS_LAYER_DECORATION;
    }
    if (valid && tml_node_find_entry(doc, settings, "mesh"))
      valid = s_read_path(doc, settings, "mesh", &type->mesh_path);
    for (u32 slot = 0u; valid &&
        slot < LDK_GRASS_MESH_MATERIAL_SLOT_COUNT; ++slot)
    {
      char key[32];
      snprintf(key, sizeof(key), "material_%u", slot);
      if (tml_node_find_entry(doc, settings, key))
        valid = s_read_path(doc, settings, key, &type->material_paths[slot]);
    }
    if (valid && tml_node_find_entry(doc, settings, "mesh_index"))
      valid = s_read_u32(doc, settings, "mesh_index", &type->mesh_index);
    if (valid && tml_node_find_entry(doc, settings, "scale_min"))
      valid = s_read_vec3(doc, settings, "scale_min", &type->scale_min);
    if (valid && tml_node_find_entry(doc, settings, "scale_max"))
      valid = s_read_vec3(doc, settings, "scale_max", &type->scale_max);
    if (valid && tml_node_find_entry(doc, settings, "rotation_min"))
      valid = s_read_vec3(doc, settings, "rotation_min", &type->rotation_min);
    if (valid && tml_node_find_entry(doc, settings, "rotation_max"))
      valid = s_read_vec3(doc, settings, "rotation_max", &type->rotation_max);
    if (valid && tml_node_find_entry(doc, settings, "align_to_normal"))
      valid = s_read_bool(doc, settings, "align_to_normal",
          &type->align_to_normal);
    if (valid && tml_node_find_entry(doc, settings, "follow_terrain"))
      valid = s_read_bool(doc, settings, "follow_terrain",
          &type->follow_terrain);
  }
  valid = valid && ldk_terrain_validate(&candidate);
  LDKImage *embedded = NULL;
  if (valid && tml_node_find_entry(doc, root, "heightmap_samples"))
  {
    TMLString encoding = {0}, samples = {0};
    static const char name[] = "u16le-base64";
    valid = !candidate.heightmap_path.length &&
        tml_node_get_string(doc, root,
            "heightmap_encoding", &encoding) &&
        encoding.size == sizeof(name) - 1u &&
        memcmp(encoding.data, name, sizeof(name) - 1u) == 0 &&
        tml_node_get_string(doc, root,
            "heightmap_samples", &samples) &&
        s_terrain_heightmap_decode(&candidate, samples, &embedded);
  }
  if (valid && masks)
  {
    const u64 count = (u64)candidate.width * candidate.height;
    for (u32 i = 0u; i < candidate.vegetation_layer_count; ++i)
    {
      const TMLNode *node = tml_node_child_at(doc, vegetation, i);
      if (!tml_node_find_entry(doc, node, "density_samples")) continue;
      TMLString encoding = {0}, samples = {0};
      static const char name[] = "u8-base64";
      valid = tml_node_get_string(doc, node, "density_encoding", &encoding) &&
          encoding.size == sizeof(name) - 1u &&
          memcmp(encoding.data, name, sizeof(name) - 1u) == 0 &&
          tml_node_get_string(doc, node, "density_samples", &samples) &&
          s_terrain_density_decode(samples, count, &masks[i]);
      if (!valid) break;
    }
  }
  if (!valid)
  {
    ldk_image_destroy(embedded);
    if (masks)
      for (u32 i = 0u; i < LDK_TERRAIN_VEGETATION_LAYER_COUNT; ++i)
      {
        free(masks[i]);
        masks[i] = NULL;
      }
    return false;
  }
  *terrain = candidate;
  *out_image = embedded;
  return true;
}

bool ldk_terrain_from_tml_node_with_heightmap(LDKTerrain *terrain,
    LDKImage **out_image, const TMLDocument *doc, const TMLNode *node)
{
  return s_terrain_from_tml_node_internal(terrain, out_image, NULL,
      doc, node);
}

bool ldk_terrain_from_tml_node_with_component_data(LDKTerrain *terrain,
    LDKImage **out_image,
    u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    const TMLDocument *doc, const TMLNode *node)
{
  if (!masks) return false;
  return s_terrain_from_tml_node_internal(terrain, out_image, masks,
      doc, node);
}

bool ldk_terrain_from_tml_with_heightmap(LDKTerrain *terrain,
    LDKImage **out_image, const char *text)
{
  if (!terrain || !out_image || !text) return false;
  TMLParseResult parsed = tml_parse(text);
  if (!parsed.ok) return false;
  const TMLNode *root = tml_root_node_at(parsed.document, 0u);
  bool valid = root && !tml_root_node_at(parsed.document, 1u) &&
      ldk_terrain_from_tml_node_with_heightmap(terrain, out_image,
          parsed.document, root);
  tml_document_free(parsed.document);
  return valid;
}

bool ldk_terrain_from_tml(LDKTerrain *terrain, const char *text)
{
  LDKImage *image = NULL;
  bool ok = ldk_terrain_from_tml_with_heightmap(terrain, &image, text);
  ldk_image_destroy(image);
  return ok;
}

