#include <ldk_texture_atlas.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_io.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_tml.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void ldk_texture_atlas_init(LDKTextureAtlas *atlas)
{
  if (atlas)
  {
    memset(atlas, 0, sizeof(*atlas));
    atlas->next_region_id = 1;
  }
}

void ldk_texture_atlas_clear(LDKTextureAtlas *atlas)
{
  if (atlas)
  {
    free(atlas->regions);
    ldk_texture_atlas_init(atlas);
  }
}

static bool s_atlas_name_valid(const char *name)
{
  size_t length = name ? strlen(name) : 0;
  return length && length < LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY;
}

static bool s_atlas_name_used(const LDKTextureAtlas *atlas, const char *name,
    u64 except_id)
{
  for (u32 i = 0; i < atlas->region_count; ++i)
  {
    if (atlas->regions[i].id != except_id &&
        strcmp(atlas->regions[i].name, name) == 0)
    {
      return true;
    }
  }
  return false;
}

static bool s_atlas_reserve(LDKTextureAtlas *atlas, u32 required)
{
  if (required <= atlas->region_capacity)
  {
    return true;
  }
  u32 capacity = atlas->region_capacity ? atlas->region_capacity : 16;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }
  if ((u64)capacity * sizeof(*atlas->regions) > SIZE_MAX)
  {
    return false;
  }
  void *memory = realloc(atlas->regions,
      (size_t)capacity * sizeof(*atlas->regions));
  if (!memory)
  {
    return false;
  }
  atlas->regions = memory;
  atlas->region_capacity = capacity;
  return true;
}

bool ldk_texture_atlas_texture_set(LDKTextureAtlas *atlas, const char *path)
{
  LDKAssetPath candidate = {0};
  if (!atlas || !path || (path[0] && !ldk_asset_path_set(&candidate, path)))
  {
    return false;
  }
  atlas->texture_path = candidate;
  return true;
}

LDKTextureAtlasRegion *ldk_texture_atlas_region_find(LDKTextureAtlas *atlas,
    u64 id)
{
  if (atlas && id)
  {
    for (u32 i = 0; i < atlas->region_count; ++i)
    {
      if (atlas->regions[i].id == id)
      {
        return &atlas->regions[i];
      }
    }
  }
  return NULL;
}

const LDKTextureAtlasRegion *ldk_texture_atlas_region_find_const(
    const LDKTextureAtlas *atlas, u64 id)
{
  return ldk_texture_atlas_region_find((LDKTextureAtlas *)atlas, id);
}

bool ldk_texture_atlas_region_add(LDKTextureAtlas *atlas, const char *name,
    u32 x, u32 y, u32 width, u32 height, u64 *out_id)
{
  if (!atlas || !s_atlas_name_valid(name) || !width || !height ||
      (u64)x + width > UINT32_MAX || (u64)y + height > UINT32_MAX ||
      atlas->region_count == UINT32_MAX ||
      !atlas->next_region_id || atlas->next_region_id == UINT64_MAX ||
      s_atlas_name_used(atlas, name, 0) ||
      !s_atlas_reserve(atlas, atlas->region_count + 1))
  {
    return false;
  }
  LDKTextureAtlasRegion *region = &atlas->regions[atlas->region_count++];
  memset(region, 0, sizeof(*region));
  region->id = atlas->next_region_id++;
  snprintf(region->name, sizeof(region->name), "%s", name);
  region->x = x;
  region->y = y;
  region->width = width;
  region->height = height;
  region->pivot_x = region->pivot_y = 0.5f;
  if (out_id)
  {
    *out_id = region->id;
  }
  return true;
}

bool ldk_texture_atlas_region_remove(LDKTextureAtlas *atlas, u64 id)
{
  if (!atlas)
  {
    return false;
  }
  for (u32 i = 0; i < atlas->region_count; ++i)
  {
    if (atlas->regions[i].id == id)
    {
      memmove(atlas->regions + i, atlas->regions + i + 1,
          (atlas->region_count - i - 1) * sizeof(*atlas->regions));
      --atlas->region_count;
      return true;
    }
  }
  return false;
}

