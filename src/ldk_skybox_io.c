#include <ldk_skybox_io.h>

#include <stdio.h>
#include <string.h>

static const char *s_face_names[LDK_SKYBOX_FACE_COUNT] = {
    "positive_x",
    "negative_x",
    "positive_y",
    "negative_y",
    "positive_z",
    "negative_z",
};

static void s_error(LDKSkyboxIOResult *result, const char *message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error), "%s", message);
  }
}

static void s_append_indent(XStrBuilder *out, u32 indent)
{
  for (u32 i = 0; i < indent; ++i)
  {
    x_strbuilder_append(out, "  ");
  }
}

static void s_append_escaped_string(XStrBuilder *out, const char *text)
{
  const char *cursor = text;

  x_strbuilder_append_char(out, '"');
  while (cursor && *cursor)
  {
    switch (*cursor)
    {
    case '\n':
      x_strbuilder_append(out, "\\n");
      break;
    case '\r':
      x_strbuilder_append(out, "\\r");
      break;
    case '\t':
      x_strbuilder_append(out, "\\t");
      break;
    case '"':
      x_strbuilder_append(out, "\\\"");
      break;
    case '\\':
      x_strbuilder_append(out, "\\\\");
      break;
    default:
      x_strbuilder_append_char(out, *cursor);
      break;
    }
    ++cursor;
  }
  x_strbuilder_append_char(out, '"');
}

static bool s_face_read(const LDKSkyboxIOContext *context,
    const TMLDocument *doc, const TMLNode *fields, const char *field_name,
    LDKAssetImage *out_image, LDKSkyboxIOResult *result)
{
  TMLString image_path;
  char path[LDK_ASSET_PATH_MAX_LENGTH + 1u] = {0};
  LDKAssetPath asset_path;

  if (!tml_node_get_string(doc, fields, field_name, &image_path) ||
      image_path.size == 0 || image_path.size >= sizeof(path) ||
      memchr(image_path.data, 0, image_path.size))
  {
    s_error(result, "skybox face needs a valid image asset path");
    return false;
  }

  memcpy(path, image_path.data, image_path.size);
  if (!ldk_asset_path_set(&asset_path, path))
  {
    s_error(result, "skybox face has an invalid image asset path");
    return false;
  }

  *out_image = ldk_asset_manager_image_load_shared(
      context->assets, asset_path.buf);
  if (x_handle_is_null(out_image->h))
  {
    s_error(result, "skybox face image is unavailable");
    return false;
  }

  const LDKAssetImageData *data =
      ldk_asset_manager_image_get_const(context->assets, *out_image);
  if (!data || data->is_missing)
  {
    s_error(result, "skybox face image is unavailable");
    return false;
  }

  return true;
}

static bool s_face_write(const LDKSkyboxIOContext *context,
    LDKAssetImage image, const char *field_name, XStrBuilder *out,
    u32 indent, LDKSkyboxIOResult *result)
{
  LDKAssetHandle handle = {image.h};
  const LDKAssetInfo *info =
      ldk_asset_get_info_const(context->assets, handle);
  LDKAssetPath asset_path;

  if (!info || info->type != LDK_ASSET_TYPE_IMAGE ||
      !context->assets->source ||
      info->source_revision != context->assets->source->revision ||
      !ldk_asset_path_set(&asset_path, info->asset_path.buf))
  {
    s_error(result, "skybox face needs a valid image asset path to save");
    return false;
  }

  s_append_indent(out, indent);
  x_strbuilder_append_format(out, "%s: ", field_name);
  s_append_escaped_string(out, asset_path.buf);
  x_strbuilder_append_char(out, '\n');
  return true;
}

bool ldk_skybox_desc_read(const LDKSkyboxIOContext *context,
    const TMLDocument *doc, const TMLNode *fields,
    LDKSkyboxDesc *out_desc, LDKSkyboxIOResult *result)
{
  LDKSkyboxDesc desc;

  if (result)
  {
    result->error[0] = 0;
  }
  if (!context || !context->assets || !doc || !fields || !out_desc)
  {
    s_error(result, "invalid skybox read arguments");
    return false;
  }

  ldk_skybox_desc_defaults(&desc);
  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    if (!s_face_read(context, doc, fields, s_face_names[i],
            &desc.faces[i], result))
    {
      return false;
    }
  }

  *out_desc = desc;
  return true;
}

bool ldk_skybox_desc_write(const LDKSkyboxIOContext *context,
    const LDKSkyboxDesc *desc, XStrBuilder *out, u32 indent,
    LDKSkyboxIOResult *result)
{
  if (result)
  {
    result->error[0] = 0;
  }
  if (!context || !context->assets || !desc || !out ||
      !ldk_skybox_desc_is_valid(desc))
  {
    s_error(result, "invalid skybox write arguments");
    return false;
  }

  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    if (!s_face_write(context, desc->faces[i], s_face_names[i], out,
            indent, result))
    {
      return false;
    }
  }

  return true;
}
