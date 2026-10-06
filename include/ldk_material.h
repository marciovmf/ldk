#ifndef LDK_MATERIAL_H
#define LDK_MATERIAL_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum LDKMaterialType
  {
    LDK_MATERIAL_TYPE_INVALID = 0,
    LDK_MATERIAL_TYPE_TEXTURED_UNLIT = 1,
    LDK_MATERIAL_TYPE_TEXTURED = 2,
    LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT = 3,
    LDK_MATERIAL_TYPE_VERTEX_COLOR = 4
  } LDKMaterialType;

  typedef enum LDKMaterialAlphaMode
  {
    LDK_MATERIAL_ALPHA_MODE_OPAQUE = 0,
    LDK_MATERIAL_ALPHA_MODE_CUTOUT = 1,
    LDK_MATERIAL_ALPHA_MODE_BLEND = 2
  } LDKMaterialAlphaMode;

  typedef enum LDKMaterialTextureFilter
  {
    LDK_MATERIAL_TEXTURE_FILTER_NEAREST = 0,
    LDK_MATERIAL_TEXTURE_FILTER_LINEAR = 1
  } LDKMaterialTextureFilter;

  typedef enum LDKMaterialTextureWrap
  {
    LDK_MATERIAL_TEXTURE_WRAP_REPEAT = 0,
    LDK_MATERIAL_TEXTURE_WRAP_CLAMP = 1,
    LDK_MATERIAL_TEXTURE_WRAP_MIRROR = 2
  } LDKMaterialTextureWrap;

  typedef enum LDKMaterialTextureMipFilter
  {
    LDK_MATERIAL_TEXTURE_MIP_FILTER_NONE = 0,
    LDK_MATERIAL_TEXTURE_MIP_FILTER_NEAREST = 1,
    LDK_MATERIAL_TEXTURE_MIP_FILTER_LINEAR = 2
  } LDKMaterialTextureMipFilter;

