#include <ldk_material.h>

#include <math.h>
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

static u64 s_material_hash_float(u64 hash, float value)
{
  u32 bits = 0;
  if (value == 0.0f)
  {
    value = 0.0f;
  }
  memcpy(&bits, &value, sizeof(bits));
  return s_material_hash_u32(hash, bits);
}

static bool s_material_asset_image_equal(LDKAssetImage a, LDKAssetImage b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static bool s_material_type_is_lit(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED ||
         type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
}

static float s_material_shininess(float shininess)
{
  return shininess == 0.0f ? 32.0f : shininess;
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
  out_desc->surface.specular = 0.0f;
  out_desc->surface.shininess = 32.0f;
  out_desc->surface.emission = 0.0f;
  out_desc->surface.normal_map.h = x_handle_null();
  out_desc->surface.specular_map.h = x_handle_null();

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
  if (desc == NULL || !ldk_material_type_is_valid(desc->type))
  {
    return false;
  }

  if (!s_material_type_is_lit(desc->type))
  {
    return true;
  }

  return isfinite(desc->surface.specular) && desc->surface.specular >= 0.0f &&
         isfinite(desc->surface.shininess) && desc->surface.shininess >= 0.0f &&
         isfinite(desc->surface.emission) && desc->surface.emission >= 0.0f;
}

bool ldk_material_desc_equal(LDKMaterialDesc const *a, LDKMaterialDesc const *b)
{
  if (!ldk_material_desc_is_valid(a) || !ldk_material_desc_is_valid(b) ||
      a->type != b->type)
  {
    return false;
  }

  bool equal;
  if (a->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      a->type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    equal = s_material_asset_image_equal(
                a->args.textured.texture, b->args.textured.texture) &&
            a->args.textured.color == b->args.textured.color;
  }
  else
  {
    equal = a->args.vertex_color.color == b->args.vertex_color.color;
  }

  if (!equal || !s_material_type_is_lit(a->type))
  {
    return equal;
  }

  return a->surface.specular == b->surface.specular &&
         s_material_shininess(a->surface.shininess) ==
             s_material_shininess(b->surface.shininess) &&
         a->surface.emission == b->surface.emission &&
         s_material_asset_image_equal(
             a->surface.normal_map, b->surface.normal_map) &&
         s_material_asset_image_equal(
             a->surface.specular_map, b->surface.specular_map);
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

  if (s_material_type_is_lit(desc->type))
  {
    hash = s_material_hash_float(hash, desc->surface.specular);
    hash = s_material_hash_float(
        hash, s_material_shininess(desc->surface.shininess));
    hash = s_material_hash_float(hash, desc->surface.emission);
    hash = s_material_hash_u32(hash, desc->surface.normal_map.h.index);
    hash = s_material_hash_u32(hash, desc->surface.normal_map.h.version);
    hash = s_material_hash_u32(hash, desc->surface.specular_map.h.index);
    hash = s_material_hash_u32(hash, desc->surface.specular_map.h.version);
  }

  return hash;
}
