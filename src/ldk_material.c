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

static bool s_material_type_is_textured(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
         type == LDK_MATERIAL_TYPE_TEXTURED;
}

static bool s_material_type_is_lit(LDKMaterialType type)
{
  return type == LDK_MATERIAL_TYPE_TEXTURED ||
         type == LDK_MATERIAL_TYPE_VERTEX_COLOR;
}

static bool s_material_alpha_mode_is_valid(LDKMaterialAlphaMode mode)
{
  return mode == LDK_MATERIAL_ALPHA_MODE_OPAQUE ||
         mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT ||
         mode == LDK_MATERIAL_ALPHA_MODE_BLEND;
}

static bool s_material_texture_sampling_is_valid(
    const LDKMaterialTexturedArgs *textured)
{
  if (textured == NULL ||
      (textured->filter != LDK_MATERIAL_TEXTURE_FILTER_NEAREST &&
          textured->filter != LDK_MATERIAL_TEXTURE_FILTER_LINEAR) ||
      (textured->mip_filter != LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE &&
          textured->mip_filter != LDK_MATERIAL_TEXTURE_MIP_FILTER_NEAREST &&
          textured->mip_filter != LDK_MATERIAL_TEXTURE_MIP_FILTER_LINEAR) ||
      (textured->wrap_u != LDK_MATERIAL_TEXTURE_WRAP_REPEAT &&
          textured->wrap_u != LDK_MATERIAL_TEXTURE_WRAP_CLAMP &&
          textured->wrap_u != LDK_MATERIAL_TEXTURE_WRAP_MIRROR) ||
      (textured->wrap_v != LDK_MATERIAL_TEXTURE_WRAP_REPEAT &&
          textured->wrap_v != LDK_MATERIAL_TEXTURE_WRAP_CLAMP &&
          textured->wrap_v != LDK_MATERIAL_TEXTURE_WRAP_MIRROR) ||
      !isfinite(textured->uv_scale.x) || !isfinite(textured->uv_scale.y) ||
      !isfinite(textured->uv_offset.x) || !isfinite(textured->uv_offset.y))
  {
    return false;
  }

  return true;
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
  out_desc->alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  out_desc->alpha_cutoff = 0.5f;
  out_desc->surface.normal_map.h = x_handle_null();
  out_desc->surface.specular_map.h = x_handle_null();
  for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
  {
    out_desc->additional_textures[i].h = x_handle_null();
  }

  if (type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    out_desc->args.textured.texture.h = x_handle_null();
    out_desc->args.textured.filter = LDK_MATERIAL_TEXTURE_FILTER_NEAREST;
    out_desc->args.textured.mip_filter = LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE;
    out_desc->args.textured.wrap_u = LDK_MATERIAL_TEXTURE_WRAP_CLAMP;
    out_desc->args.textured.wrap_v = LDK_MATERIAL_TEXTURE_WRAP_CLAMP;
    out_desc->args.textured.uv_scale = (Vec2){1.0f, 1.0f};
    out_desc->args.textured.uv_offset = (Vec2){0.0f, 0.0f};
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

  if (!s_material_alpha_mode_is_valid(desc->alpha_mode) ||
      (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT &&
          !s_material_type_is_textured(desc->type)) ||
      (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT &&
          (!isfinite(desc->alpha_cutoff) || desc->alpha_cutoff < 0.0f ||
              desc->alpha_cutoff > 1.0f)))
  {
    return false;
  }

  if (s_material_type_is_textured(desc->type) &&
      !s_material_texture_sampling_is_valid(&desc->args.textured))
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

LDKAssetImage ldk_material_texture_slot_get(
    LDKMaterialDesc const *desc, u32 slot)
{
  LDKAssetImage null_image = {0};
  null_image.h = x_handle_null();

  if (desc == NULL || slot >= LDK_MATERIAL_TEXTURE_SLOT_COUNT)
  {
    return null_image;
  }

  switch (slot)
  {
  case LDK_MATERIAL_TEXTURE_SLOT_ALBEDO:
    return desc->args.textured.texture;
  case LDK_MATERIAL_TEXTURE_SLOT_NORMAL:
    return desc->surface.normal_map;
  case LDK_MATERIAL_TEXTURE_SLOT_SPECULAR:
    return desc->surface.specular_map;
  default:
    return desc->additional_textures[slot - 3u];
  }
}

bool ldk_material_texture_slot_set(
    LDKMaterialDesc *desc, u32 slot, LDKAssetImage image)
{
  if (desc == NULL || slot >= LDK_MATERIAL_TEXTURE_SLOT_COUNT)
  {
    return false;
  }

  switch (slot)
  {
  case LDK_MATERIAL_TEXTURE_SLOT_ALBEDO:
    desc->args.textured.texture = image;
    break;
  case LDK_MATERIAL_TEXTURE_SLOT_NORMAL:
    desc->surface.normal_map = image;
    break;
  case LDK_MATERIAL_TEXTURE_SLOT_SPECULAR:
    desc->surface.specular_map = image;
    break;
  default:
    desc->additional_textures[slot - 3u] = image;
    break;
  }

  return true;
}

bool ldk_material_desc_equal(LDKMaterialDesc const *a, LDKMaterialDesc const *b)
{
  if (!ldk_material_desc_is_valid(a) || !ldk_material_desc_is_valid(b) ||
      a->type != b->type)
  {
    return false;
  }

  bool equal = a->alpha_mode == b->alpha_mode &&
      (a->alpha_mode != LDK_MATERIAL_ALPHA_MODE_CUTOUT ||
          a->alpha_cutoff == b->alpha_cutoff);
  if (!equal)
  {
    return false;
  }

  if (a->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      a->type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    equal = s_material_asset_image_equal(
                a->args.textured.texture, b->args.textured.texture) &&
            a->args.textured.filter == b->args.textured.filter &&
            a->args.textured.mip_filter == b->args.textured.mip_filter &&
            a->args.textured.wrap_u == b->args.textured.wrap_u &&
            a->args.textured.wrap_v == b->args.textured.wrap_v &&
            a->args.textured.uv_scale.x == b->args.textured.uv_scale.x &&
            a->args.textured.uv_scale.y == b->args.textured.uv_scale.y &&
            a->args.textured.uv_offset.x == b->args.textured.uv_offset.x &&
            a->args.textured.uv_offset.y == b->args.textured.uv_offset.y &&
            a->args.textured.color == b->args.textured.color;
  }
  else
  {
    equal = a->args.vertex_color.color == b->args.vertex_color.color;
  }

  if (equal && s_material_type_is_textured(a->type))
  {
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      if (!s_material_asset_image_equal(
              a->additional_textures[i], b->additional_textures[i]))
      {
        equal = false;
        break;
      }
    }
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
  hash = s_material_hash_u32(hash, (u32)desc->alpha_mode);
  if (desc->alpha_mode == LDK_MATERIAL_ALPHA_MODE_CUTOUT)
  {
    hash = s_material_hash_float(hash, desc->alpha_cutoff);
  }

  if (desc->type == LDK_MATERIAL_TYPE_TEXTURED_UNLIT ||
      desc->type == LDK_MATERIAL_TYPE_TEXTURED)
  {
    hash = s_material_hash_u32(hash, desc->args.textured.texture.h.index);
    hash = s_material_hash_u32(hash, desc->args.textured.texture.h.version);
    hash = s_material_hash_u32(hash, (u32)desc->args.textured.filter);
    hash = s_material_hash_u32(hash, (u32)desc->args.textured.mip_filter);
    hash = s_material_hash_u32(hash, (u32)desc->args.textured.wrap_u);
    hash = s_material_hash_u32(hash, (u32)desc->args.textured.wrap_v);
    hash = s_material_hash_float(hash, desc->args.textured.uv_scale.x);
    hash = s_material_hash_float(hash, desc->args.textured.uv_scale.y);
    hash = s_material_hash_float(hash, desc->args.textured.uv_offset.x);
    hash = s_material_hash_float(hash, desc->args.textured.uv_offset.y);
    hash = s_material_hash_u32(hash, desc->args.textured.color);
    for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
    {
      hash = s_material_hash_u32(
          hash, desc->additional_textures[i].h.index);
      hash = s_material_hash_u32(
          hash, desc->additional_textures[i].h.version);
    }
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
