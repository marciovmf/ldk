#include <ldk_skybox_asset.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void s_error(LDKSkyboxIOResult *result, const char *message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error), "%s", message);
  }
}

LDKAssetSkybox ldk_asset_skybox_null(void)
{
  LDKAssetSkybox asset = {x_handle_null()};
  return asset;
}

static LDKAssetInfo *s_info(LDKAssetManager *manager, LDKAssetSkybox asset)
{
  LDKAssetHandle handle = {asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, handle);
  return info && info->type == LDK_ASSET_TYPE_SKYBOX ? info : NULL;
}

const LDKAssetSkyboxData *ldk_asset_manager_skybox_get_const(
    LDKAssetManager *manager, LDKAssetSkybox asset)
{
  LDKAssetInfo *info = s_info(manager, asset);
  return info ? info->data : NULL;
}

static bool s_path(const LDKSkyboxIOContext *context, const char *path,
    LDKAssetPath *asset_path, LDKSkyboxIOResult *result)
{
  s_error(result, "");
  if (!context || !context->assets || !context->assets->source ||
      !ldk_asset_path_set(asset_path, path))
  {
    s_error(result, "skybox asset requires a valid asset path");
    return false;
  }
  return true;
}

static LDKAssetSkybox s_find(
    LDKAssetManager *manager, const LDKAssetPath *path)
{
  XHPoolIter it = {0};
  XHandle h = x_handle_null();

  for (LDKAssetInfo *info = x_hpool_iter_begin(&manager->pool, &it, &h);
       info; info = x_hpool_iter_next(&manager->pool, &it, &h))
  {
    if (info->type == LDK_ASSET_TYPE_SKYBOX &&
        info->source_revision == manager->source->revision &&
        strcmp(info->asset_path.buf, path->buf) == 0)
    {
      LDKAssetSkybox asset = {h};
      return asset;
    }
  }

  return ldk_asset_skybox_null();
}

static LDKAssetSkybox s_insert(LDKAssetManager *manager,
    const LDKAssetPath *path, const LDKSkyboxDesc *descriptor,
    bool dirty, LDKSkyboxIOResult *result)
{
  LDKAssetSkybox asset = ldk_asset_skybox_null();
  LDKAssetSkyboxData *data = malloc(sizeof(*data));

  if (data)
  {
    asset.h = x_hpool_alloc(&manager->pool);
    if (!x_handle_is_null(asset.h))
    {
      LDKAssetInfo *info = x_hpool_get(&manager->pool, asset.h);
      data->descriptor = *descriptor;
      data->revision = 1;
      data->dirty = dirty;
      info->type = LDK_ASSET_TYPE_SKYBOX;
      info->asset_path = *path;
      info->source_revision = manager->source->revision;
      info->data = data;
      info->load_timestamp = dirty ? 0 : (u64)time(NULL);
      return asset;
    }
    free(data);
  }

  s_error(result, "failed to allocate skybox asset");
  return asset;
}

LDKAssetSkybox ldk_asset_manager_skybox_create(
    const LDKSkyboxIOContext *context, const char *path,
    const LDKSkyboxDesc *descriptor, LDKSkyboxIOResult *result)
{
  LDKAssetPath asset_path;
  LDKAssetSourceFile existing;

  if (!s_path(context, path, &asset_path, result))
  {
    return ldk_asset_skybox_null();
  }
  if (!ldk_skybox_desc_is_valid(descriptor))
  {
    s_error(result, "invalid skybox descriptor");
    return ldk_asset_skybox_null();
  }
  if (!x_handle_is_null(s_find(context->assets, &asset_path).h) ||
      ldk_asset_source_find(context->assets->source,
          asset_path.buf, &existing))
  {
    s_error(result, "skybox asset path already exists");
    return ldk_asset_skybox_null();
  }

  return s_insert(context->assets, &asset_path, descriptor, true, result);
}

