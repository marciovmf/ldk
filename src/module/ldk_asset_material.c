#include <ldk_material_asset.h>
#include <ldk.h>
#include <errno.h>
#include <stdx/stdx_io.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void s_error(LDKMaterialIOResult *result, const char *message)
{
  if (result)
    snprintf(result->error, sizeof(result->error), "%s", message);
}

LDKAssetMaterial ldk_asset_material_null(void)
{
  LDKAssetMaterial asset = {x_handle_null()};
  return asset;
}

static LDKAssetInfo *s_info(LDKAssetManager *manager, LDKAssetMaterial asset)
{
  LDKAssetHandle handle = {asset.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, handle);
  return info && info->type == LDK_ASSET_TYPE_MATERIAL ? info : NULL;
}

const LDKAssetMaterialData *ldk_asset_manager_material_get_const(
    LDKAssetManager *manager, LDKAssetMaterial asset)
{
  LDKAssetInfo *info = s_info(manager, asset);
  return info ? info->data : NULL;
}

static bool s_path(const LDKMaterialIOContext *context, const char *path,
    XFSPath *absolute, LDKMaterialIOResult *result)
{
  s_error(result, "");
  if (!context || !context->assets || !path || !path[0] ||
      !context->runtree_path.length ||
      !x_fs_path_is_absolute_cstr(context->runtree_path.buf))
  {
    s_error(result, "material asset requires an absolute runtree and a path");
    return false;
  }
  XFSPath root = context->runtree_path, relative = {0};
  x_fs_path_normalize(&root);
  bool is_absolute = x_fs_path_is_absolute_cstr(path);
  if (strlen(path) + (is_absolute ? 0 : root.length + 1) >=
      sizeof(absolute->buf))
  {
    s_error(result, "material asset path is too long");
    return false;
  }
  if (is_absolute)
    x_fs_path_set(absolute, path);
  else
    x_fs_path(absolute, root.buf, path);
  x_fs_path_normalize(absolute);
  if (!x_fs_path_common_prefix(root.buf, absolute->buf, &relative) ||
      !relative.length || strcmp(relative.buf, ".") == 0)
  {
    s_error(result, "material asset must be inside the project runtree");
    return false;
  }
  return true;
}

static LDKAssetMaterial s_find(LDKAssetManager *manager, const XFSPath *path)
{
  XHPoolIter it = {0};
  XHandle h = x_handle_null();
  for (LDKAssetInfo *info = x_hpool_iter_begin(&manager->pool, &it, &h);
       info; info = x_hpool_iter_next(&manager->pool, &it, &h))
  {
    if (info->type == LDK_ASSET_TYPE_MATERIAL &&
        strcmp(info->asset_path.buf, path->buf) == 0)
    {
      LDKAssetMaterial asset = {h};
      return asset;
    }
  }
  return ldk_asset_material_null();
}

static LDKAssetMaterial s_insert(LDKAssetManager *manager, const XFSPath *path,
    const LDKMaterialDesc *descriptor, bool dirty, LDKMaterialIOResult *result)
{
  LDKAssetMaterial asset = ldk_asset_material_null();
  LDKAssetMaterialData *data = malloc(sizeof(*data));
  if (data)
  {
    asset.h = x_hpool_alloc(&manager->pool);
    if (!x_handle_is_null(asset.h))
    {
      LDKAssetInfo *info = x_hpool_get(&manager->pool, asset.h);
      data->descriptor = *descriptor;
      data->revision = 1;
      data->dirty = dirty;
      data->is_missing = false;
      info->type = LDK_ASSET_TYPE_MATERIAL;
      info->asset_path = *path;
      info->data = data;
      info->load_timestamp = dirty ? 0 : (u64)time(NULL);
      return asset;
    }
    free(data);
  }
  s_error(result, "failed to allocate material asset");
  return asset;
}

static void s_report_missing(const LDKMaterialIOContext *context,
    const char *path)
{
  char message[512];
  snprintf(message, sizeof(message),
      "Material '%s' is missing; using unlit magenta checker material.", path);
  ldk_log_error("%s\n", message);
  if (context->diagnostic)
    context->diagnostic(message, context->user);
}

