#ifndef LDK_MATERIAL_H
#define LDK_MATERIAL_H

#include <ldk_asset.h>
#include <ldk_common.h>

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

  typedef struct LDKMaterialTexturedArgs
  {
    LDKAssetImage texture;
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
   * Every supported material type defaults to opaque white. Textured materials
   * also default to a null image asset, which can be populated by the caller.
   * Lit materials default to no specular contribution, shininess 32, no
   * emission, and no normal/specular maps. On failure, out_desc is reset to
   * an invalid zero descriptor.
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
   * validated later by the asset/material resolver. Active lit surface values
   * must be finite and non-negative. A zero shininess is canonicalized
   * to the default value (32) so zero-initialized descriptors remain valid.
   *
   * @param desc Descriptor to inspect.
   * @return true when the descriptor is structurally valid.
   */
  LDK_API bool ldk_material_desc_is_valid(LDKMaterialDesc const *desc);

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