bool ldk_texture_atlas_region_rename(LDKTextureAtlas *atlas, u64 id,
    const char *name)
{
  LDKTextureAtlasRegion *region = ldk_texture_atlas_region_find(atlas, id);
  if (!region || !s_atlas_name_valid(name) ||
      s_atlas_name_used(atlas, name, id))
  {
    return false;
  }
  snprintf(region->name, sizeof(region->name), "%s", name);
  return true;
}

bool ldk_texture_atlas_validate(const LDKTextureAtlas *atlas)
{
  if (!atlas || !atlas->next_region_id ||
      (atlas->region_count && !atlas->regions) ||
      (atlas->texture_path.buf[0] &&
          (atlas->texture_path.length != strlen(atlas->texture_path.buf))))
  {
    return false;
  }
  for (u32 i = 0; i < atlas->region_count; ++i)
  {
    const LDKTextureAtlasRegion *region = &atlas->regions[i];
    if (!region->id || region->id >= atlas->next_region_id ||
        !memchr(region->name, 0, sizeof(region->name)) ||
        !s_atlas_name_valid(region->name) || !region->width || !region->height ||
        (u64)region->x + region->width > UINT32_MAX ||
        (u64)region->y + region->height > UINT32_MAX ||
        !isfinite(region->pivot_x) || !isfinite(region->pivot_y) ||
        region->pivot_x < 0.0f || region->pivot_x > 1.0f ||
        region->pivot_y < 0.0f || region->pivot_y > 1.0f)
    {
      return false;
    }
    for (u32 j = 0; j < i; ++j)
    {
      if (atlas->regions[j].id == region->id ||
          strcmp(atlas->regions[j].name, region->name) == 0)
      {
        return false;
      }
    }
  }
  return true;
}

bool ldk_texture_atlas_copy(LDKTextureAtlas *dst, const LDKTextureAtlas *src)
{
  if (!dst || !ldk_texture_atlas_validate(src) || dst == src)
  {
    return dst == src && src != NULL;
  }
  LDKTextureAtlas copy;
  ldk_texture_atlas_init(&copy);
  copy.texture_path = src->texture_path;
  copy.next_region_id = src->next_region_id;
  if (!s_atlas_reserve(&copy, src->region_count))
  {
    return false;
  }
  memcpy(copy.regions, src->regions,
      (size_t)src->region_count * sizeof(*copy.regions));
  copy.region_count = src->region_count;
  ldk_texture_atlas_clear(dst);
  *dst = copy;
  return true;
}