static LDKAssetMaterial s_missing(const LDKMaterialIOContext *context,
    const XFSPath *path, LDKMaterialIOResult *result)
{
  /* Reuse the manager-owned procedural missing-image checker. */
  LDKMaterialDesc descriptor;
  ldk_material_desc_defaults(LDK_MATERIAL_TYPE_TEXTURED_UNLIT, &descriptor);
  descriptor.args.textured.texture =
      ldk_asset_manager_image_missing(context->assets, NULL);
  if (x_handle_is_null(descriptor.args.textured.texture.h))
  {
    s_error(result, "failed to allocate missing material texture");
    return ldk_asset_material_null();
  }
  LDKAssetMaterial asset = s_insert(
      context->assets, path, &descriptor, false, result);
  if (x_handle_is_null(asset.h))
    return asset;
  LDKAssetInfo *info = s_info(context->assets, asset);
  ((LDKAssetMaterialData *)info->data)->is_missing = true;
  info->load_timestamp = 0;
  s_report_missing(context, path->buf);
  return asset;
}

LDKAssetMaterial ldk_asset_manager_material_create(
    const LDKMaterialIOContext *context, const char *path,
    const LDKMaterialDesc *descriptor, LDKMaterialIOResult *result)
{
  XFSPath absolute = {0};
  if (!s_path(context, path, &absolute, result))
    return ldk_asset_material_null();
  if (!ldk_material_desc_is_valid(descriptor))
  {
    s_error(result, "invalid material descriptor");
    return ldk_asset_material_null();
  }
  if (!x_handle_is_null(s_find(context->assets, &absolute).h) ||
      x_fs_path_exists(&absolute))
  {
    s_error(result, "material asset path already exists");
    return ldk_asset_material_null();
  }
  return s_insert(context->assets, &absolute, descriptor, true, result);
}

LDKAssetMaterial ldk_asset_manager_material_load_shared(
    const LDKMaterialIOContext *context, const char *path,
    LDKMaterialIOResult *result)
{
  XFSPath absolute = {0};
  if (!s_path(context, path, &absolute, result))
    return ldk_asset_material_null();
  LDKAssetMaterial asset = s_find(context->assets, &absolute);
  if (!x_handle_is_null(asset.h))
  {
    if (ldk_asset_manager_material_get_const(context->assets, asset)->is_missing)
      s_report_missing(context, absolute.buf);
    return asset;
  }
  size_t size = 0;
  errno = 0;
  char *text = x_io_read_text(absolute.buf, &size);
  if (!text && (errno == ENOENT || errno == ENOTDIR))
    return s_missing(context, &absolute, result);
  if (!text || memchr(text, 0, size))
  {
    free(text);
    s_error(result, "cannot read material file");
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
  LDKMaterialDesc descriptor;
  if (parsed.document->root_node_count != 1 || !node ||
      node->name.size != 8 || memcmp(node->name.data, "material", 8) != 0)
    s_error(result, "material file must contain one material node");
  else if (ldk_material_desc_read(context, parsed.document, node,
               &descriptor, result))
    asset = s_insert(context->assets, &absolute, &descriptor, false, result);
  tml_document_free(parsed.document);
  return asset;
}

bool ldk_asset_manager_material_update(LDKAssetManager *manager,
    LDKAssetMaterial asset, const LDKMaterialDesc *descriptor)
{
  LDKAssetInfo *info = s_info(manager, asset);
  if (!info || !ldk_material_desc_is_valid(descriptor))
    return false;
  LDKAssetMaterialData *data = info->data;
  if (data->is_missing)
    return false;
  if (ldk_material_desc_equal(&data->descriptor, descriptor))
    return true;
  if (data->revision == UINT64_MAX)
    return false;
  data->descriptor = *descriptor;
  ++data->revision;
  data->dirty = true;
  return true;
}

bool ldk_asset_manager_material_save(const LDKMaterialIOContext *context,
    LDKAssetMaterial asset, LDKMaterialIOResult *result)
{
  s_error(result, "");
  LDKAssetInfo *info = context ? s_info(context->assets, asset) : NULL;
  if (!info)
  {
    s_error(result, "invalid material asset");
    return false;
  }
  XFSPath absolute = {0};
  if (!s_path(context, info->asset_path.buf, &absolute, result))
    return false;
  LDKAssetMaterialData *data = info->data;
  if (data->is_missing)
  {
    s_error(result, "missing material placeholders cannot be saved");
    return false;
  }
  XStrBuilder *out = x_strbuilder_create();
  if (!out)
  {
    s_error(result, "failed to allocate material output");
    return false;
  }
  x_strbuilder_append(out, "material:\n");
  bool ok = ldk_material_desc_write(context, &data->descriptor, out, 1, result);
  if (ok)
  {
    FILE *file = fopen(absolute.buf, "wb");
    if (!file)
      ok = false;
    else
    {
      ok = fwrite(out->data, 1, out->length, file) == out->length;
      if (fclose(file) != 0)
        ok = false;
    }
    if (!ok)
      s_error(result, "failed to write material file");
  }
  x_strbuilder_destroy(out);
  if (ok)
    data->dirty = false;
  return ok;
}