#define LDK_MATERIAL_TEXTURE_SLOT_COUNT 8u
#define LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT 5u

  typedef enum LDKMaterialTextureSlot
  {
    LDK_MATERIAL_TEXTURE_SLOT_ALBEDO = 0,
    LDK_MATERIAL_TEXTURE_SLOT_NORMAL = 1,
    LDK_MATERIAL_TEXTURE_SLOT_SPECULAR = 2,
    LDK_MATERIAL_TEXTURE_SLOT_3 = 3,
    LDK_MATERIAL_TEXTURE_SLOT_4 = 4,
    LDK_MATERIAL_TEXTURE_SLOT_5 = 5,
    LDK_MATERIAL_TEXTURE_SLOT_6 = 6,
    LDK_MATERIAL_TEXTURE_SLOT_7 = 7
  } LDKMaterialTextureSlot;

  typedef struct LDKMaterialTextureSettings
  {
    LDKMaterialTextureFilter filter;
    LDKMaterialTextureMipFilter mip_filter;
    LDKMaterialTextureWrap wrap_u;
    LDKMaterialTextureWrap wrap_v;
    Vec2 uv_scale;
    Vec2 uv_offset;
    /* Otherwise inherit the legacy material-wide settings. */
    bool independent;
  } LDKMaterialTextureSettings;

  typedef struct LDKMaterialTexturedArgs
  {
    LDKAssetImage texture;
    /* Legacy defaults for slots without independent settings; the
     * image assets remain shared. */
    LDKMaterialTextureFilter filter;
    LDKMaterialTextureMipFilter mip_filter;
    LDKMaterialTextureWrap wrap_u;
    LDKMaterialTextureWrap wrap_v;
    /* Applied to mesh UVs before sampling. */
    Vec2 uv_scale;
    Vec2 uv_offset;
    rgba32 color;
  } LDKMaterialTexturedArgs;

  typedef struct LDKMaterialVertexColorArgs
  {
    rgba32 color;
  } LDKMaterialVertexColorArgs;

  typedef union LDKMaterialArgs
  {
    LDKMaterialTexturedArgs textured;
    LDKMaterialVertexColorArgs vertex_color;
  } LDKMaterialArgs;

  /* Surface parameters are active for lit material types. */
  typedef struct LDKMaterialSurfaceArgs
  {
    float specular;  /* Phong specular strength. */
    float shininess; /* Phong exponent; zero selects the default (32). */
    float emission;  /* Adds albedo * emission independently of lights. */
    LDKAssetImage normal_map;   /* Optional tangent-space normal map. */
    LDKAssetImage specular_map; /* Optional R-channel specular multiplier. */
  } LDKMaterialSurfaceArgs;

  typedef struct LDKMaterialDesc
  {
    LDKMaterialType type;
    LDKMaterialArgs args;
    LDKMaterialSurfaceArgs surface;
    /* Extra shader-defined Texture2D slots. Slots 0..2 are represented by
     * the existing albedo/normal/specular fields for compatibility. */
    LDKAssetImage additional_textures[LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT];
    /* Includes added slots that do not yet have an image. */
    u32 texture_slot_mask;
    LDKMaterialTextureSettings texture_settings[LDK_MATERIAL_TEXTURE_SLOT_COUNT];
    /* Alpha mode is a render property shared by all material types.
     * CUTOUT currently requires a textured material; BLEND is supported by
     * both textured and vertex-color materials. */
    LDKMaterialAlphaMode alpha_mode;
    float alpha_cutoff;
  } LDKMaterialDesc;

  /**
   * @brief Check whether a value identifies a supported material type.
   * @param type Material type to inspect.
   * @return true for a supported material type, false otherwise.
   */
  LDK_API bool ldk_material_type_is_valid(LDKMaterialType type);

  /**
   * @brief Initialize a material descriptor with deterministic defaults.
   *
   * Every supported material type defaults to opaque white, opaque alpha mode,
   * and a 0.5 cutout threshold. Textured materials also default to a null image
   * asset, nearest filtering, no mip filtering, clamp-to-edge wrapping, and
   * identity UV transform. Additional texture slots default to null. Lit
   * materials default to no specular contribution, shininess 32, no emission,
   * and no normal/specular maps. On failure, out_desc is
   * reset to an invalid zero descriptor.
   *
   * @param type Material type whose defaults should be produced.
   * @param out_desc Destination descriptor.
   * @return true on success, false for an invalid type or null destination.
   */
  LDK_API bool ldk_material_desc_defaults(
      LDKMaterialType type, LDKMaterialDesc *out_desc);

  /**
   * @brief Check whether a descriptor has a supported material type.
   *
   * This validates the descriptor structure only. Asset availability is
   * validated later by the asset/material resolver. Textured cutout thresholds
   * must be finite and within [0, 1]. Active lit surface values must be finite
   * and non-negative. A zero shininess is canonicalized to the default value
   * (32) so zero-initialized descriptors remain valid.
   *
   * @param desc Descriptor to inspect.
   * @return true when the descriptor is structurally valid.
   */
  LDK_API bool ldk_material_desc_is_valid(LDKMaterialDesc const *desc);

  /** Return the image assigned to a material texture slot. */
  LDK_API LDKAssetImage ldk_material_texture_slot_get(
      LDKMaterialDesc const *desc, u32 slot);

  /** Assign an image to a material texture slot. */
  LDK_API bool ldk_material_texture_slot_set(
      LDKMaterialDesc *desc, u32 slot, LDKAssetImage image);

  /** Return independent settings, or the legacy material defaults. */
  LDK_API LDKMaterialTextureSettings ldk_material_texture_settings_get(
      LDKMaterialDesc const *desc, u32 slot);

  /**
   * @brief Compare the active values of two material descriptors exactly.
   * @param a First descriptor.
   * @param b Second descriptor.
   * @return true when both descriptors represent the same authored material.
   */
  LDK_API bool ldk_material_desc_equal(
      LDKMaterialDesc const *a, LDKMaterialDesc const *b);

  /**
   * @brief Hash the active values of a material descriptor.
   * @param desc Descriptor to hash.
   * @return Deterministic hash, or zero for a null or invalid descriptor.
   */
  LDK_API u64 ldk_material_desc_hash(LDKMaterialDesc const *desc);

#ifdef __cplusplus
}
#endif

#endif // LDK_MATERIAL_H
