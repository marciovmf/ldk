/* Manager-owned texture atlas assets. Images are shared, never individually freed. */
#include <module/ldk_asset_manager.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

LDKAssetTextureAtlas ldk_asset_texture_atlas_null(void)
{
  LDKAssetTextureAtlas out = {x_handle_null()};
  return out;
}

bool ldk_asset_manager_texture_atlas_is_alive(
    LDKAssetManager *manager, LDKAssetTextureAtlas asset)
{
  LDKAssetHandle generic = {asset.h};
  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_TEXTURE_ATLAS;
}

const LDKAssetTextureAtlasData *ldk_asset_manager_texture_atlas_get_const(
    LDKAssetManager *manager, LDKAssetTextureAtlas asset)
{
  LDKAssetHandle generic = {asset.h};
  const LDKAssetInfo *info = ldk_asset_get_info_const(manager, generic);
  return info && info->type == LDK_ASSET_TYPE_TEXTURE_ATLAS ? info->data : NULL;
}

static LDKAssetTextureAtlas s_atlas_create(LDKAssetManager *manager,
    const LDKTextureAtlas *atlas, const LDKAssetPath *path)
{
  LDKAssetTextureAtlas out = ldk_asset_texture_atlas_null();
  if (!manager || !ldk_texture_atlas_validate(atlas))
  {
    return out;
  }
  LDKAssetTextureAtlasData *data = calloc(1, sizeof(*data));
  if (!data)
  {
    return out;
  }
  ldk_texture_atlas_init(&data->atlas);
  data->image = ldk_asset_image_null();
  data->revision = 1u;
  if (!ldk_texture_atlas_copy(&data->atlas, atlas))
  {
    free(data);
    return out;
  }
  if (data->atlas.texture_path.buf[0])
  {
    data->image = ldk_asset_manager_image_load_shared(manager,
        data->atlas.texture_path.buf);
    /* A missing image must not prevent inspecting/editing atlas metadata. */
  }
  XHandle handle = x_hpool_alloc(&manager->pool);
  if (x_handle_is_null(handle))
  {
    ldk_texture_atlas_clear(&data->atlas);
    free(data);
    return out;
  }
  LDKAssetInfo *info = x_hpool_get(&manager->pool, handle);
  if (!info)
  {
    x_hpool_free(&manager->pool, handle);
    ldk_texture_atlas_clear(&data->atlas);
    free(data);
    return out;
  }
  info->type = LDK_ASSET_TYPE_TEXTURE_ATLAS;
  info->data = data;
  if (path)
  {
    info->asset_path = *path;
    info->source_revision = manager->source->revision;
  }
  out.h = handle;
  return out;
}

LDKAssetTextureAtlas ldk_asset_manager_texture_atlas_create(
    LDKAssetManager *manager, const LDKTextureAtlas *atlas)
{
  return s_atlas_create(manager, atlas, NULL);
}

void ldk_asset_manager_texture_atlas_unload(
    LDKAssetManager *manager, LDKAssetTextureAtlas asset)
{
  if (ldk_asset_manager_texture_atlas_is_alive(manager, asset))
  {
    x_hpool_free(&manager->pool, asset.h);
  }
}

typedef struct LDKAtlasFind
{
  LDKAssetPath path;
  u64 revision;
  LDKAssetTextureAtlas atlas;
} LDKAtlasFind;

static bool s_atlas_find(LDKAssetHandle handle, LDKAssetInfo *info, void *user)
{
  LDKAtlasFind *find = user;
  if (info->type == LDK_ASSET_TYPE_TEXTURE_ATLAS &&
      info->source_revision == find->revision &&
      strcmp(find->path.buf, info->asset_path.buf) == 0)
  {
    find->atlas.h = handle.h;
    return false;
  }
  return true;
}

static bool s_atlas_path_valid(const char *path)
{
  if (!path)
  {
    return false;
  }
  const char *ext = strrchr(path, '.');
  if (!ext || strlen(ext) != 6)
  {
    return false;
  }
  const char *expected = ".atlas";
  for (u32 i = 0; i < 6; ++i)
  {
    if (tolower((unsigned char)ext[i]) != expected[i])
    {
      return false;
    }
  }
  return true;
}

static bool s_atlas_read(LDKAssetManager *manager, const char *path,
    LDKTextureAtlas *atlas)
{
  LDKAssetSourceFile file;
  if (!ldk_asset_source_find(manager->source, path, &file))
  {
    return false;
  }
  u64 byte_count = ldk_asset_source_file_size(&file);
  if (byte_count > SIZE_MAX - 1u)
  {
    return false;
  }
  char *buffer = malloc((size_t)byte_count + 1u);
  if (!buffer)
  {
    return false;
  }
  bool ok = ldk_asset_source_file_read(&file, buffer, byte_count);
  buffer[byte_count] = 0;
  if (ok)
  {
    ok = ldk_texture_atlas_from_tml(atlas, buffer);
  }
  free(buffer);
  return ok;
}

LDKAssetTextureAtlas ldk_asset_manager_texture_atlas_load_shared(
    LDKAssetManager *manager, const char *path)
{
  LDKAssetTextureAtlas null = ldk_asset_texture_atlas_null();
  LDKAtlasFind find = {0};
  if (!manager || !manager->source || !s_atlas_path_valid(path) ||
      !ldk_asset_path_set(&find.path, path))
  {
    return null;
  }
  find.revision = manager->source->revision;
  find.atlas = null;
  ldk_asset_foreach(manager, s_atlas_find, &find);
  if (!x_handle_is_null(find.atlas.h))
  {
    return find.atlas;
  }
  LDKTextureAtlas atlas;
  ldk_texture_atlas_init(&atlas);
  if (!s_atlas_read(manager, find.path.buf, &atlas))
  {
    ldk_texture_atlas_clear(&atlas);
    return null;
  }
  LDKAssetTextureAtlas out = s_atlas_create(manager, &atlas, &find.path);
  ldk_texture_atlas_clear(&atlas);
  return out;
}

bool ldk_asset_manager_texture_atlas_reload(LDKAssetManager *manager,
    const char *path)
{
  LDKAtlasFind find = {0};
  if (!manager || !manager->source || !s_atlas_path_valid(path) ||
      !ldk_asset_path_set(&find.path, path))
  {
    return false;
  }
  find.revision = manager->source->revision;
  find.atlas = ldk_asset_texture_atlas_null();
  ldk_asset_foreach(manager, s_atlas_find, &find);
  if (x_handle_is_null(find.atlas.h))
  {
    return true;
  }
  LDKTextureAtlas candidate;
  ldk_texture_atlas_init(&candidate);
  if (!s_atlas_read(manager, path, &candidate))
  {
    ldk_texture_atlas_clear(&candidate);
    return false;
  }
  LDKAssetHandle generic = {find.atlas.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, generic);
  LDKAssetTextureAtlasData *data = info ? info->data : NULL;
  if (!data)
  {
    ldk_texture_atlas_clear(&candidate);
    return false;
  }
  LDKAssetImage image = ldk_asset_image_null();
  if (candidate.texture_path.buf[0])
  {
    image = ldk_asset_manager_image_load_shared(manager, candidate.texture_path.buf);
  }
  ldk_texture_atlas_clear(&data->atlas);
  data->atlas = candidate;
  data->image = image;
  ++data->revision;
  return true;
}
