/* Manager-owned .terrain assets. Paths and image handles follow the existing
 * shared-asset lifetime rules; the terrain metadata itself owns no images. */
#include <module/ldk_asset_manager.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LDKAssetTerrain ldk_asset_terrain_null(void)
{
  LDKAssetTerrain result = {x_handle_null()};
  return result;
}

bool ldk_asset_manager_terrain_is_alive(
    LDKAssetManager *manager, LDKAssetTerrain asset)
{
  LDKAssetHandle generic = {asset.h};
  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_TERRAIN;
}

const LDKAssetTerrainData *ldk_asset_manager_terrain_get_const(
    LDKAssetManager *manager, LDKAssetTerrain asset)
{
  LDKAssetHandle generic = {asset.h};
  const LDKAssetInfo *info = ldk_asset_get_info_const(manager, generic);
  return info && info->type == LDK_ASSET_TYPE_TERRAIN
      ? (const LDKAssetTerrainData *)info->data : NULL;
}

static LDKAssetImage s_terrain_heightmap_resolve(
    LDKAssetManager *manager, const LDKTerrain *terrain)
{
  if (!terrain->heightmap_path.buf[0])
  {
    return ldk_asset_image_null();
  }
  /* Unavailable heightmaps do not prevent inspecting/editing metadata. */
  return ldk_asset_manager_image_load_shared(
      manager, terrain->heightmap_path.buf);
}

static LDKAssetTerrain s_terrain_create(LDKAssetManager *manager,
    const LDKTerrain *terrain, LDKImage *embedded, const LDKAssetPath *path)
{
  LDKAssetTerrain result = ldk_asset_terrain_null();
  LDKAssetTerrainData *data;
  LDKAssetInfo *info;
  XHandle handle;
  if (!manager || !ldk_terrain_validate(terrain))
  {
    return result;
  }
  data = calloc(1u, sizeof(*data));
  if (!data)
  {
    return result;
  }
  data->terrain = *terrain;
  data->heightmap_embedded = embedded;
  data->heightmap = s_terrain_heightmap_resolve(manager, terrain);
  data->revision = 1u;
  handle = x_hpool_alloc(&manager->pool);
  if (x_handle_is_null(handle))
  {
    free(data);
    return result;
  }
  info = x_hpool_get(&manager->pool, handle);
  if (!info)
  {
    x_hpool_free(&manager->pool, handle);
    free(data);
    return result;
  }
  info->type = LDK_ASSET_TYPE_TERRAIN;
  info->data = data;
  if (path)
  {
    info->asset_path = *path;
    info->source_revision = manager->source->revision;
  }
  result.h = handle;
  return result;
}

LDKAssetTerrain ldk_asset_manager_terrain_create(
    LDKAssetManager *manager, const LDKTerrain *terrain)
{
  return s_terrain_create(manager, terrain, NULL, NULL);
}

void ldk_asset_manager_terrain_unload(
    LDKAssetManager *manager, LDKAssetTerrain asset)
{
  if (ldk_asset_manager_terrain_is_alive(manager, asset))
  {
    x_hpool_free(&manager->pool, asset.h);
  }
}

typedef struct LDKTerrainFind
{
  LDKAssetPath path;
  u64 revision;
  LDKAssetTerrain asset;
} LDKTerrainFind;

static bool s_terrain_find(
    LDKAssetHandle handle, LDKAssetInfo *info, void *user)
{
  LDKTerrainFind *find = user;
  if (info->type == LDK_ASSET_TYPE_TERRAIN &&
      info->source_revision == find->revision &&
      strcmp(find->path.buf, info->asset_path.buf) == 0)
  {
    find->asset.h = handle.h;
    return false;
  }
  return true;
}

