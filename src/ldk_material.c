#include <ldk_material.h>

#include <string.h>

#define LDK_MATERIAL_HASH_OFFSET 14695981039346656037ull
#define LDK_MATERIAL_HASH_PRIME 1099511628211ull

static u64 s_material_hash_u32(u64 hash, u32 value)
{
  for (u32 i = 0; i < 4; i++)
  {
    hash ^= (u8)(value & 0xffu);
    hash *= LDK_MATERIAL_HASH_PRIME;
    value >>= 8;
  }

  return hash;
}

static bool s_material_asset_image_equal(LDKAssetImage a, LDKAssetImage b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

bool ldk_material_type_is_valid(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
         type == LDK_MATERIAL_TYPE_TEXTURED ||
         type == LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT ||
         type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
}

bool ldk_material_desc_defaults(LDKMaterialType type, LDKMaterialDesc *out_desc)
{
  if (out_desc == NULL)
  {
    return false;
  }

  memset(out_desc, 0, sizeof(*out_desc));

  if (!ldk_material_type_is_valid(type))
  {
    return false;
  }

  out_desc->type = type;

  if (type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    out_desc->args.textured.texture.h = x_handle_null();
    out_desc->args.textured.color = 0xffffffffu;
  }
  else
  {
    out_desc->args.vertex_color.color = 0xffffffffu;
  }

  return true;
}

bool ldk_material_desc_is_valid(LDKMaterialDesc const *desc)
{
  return desc != NULL && ldk_material_type_is_valid(desc->type);
}

bool ldk_material_desc_equal(LDKMaterialDesc const *a, LDKMaterialDesc const *b)
{
  if (!ldk_material_desc_is_valid(a) || !ldk_material_desc_is_valid(b) ||
      a->type != b->type)
  {
    return false;
  }

  if (a->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      a->type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    return s_material_asset_image_equal(
               a->args.textured.texture, b->args.textured.texture) &&
           a->args.textured.color == b->args.textured.color;
  }

  return a->args.vertex_color.color == b->args.vertex_color.color;
}

u64 ldk_material_desc_hash(LDKMaterialDesc const *desc)
{
  if (!ldk_material_desc_is_valid(desc))
  {
    return 0;
  }

  u64 hash = LDK_MATERIAL_HASH_OFFSET;
  hash = s_material_hash_u32(hash, (u32)desc->type);

  if (desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      desc->type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    hash = s_material_hash_u32(hash, desc->args.textured.texture.h.index);
    hash = s_material_hash_u32(hash, desc->args.textured.texture.h.version);
    hash = s_material_hash_u32(hash, desc->args.textured.color);
  }
  else
  {
    hash = s_material_hash_u32(hash, desc->args.vertex_color.color);
  }

  return hash;
}