LDKAssetSkybox ldk_asset_manager_skybox_load_shared(
    const LDKSkyboxIOContext *context, const char *path,
    LDKSkyboxIOResult *result)
{
  LDKAssetPath asset_path;
  LDKAssetSourceFile file;
  LDKAssetSkybox asset = ldk_asset_skybox_null();
  u64 size;
  char *text;

  if (!s_path(context, path, &asset_path, result))
  {
    return asset;
  }

  asset = s_find(context->assets, &asset_path);
  if (!x_handle_is_null(asset.h))
  {
    return asset;
  }

  if (!ldk_asset_source_find(context->assets->source,
          asset_path.buf, &file))
  {
    s_error(result, "skybox asset is missing");
    return ldk_asset_skybox_null();
  }

  size = ldk_asset_source_file_size(&file);
  if (size > (u64)SIZE_MAX - 1u)
  {
    s_error(result, "skybox asset is too large");
    return asset;
  }

  text = malloc((size_t)size + 1u);
  if (!text || !ldk_asset_source_file_read(&file, text, size))
  {
    free(text);
    s_error(result, "cannot read skybox asset");
    return asset;
  }
  text[size] = 0;
  if (memchr(text, 0, (size_t)size))
  {
    free(text);
    s_error(result, "cannot read skybox asset");
    return asset;
  }

  TMLParseResult parsed = tml_parse(text);
  free(text);
  if (!parsed.ok)
  {
    s_error(result, parsed.error);
    return asset;
  }

  const TMLNode *node = tml_root_node_at(parsed.document, 0);
  LDKSkyboxDesc descriptor;
  if (parsed.document->root_node_count != 1 || !node ||
      node->name.size != 6 || memcmp(node->name.data, "skybox", 6) != 0)
  {
    s_error(result, "skybox file must contain one skybox node");
  }
  else if (ldk_skybox_desc_read(context, parsed.document, node,
               &descriptor, result))
  {
    asset = s_insert(
        context->assets, &asset_path, &descriptor, false, result);
  }

  tml_document_free(parsed.document);
  return asset;
}

bool ldk_asset_manager_skybox_update(LDKAssetManager *manager,
    LDKAssetSkybox asset, const LDKSkyboxDesc *descriptor)
{
  LDKAssetInfo *info = s_info(manager, asset);
  if (!info || !ldk_skybox_desc_is_valid(descriptor))
  {
    return false;
  }

  LDKAssetSkyboxData *data = info->data;
  if (ldk_skybox_desc_equal(&data->descriptor, descriptor))
  {
    return true;
  }
  if (data->revision == UINT64_MAX)
  {
    return false;
  }

  data->descriptor = *descriptor;
  ++data->revision;
  data->dirty = true;
  return true;
}

bool ldk_asset_manager_skybox_save(const LDKSkyboxIOContext *context,
    LDKAssetSkybox asset, LDKSkyboxIOResult *result)
{
  s_error(result, "");
  LDKAssetInfo *info = context ? s_info(context->assets, asset) : NULL;
  if (!info || !context->assets->source ||
      info->source_revision != context->assets->source->revision)
  {
    s_error(result, "invalid skybox asset");
    return false;
  }

  LDKAssetPath asset_path;
  if (!s_path(context, info->asset_path.buf, &asset_path, result))
  {
    return false;
  }

  LDKAssetSkyboxData *data = info->data;
  XStrBuilder *out = x_strbuilder_create();
  if (!out)
  {
    s_error(result, "failed to allocate skybox output");
    return false;
  }

  x_strbuilder_append(out, "skybox:\n");
  bool ok = ldk_skybox_desc_write(
      context, &data->descriptor, out, 1, result);
  if (ok && !ldk_asset_source_file_write(context->assets->source,
                asset_path.buf, out->data, (u64)out->length))
  {
    s_error(result, "failed to write skybox file");
    ok = false;
  }

  x_strbuilder_destroy(out);
  if (ok)
  {
    data->dirty = false;
  }
  return ok;
}