static bool s_terrain_path_valid(const char *path)
{
  static const char ext[] = ".terrain";
  const char *suffix = path ? strrchr(path, '.') : NULL;
  if (!suffix || strlen(suffix) != sizeof(ext) - 1u)
  {
    return false;
  }
  for (u32 i = 0u; i < sizeof(ext) - 1u; ++i)
  {
    if (tolower((unsigned char)suffix[i]) != ext[i])
    {
      return false;
    }
  }
  return true;
}

static bool s_terrain_read(LDKAssetManager *manager, const char *path,
    LDKTerrain *terrain, LDKImage **embedded)
{
  LDKAssetSourceFile file;
  u64 size;
  char *buffer;
  bool ok;
  if (!ldk_asset_source_find(manager->source, path, &file))
  {
    return false;
  }
  size = ldk_asset_source_file_size(&file);
  if (size > (u64)SIZE_MAX - 1u)
  {
    return false;
  }
  buffer = malloc((size_t)size + 1u);
  if (!buffer)
  {
    return false;
  }
  ok = ldk_asset_source_file_read(&file, buffer, size);
  buffer[size] = '\0';
  if (ok)
  {
    ok = ldk_terrain_from_tml_with_heightmap(terrain, embedded, buffer);
  }
  free(buffer);
  return ok;
}

LDKAssetTerrain ldk_asset_manager_terrain_load_shared(
    LDKAssetManager *manager, const char *path)
{
  LDKTerrainFind find = {0};
  LDKTerrain terrain;
  LDKAssetTerrain result = ldk_asset_terrain_null();
  if (!manager || !manager->source || !s_terrain_path_valid(path) ||
      !ldk_asset_path_set(&find.path, path))
  {
    return result;
  }
  find.revision = manager->source->revision;
  find.asset = result;
  ldk_asset_foreach(manager, s_terrain_find, &find);
  if (!x_handle_is_null(find.asset.h))
  {
    return find.asset;
  }
  ldk_terrain_init(&terrain);
  LDKImage *embedded = NULL;
  if (!s_terrain_read(manager, find.path.buf, &terrain, &embedded))
    return result;
  result = s_terrain_create(manager, &terrain, embedded, &find.path);
  if (x_handle_is_null(result.h)) ldk_image_destroy(embedded);
  return result;
}

bool ldk_asset_manager_terrain_update(LDKAssetManager *manager,
    LDKAssetTerrain asset, const LDKTerrain *terrain)
{
  LDKAssetHandle generic = {asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, generic);
  LDKAssetTerrainData *data = info && info->type == LDK_ASSET_TYPE_TERRAIN
      ? (LDKAssetTerrainData *)info->data : NULL;
  LDKAssetImage image;
  if (!data || !ldk_terrain_validate(terrain))
  {
    return false;
  }
  image = s_terrain_heightmap_resolve(manager, terrain);
  if (data->heightmap_preview &&
      (data->terrain.width != terrain->width ||
       data->terrain.height != terrain->height))
    data->heightmap_preview_enabled = false;
  data->terrain = *terrain;
  data->heightmap = image;
  ++data->revision;
  if (!data->revision)
  {
    data->revision = 1u;
  }
  return true;
}

/* Both terrain source images and editor preview images may be borrowed by
 * render/vegetation work. Retire replaced images until asset destruction. */
static bool s_preview_retire_image(LDKAssetTerrainData *data, LDKImage *image)
{
  if (!image) return true;
  if (data->heightmap_preview_retired_count == UINT32_MAX) return false;
  size_t n = (size_t)data->heightmap_preview_retired_count + 1u;
  if (n > SIZE_MAX / sizeof(*data->heightmap_preview_retired)) return false;
  LDKImage **items = realloc(data->heightmap_preview_retired,
      n * sizeof(*items));
  if (!items) return false;
  data->heightmap_preview_retired = items;
  items[data->heightmap_preview_retired_count++] = image;
  return true;
}