bool ldk_texture_atlas_grid_append(LDKTextureAtlas *atlas,
    u32 image_width, u32 image_height, u32 cell_width, u32 cell_height,
    u32 spacing_x, u32 spacing_y, u32 margin_x, u32 margin_y)
{
  if (!atlas || !cell_width || !cell_height ||
      (u64)margin_x * 2u + cell_width > image_width ||
      (u64)margin_y * 2u + cell_height > image_height)
  {
    return false;
  }
  u64 columns = 1u + ((u64)image_width - 2u * margin_x - cell_width) /
      ((u64)cell_width + spacing_x);
  u64 rows = 1u + ((u64)image_height - 2u * margin_y - cell_height) /
      ((u64)cell_height + spacing_y);
  u64 count = rows * columns;
  /* Keep an accidental 1x1 grid on a giant image from freezing the editor. */
  if (!count || count > 4096u ||
      count > UINT32_MAX - atlas->region_count ||
      (u64)atlas->next_region_id + count >= INT64_MAX ||
      !s_atlas_reserve(atlas, atlas->region_count + (u32)count))
  {
    return false;
  }
  for (u64 row = 0; row < rows; ++row)
  {
    for (u64 column = 0; column < columns; ++column)
    {
      LDKTextureAtlasRegion *region = &atlas->regions[atlas->region_count];
      memset(region, 0, sizeof(*region));
      region->id = atlas->next_region_id++;
      region->x = (u32)(margin_x + column * ((u64)cell_width + spacing_x));
      region->y = (u32)(margin_y + row * ((u64)cell_height + spacing_y));
      region->width = cell_width;
      region->height = cell_height;
      region->pivot_x = region->pivot_y = 0.5f;
      /* The stable ID avoids collisions among generated regions. A user can
       * still have assigned a conflicting manual name, so suffix those. */
      char base[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
      snprintf(base, sizeof(base), "sprite_%04llu",
          (unsigned long long)(region->id - 1u));
      snprintf(region->name, sizeof(region->name), "%s", base);
      for (u32 suffix = 1;
          s_atlas_name_used(atlas, region->name, region->id); ++suffix)
      {
        if (suffix == UINT32_MAX)
        {
          /* Unreachable in normal editor use, but do not leak partial state. */
          return false;
        }
        snprintf(region->name, sizeof(region->name), "%s_%u", base, suffix);
      }
      ++atlas->region_count;
    }
  }
  return true;
}

bool ldk_texture_atlas_region_uv(const LDKTextureAtlasRegion *region,
    u32 image_width, u32 image_height, float out_uv[4])
{
  if (!region || !out_uv || !image_width || !image_height ||
      !region->width || !region->height ||
      (u64)region->x + region->width > image_width ||
      (u64)region->y + region->height > image_height)
  {
    return false;
  }
  out_uv[0] = (float)region->x / (float)image_width;
  out_uv[1] = (float)region->y / (float)image_height;
  out_uv[2] = (float)((u64)region->x + region->width) / (float)image_width;
  out_uv[3] = (float)((u64)region->y + region->height) / (float)image_height;
  return true;
}

static void s_atlas_write_string(XStrBuilder *out, const char *text)
{
  x_strbuilder_append_char(out, '"');
  for (; *text; ++text)
  {
    if (*text == '"' || *text == '\\')
    {
      x_strbuilder_append_char(out, '\\');
    }
    if (*text == '\n')
    {
      x_strbuilder_append(out, "\\n");
    }
    else if (*text == '\r')
    {
      x_strbuilder_append(out, "\\r");
    }
    else if (*text == '\t')
    {
      x_strbuilder_append(out, "\\t");
    }
    else
    {
      x_strbuilder_append_char(out, *text);
    }
  }
  x_strbuilder_append_char(out, '"');
}

bool ldk_texture_atlas_to_tml(const LDKTextureAtlas *atlas, char **out_text)
{
  if (!out_text || !ldk_texture_atlas_validate(atlas))
  {
    return false;
  }
  *out_text = NULL;
  XStrBuilder *out = x_strbuilder_create();
  if (!out)
  {
    return false;
  }
  x_strbuilder_append(out, "texture_atlas:\n  version: 1\n  texture: ");
  s_atlas_write_string(out, atlas->texture_path.buf);
  x_strbuilder_append_format(out, "\n  next_id: %llu\n  regions:\n",
      (unsigned long long)atlas->next_region_id);
  for (u32 i = 0; i < atlas->region_count; ++i)
  {
    const LDKTextureAtlasRegion *r = &atlas->regions[i];
    x_strbuilder_append_format(out, "    - id: %llu\n      name: ",
        (unsigned long long)r->id);
    s_atlas_write_string(out, r->name);
    x_strbuilder_append_format(out,
        "\n      rect: %u, %u, %u, %u\n      pivot: %.9f, %.9f\n",
        r->x, r->y, r->width, r->height,
        (double)r->pivot_x, (double)r->pivot_y);
  }
  const char *text = x_strbuilder_to_string(out);
  if (text)
  {
    size_t size = strlen(text) + 1;
    *out_text = malloc(size);
    if (*out_text)
    {
      memcpy(*out_text, text, size);
    }
  }
  x_strbuilder_destroy(out);
  return *out_text != NULL;
}

bool ldk_texture_atlas_from_tml(LDKTextureAtlas *atlas, const char *text)
{
  if (!atlas || !text)
  {
    return false;
  }
  TMLParseResult parsed = tml_parse(text);
  if (!parsed.ok)
  {
    return false;
  }
  LDKTextureAtlas candidate;
  ldk_texture_atlas_init(&candidate);
  const TMLNode *root = tml_root_node_at(parsed.document, 0);
  TMLString texture;
  i64 version, next_id;
  bool valid = root && root->name.size == strlen("texture_atlas") &&
      memcmp(root->name.data, "texture_atlas", root->name.size) == 0 &&
      tml_node_get_i64(parsed.document, root, "version", &version) &&
      version == 1 &&
      tml_node_get_string(parsed.document, root, "texture", &texture) &&
      texture.size <= LDK_ASSET_PATH_MAX_LENGTH &&
      !memchr(texture.data, '\0', texture.size);
  if (valid)
  {
    char buffer[LDK_ASSET_PATH_MAX_LENGTH + 1];
    memcpy(buffer, texture.data, texture.size);
    buffer[texture.size] = 0;
    valid = ldk_texture_atlas_texture_set(&candidate, buffer);
  }
  const TMLNode *regions = valid ?
      tml_node_find_child(parsed.document, root, "regions") : NULL;
  if (!regions)
  {
    valid = false;
  }
  for (u32 i = 0; valid; ++i)
  {
    const TMLNode *node = tml_node_child_at(parsed.document, regions, i);
    if (!node)
    {
      break;
    }
    i64 id;
    TMLString name;
    TMLI64Slice rect;
    TMLF64Slice pivot;
    valid = tml_node_get_i64(parsed.document, node, "id", &id) && id > 0 &&
        tml_node_get_string(parsed.document, node, "name", &name) &&
        name.size && name.size < LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY &&
        !memchr(name.data, '\0', name.size) &&
        tml_node_get_i64_array(parsed.document, node, "rect", &rect) &&
        rect.count == 4 &&
        tml_node_get_f64_array(parsed.document, node, "pivot", &pivot) &&
        pivot.count == 2;
    if (!valid)
    {
      break;
    }
    for (u32 j = 0; j < 4; ++j)
    {
      if (rect.data[j] < 0 || rect.data[j] > UINT32_MAX)
      {
        valid = false;
      }
    }
    if (!valid || !isfinite(pivot.data[0]) || !isfinite(pivot.data[1]))
    {
      break;
    }
    char buffer[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
    memcpy(buffer, name.data, name.size);
    buffer[name.size] = 0;
    u64 new_id;
    valid = ldk_texture_atlas_region_add(&candidate, buffer,
        (u32)rect.data[0], (u32)rect.data[1],
        (u32)rect.data[2], (u32)rect.data[3], &new_id);
    if (valid)
    {
      candidate.regions[candidate.region_count - 1].id = (u64)id;
      candidate.regions[candidate.region_count - 1].pivot_x = (float)pivot.data[0];
      candidate.regions[candidate.region_count - 1].pivot_y = (float)pivot.data[1];
    }
  }
  if (valid && tml_node_get_i64(parsed.document, root, "next_id", &next_id) &&
      next_id > 0)
  {
    candidate.next_region_id = (u64)next_id;
  }
  else
  {
    valid = false;
  }
  valid = valid && ldk_texture_atlas_validate(&candidate);
  tml_document_free(parsed.document);
  if (!valid)
  {
    ldk_texture_atlas_clear(&candidate);
    return false;
  }
  ldk_texture_atlas_clear(atlas);
  *atlas = candidate;
  return true;
}

bool ldk_texture_atlas_load(LDKTextureAtlas *atlas, const char *filename)
{
  if (!atlas || !filename)
  {
    return false;
  }
  size_t size = 0;
  char *text = x_io_read_text(filename, &size);
  if (!text)
  {
    return false;
  }
  bool success = ldk_texture_atlas_from_tml(atlas, text);
  free(text);
  return success;
}

bool ldk_texture_atlas_save(const LDKTextureAtlas *atlas, const char *filename)
{
  char *text = NULL;
  if (!filename || !ldk_texture_atlas_to_tml(atlas, &text))
  {
    return false;
  }
  bool success = x_io_write_text(filename, text);
  free(text);
  return success;
}
