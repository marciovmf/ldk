#include <ldk_material_io.h>
#include <ldk.h>

#include <stdio.h>
#include <string.h>

static void s_result_error(LDKMaterialIOResult *result, const char *message)
{
  if (result)
    snprintf(result->error, sizeof(result->error), "%s", message);
}

static bool s_node_get_u32(const TMLDocument *doc, const TMLNode *node,
    const char *name, u32 *out_value)
{
  const TMLEntry *entry;
  i64 value;

  if (!out_value)
  {
    return false;
  }

  entry = tml_node_find_entry(doc, node, name);
  if (!entry || !tml_entry_get_i64(entry, &value) || value < 0 ||
      (u64)value > UINT32_MAX)
  {
    return false;
  }

  *out_value = (u32)value;
  return true;
}

static void s_append_indent(XStrBuilder *out, u32 indent)
{
  u32 i;

  for (i = 0; i < indent; i++)
  {
    x_strbuilder_append(out, "  ");
  }
}

static void s_append_escaped_string(XStrBuilder *out, const char *text)
{
  const char *cursor = text;

  x_strbuilder_append_char(out, '"');

  if (cursor)
  {
    while (*cursor)
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

      cursor++;
    }
  }

  x_strbuilder_append_char(out, '"');
}

static void s_report_missing_image(
    const LDKMaterialIOContext *context, const char *path)
{
  char message[512];
  snprintf(message, sizeof(message),
      "Material texture '%s' is unavailable; using magenta checkerboard.",
      path ? path : "<unassigned>");
  ldk_log_error("%s\n", message);
  if (context->diagnostic)
    context->diagnostic(message, context->user);
}

/* Material image paths are relative to the configured project runtree. */
static bool s_material_image_path(const LDKMaterialIOContext *context,
    const char *path, XFSPath *absolute,
    XFSPath *relative)
{
  if (!context->runtree_path.length || !path || !path[0])
    return false;
  XFSPath root = context->runtree_path;
  x_fs_path_normalize(&root);
  if (x_fs_path_is_absolute_cstr(path))
  {
    if (strlen(path) >= sizeof(absolute->buf))
      return false;
    x_fs_path_set(absolute, path);
  }
  else
  {
    if (root.length + 1 + strlen(path) >= sizeof(absolute->buf))
      return false;
    x_fs_path(absolute, root.buf, path);
  }
  x_fs_path_normalize(absolute);
  return x_fs_path_common_prefix(root.buf, absolute->buf, relative) &&
      relative->length && strcmp(relative->buf, ".") != 0;
}

bool ldk_material_desc_read(const LDKMaterialIOContext *context,
    const TMLDocument *doc, const TMLNode *fields,
    LDKMaterialDesc *out_desc, LDKMaterialIOResult *result)
{
  if (result)
    result->error[0] = 0;
  if (!context || !doc || !fields || !out_desc)
  {
    s_result_error(result, "invalid material read arguments");
    return false;
  }
  u32 type, color = 0xffffffffu;
  LDKMaterialDesc desc;
  if (!s_node_get_u32(doc, fields, "material_type", &type) ||
      !ldk_material_desc_defaults((LDKMaterialType)type, &desc) ||
      (tml_node_find_entry(doc, fields, "material_color") &&
          !s_node_get_u32(doc, fields, "material_color", &color)))
  {
    s_result_error(result, "invalid material type or color");
    return false;
  }
  bool textured = desc.type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc.type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  if (textured)
  {
    if (!context->assets)
    {
      s_result_error(result, "material read requires an asset manager");
      return false;
    }
    desc.args.textured.color = color;
    if (tml_node_find_entry(doc, fields, "material_texture"))
    {
      TMLString image_path;
      XFSPath path = {0}, absolute = {0}, relative = {0};
      if (!tml_node_get_string(doc, fields, "material_texture", &image_path) ||
          image_path.size >= sizeof(path.buf) ||
          memchr(image_path.data, 0, image_path.size))
      {
        s_result_error(result, "invalid material image path");
        return false;
      }
      memcpy(path.buf, image_path.data, image_path.size);
      if (path.buf[0])
      {
        if (x_fs_path_is_absolute_cstr(path.buf) ||
            !s_material_image_path(context, path.buf, &absolute, &relative))
        {
          s_result_error(result, "material image must be inside the runtree");
          return false;
        }
        desc.args.textured.texture = ldk_asset_manager_image_load_shared(
            context->assets, absolute.buf);
        if (x_handle_is_null(desc.args.textured.texture.h))
        {
          desc.args.textured.texture = ldk_asset_manager_image_missing(
              context->assets, absolute.buf);
          if (x_handle_is_null(desc.args.textured.texture.h))
          {
            s_result_error(result, "failed to allocate missing texture");
            return false;
          }
        }
        const LDKAssetImageData *image = ldk_asset_manager_image_get_const(
            context->assets, desc.args.textured.texture);
        if (image && image->is_missing)
          s_report_missing_image(context, absolute.buf);
      }
    }
    if (x_handle_is_null(desc.args.textured.texture.h))
      s_report_missing_image(context, NULL);
  }
  else
    desc.args.vertex_color.color = color;
  *out_desc = desc;
  return true;
}

bool ldk_material_desc_write(const LDKMaterialIOContext *context,
    const LDKMaterialDesc *desc, XStrBuilder *out, u32 indent,
    LDKMaterialIOResult *result)
{
  if (result)
    result->error[0] = 0;
  if (!context || !out)
  {
    s_result_error(result, "invalid material write arguments");
    return false;
  }
  if (!ldk_material_desc_is_valid(desc))
  {
    s_result_error(result, "cannot save an invalid material");
    return false;
  }
  bool textured = desc->type == LDK_MATERIAL_TYPE_TEXTURED ||
      desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT;
  rgba32 color = textured ? desc->args.textured.color
                         : desc->args.vertex_color.color;
  s_append_indent(out, indent);
  x_strbuilder_append_format(out, "material_type: %u\n",
      (u32)desc->type);
  s_append_indent(out, indent);
  x_strbuilder_append_format(out, "material_color: %u\n", color);
  if (textured)
  {
    XFSPath absolute = {0}, relative = {0};
    if (!x_handle_is_null(desc->args.textured.texture.h))
    {
      LDKAssetHandle handle = {desc->args.textured.texture.h};
      const LDKAssetInfo *info = ldk_asset_get_info_const(
          context->assets, handle);
      if (!info || info->type != LDK_ASSET_TYPE_IMAGE ||
          !s_material_image_path(context, info->asset_path.buf,
              &absolute, &relative))
      {
        s_result_error(result,
            "material image needs a file inside the project runtree to save");
        return false;
      }
    }
    s_append_indent(out, indent);
    x_strbuilder_append(out, "material_texture: ");
    s_append_escaped_string(out, relative.buf);
    x_strbuilder_append_char(out, '\n');
  }
  return true;
}