bool ldk_asset_manager_terrain_reload(
    LDKAssetManager *manager, const char *path)
{
  LDKTerrainFind find = {0};
  LDKTerrain terrain;
  LDKImage *embedded = NULL;
  if (!manager || !manager->source || !s_terrain_path_valid(path) ||
      !ldk_asset_path_set(&find.path, path))
  {
    return false;
  }
  find.revision = manager->source->revision;
  find.asset = ldk_asset_terrain_null();
  ldk_asset_foreach(manager, s_terrain_find, &find);
  if (x_handle_is_null(find.asset.h))
  {
    return true;
  }
  ldk_terrain_init(&terrain);
  if (!s_terrain_read(manager, find.path.buf, &terrain, &embedded))
    return false;
  LDKAssetHandle generic = {find.asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, generic);
  LDKAssetTerrainData *data = info && info->type == LDK_ASSET_TYPE_TERRAIN
      ? info->data : NULL;
  if (!data || !s_preview_retire_image(data, data->heightmap_embedded))
  {
    ldk_image_destroy(embedded);
    return false;
  }
  /* Commit only after parsing and retiring the former heightfield. Do not
   * free images that may still be borrowed by an active renderer frame. */
  data->terrain = terrain;
  data->heightmap = s_terrain_heightmap_resolve(manager, &terrain);
  data->heightmap_embedded = embedded;
  data->heightmap_preview_enabled = false;
  if (++data->revision == 0u) data->revision = 1u;
  return true;
}

/* The preview is a private copy: pixel modifications from the editor cannot
 * mutate a shared source-image asset or invalidate another reader's pointer. */
/* Retain superseded buffers for any TerrainSystem/GrassSystem readers that
 * still hold an older LDKImageInfo pointer. They are released at asset teardown. */
static bool s_preview_retire(LDKAssetTerrainData *data)
{
  if (!s_preview_retire_image(data, data->heightmap_preview)) return false;
  data->heightmap_preview = NULL;
  return true;
}

bool ldk_asset_manager_terrain_preview_set(LDKAssetManager *manager,
    LDKAssetTerrain asset, const LDKImageInfo *image)
{
  LDKAssetHandle handle = {asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, handle);
  LDKAssetTerrainData *data = info && info->type == LDK_ASSET_TYPE_TERRAIN
      ? info->data : NULL;
  LDKImageInfo old = {0};
  if (!data || !image || !image->pixels ||
      image->width != data->terrain.width ||
      image->height != data->terrain.height ||
      image->channel_count != 4u ||
      (image->format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
       image->format != LDK_IMAGE_FORMAT_RGBA16_UNORM) ||
      image->bytes_per_channel !=
          (image->format == LDK_IMAGE_FORMAT_RGBA16_UNORM ? 2u : 1u) ||
      image->byte_count != (u64)image->width * image->height * 4u *
          image->bytes_per_channel)
    return false;

  if (data->heightmap_preview &&
      ldk_image_get_info(data->heightmap_preview, &old) &&
      old.width == image->width && old.height == image->height &&
      old.format == image->format)
  {
    if (!ldk_image_pixels_set(data->heightmap_preview, image->pixels,
            image->byte_count)) return false;
  }
  else
  {
    LDKImage *copy = ldk_image_create_format(image->width, image->height,
        image->format, image->pixels);
    if (!copy) return false;
    if (!s_preview_retire(data))
    {
      ldk_image_destroy(copy);
      return false;
    }
    data->heightmap_preview = copy;
  }
  data->heightmap_preview_enabled = true;
  if (++data->revision == 0u) data->revision = 1u;
  return true;
}

bool ldk_asset_manager_terrain_preview_clear(
    LDKAssetManager *manager, LDKAssetTerrain asset)
{
  LDKAssetHandle handle = {asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, handle);
  LDKAssetTerrainData *data = info && info->type == LDK_ASSET_TYPE_TERRAIN
      ? info->data : NULL;
  if (!data) return false;
  if (data->heightmap_preview_enabled)
  {
    data->heightmap_preview_enabled = false;
    if (++data->revision == 0u) data->revision = 1u;
  }
  return true;
}
