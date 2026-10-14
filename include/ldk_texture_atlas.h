/**
 * # Texture atlases
 *
 * Manage sprite regions and other metadata stored in a texture atlas.
 */

#ifndef LDK_TEXTURE_ATLAS_H
#define LDK_TEXTURE_ATLAS_H

#include <ldk_asset.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.asset.texture_atlas */
#define LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY 96u

/** @group ldk.asset.texture_atlas */
typedef struct LDKTextureAtlasRegion
{
  u64 id; /* Stable across renaming, reordering and serialization. */
  char name[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
  u32 x, y, width, height; /* Top-left origin, pixels; not normalized UVs. */
  float pivot_x, pivot_y; /* Fractions of the region: (0.5, 0.5) is center. */
} LDKTextureAtlasRegion;

/** @group ldk.asset.texture_atlas */
typedef struct LDKTextureAtlas
{
  LDKAssetPath texture_path; /* Logical image asset path, not an OS path. */
  LDKTextureAtlasRegion *regions;
  u32 region_count, region_capacity;
  u64 next_region_id;
} LDKTextureAtlas;

/**
 * @brief Initialize an empty texture atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @group ldk.asset.texture_atlas
 */
LDK_API void ldk_texture_atlas_init(LDKTextureAtlas *atlas);
/**
 * @brief Clear the state of the texture atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @group ldk.asset.texture_atlas
 */
LDK_API void ldk_texture_atlas_clear(LDKTextureAtlas *atlas);
/**
 * @brief Copy the texture atlas.
 * @param dst Destination object receiving copied data.
 * @param src Source object whose data will be copied.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_copy(LDKTextureAtlas *dst, const LDKTextureAtlas *src);
/**
 * @brief Validate the texture atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_validate(const LDKTextureAtlas *atlas);
/**
 * @brief Set the texture atlas texture.
 * @param atlas Texture atlas being inspected or modified.
 * @param path Path identifying the file or resource.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_texture_set(LDKTextureAtlas *atlas, const char *path);
/**
 * @brief Find the texture atlas region.
 * @param atlas Texture atlas being inspected or modified.
 * @param id Identifier of the target object.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group ldk.asset.texture_atlas
 */
LDK_API LDKTextureAtlasRegion *ldk_texture_atlas_region_find(
    LDKTextureAtlas *atlas, u64 id);
/**
 * @brief Find a region by stable ID without exposing mutable access.
 * @param atlas Texture atlas being inspected or modified.
 * @param id Identifier of the target object.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group ldk.asset.texture_atlas
 */
LDK_API const LDKTextureAtlasRegion *ldk_texture_atlas_region_find_const(
    const LDKTextureAtlas *atlas, u64 id);
/**
 * @brief Add the texture atlas region.
 * @param atlas Texture atlas being inspected or modified.
 * @param name Name identifying the object or setting.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @param width Width in pixels or the units expected by the API.
 * @param height Height in pixels or the units expected by the API.
 * @param out_id Receives the resulting ID.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_region_add(LDKTextureAtlas *atlas,
    const char *name, u32 x, u32 y, u32 width, u32 height, u64 *out_id);
/**
 * @brief Remove the texture atlas region.
 * @param atlas Texture atlas being inspected or modified.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_region_remove(LDKTextureAtlas *atlas, u64 id);
/**
 * @brief Rename a region without changing its stable identifier.
 * @param atlas Texture atlas being inspected or modified.
 * @param id Identifier of the target object.
 * @param name Name identifying the object or setting.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_region_rename(LDKTextureAtlas *atlas, u64 id,
    const char *name);
/**
 * @brief Add a grid of regions to the atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @param image_width Image width used by this operation.
 * @param image_height Image height used by this operation.
 * @param cell_width Cell width used by this operation.
 * @param cell_height Cell height used by this operation.
 * @param spacing_x Spacing x used by this operation.
 * @param spacing_y Spacing y used by this operation.
 * @param margin_x Margin x used by this operation.
 * @param margin_y Margin y used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_grid_append(LDKTextureAtlas *atlas,
    u32 image_width, u32 image_height, u32 cell_width, u32 cell_height,
    u32 spacing_x, u32 spacing_y, u32 margin_x, u32 margin_y);
/* Writes UVs as left, top, right, bottom in normalized image coordinates. */
/**
 * @brief Compute normalized UV coordinates for an atlas region.
 *
 * Atlas regions are specified in pixel coordinates; this operation converts them to normalized texture coordinates.
 * @param region Region used by this operation.
 * @param image_width Image width used by this operation.
 * @param image_height Image height used by this operation.
 * @param out_uv Receives the resulting UV.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_region_uv(const LDKTextureAtlasRegion *region,
    u32 image_width, u32 image_height, float out_uv[4]);
/* TML text is allocated with malloc; caller frees it. */
/**
 * @brief Serialize the atlas and its regions to TML.
 * @param atlas Texture atlas being inspected or modified.
 * @param out_text Destination buffer for the resulting text.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_to_tml(const LDKTextureAtlas *atlas, char **out_text);
/**
 * @brief Read an atlas and its regions from TML.
 * @param atlas Texture atlas being inspected or modified.
 * @param text Text used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_from_tml(LDKTextureAtlas *atlas, const char *text);
/**
 * @brief Load the texture atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @param filename Filename used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_load(LDKTextureAtlas *atlas, const char *filename);
/**
 * @brief Save the texture atlas.
 * @param atlas Texture atlas being inspected or modified.
 * @param filename Filename used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.asset.texture_atlas
 */
LDK_API bool ldk_texture_atlas_save(const LDKTextureAtlas *atlas, const char *filename);

#ifdef __cplusplus
}
#endif
#endif
