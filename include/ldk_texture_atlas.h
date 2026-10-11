#ifndef LDK_TEXTURE_ATLAS_H
#define LDK_TEXTURE_ATLAS_H

#include <ldk_asset.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY 96u

typedef struct LDKTextureAtlasRegion
{
  u64 id; /* Stable across renaming, reordering and serialization. */
  char name[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
  u32 x, y, width, height; /* Top-left origin, pixels; not normalized UVs. */
  float pivot_x, pivot_y; /* Fractions of the region: (0.5, 0.5) is center. */
} LDKTextureAtlasRegion;

typedef struct LDKTextureAtlas
{
  LDKAssetPath texture_path; /* Logical image asset path, not an OS path. */
  LDKTextureAtlasRegion *regions;
  u32 region_count, region_capacity;
  u64 next_region_id;
} LDKTextureAtlas;

LDK_API void ldk_texture_atlas_init(LDKTextureAtlas *atlas);
LDK_API void ldk_texture_atlas_clear(LDKTextureAtlas *atlas);
LDK_API bool ldk_texture_atlas_copy(LDKTextureAtlas *dst, const LDKTextureAtlas *src);
LDK_API bool ldk_texture_atlas_validate(const LDKTextureAtlas *atlas);
LDK_API bool ldk_texture_atlas_texture_set(LDKTextureAtlas *atlas, const char *path);
LDK_API LDKTextureAtlasRegion *ldk_texture_atlas_region_find(
    LDKTextureAtlas *atlas, u64 id);
LDK_API const LDKTextureAtlasRegion *ldk_texture_atlas_region_find_const(
    const LDKTextureAtlas *atlas, u64 id);
LDK_API bool ldk_texture_atlas_region_add(LDKTextureAtlas *atlas,
    const char *name, u32 x, u32 y, u32 width, u32 height, u64 *out_id);
LDK_API bool ldk_texture_atlas_region_remove(LDKTextureAtlas *atlas, u64 id);
LDK_API bool ldk_texture_atlas_region_rename(LDKTextureAtlas *atlas, u64 id,
    const char *name);
LDK_API bool ldk_texture_atlas_grid_append(LDKTextureAtlas *atlas,
    u32 image_width, u32 image_height, u32 cell_width, u32 cell_height,
    u32 spacing_x, u32 spacing_y, u32 margin_x, u32 margin_y);
/* Writes UVs as left, top, right, bottom in normalized image coordinates. */
LDK_API bool ldk_texture_atlas_region_uv(const LDKTextureAtlasRegion *region,
    u32 image_width, u32 image_height, float out_uv[4]);
/* TML text is allocated with malloc; caller frees it. */
LDK_API bool ldk_texture_atlas_to_tml(const LDKTextureAtlas *atlas, char **out_text);
LDK_API bool ldk_texture_atlas_from_tml(LDKTextureAtlas *atlas, const char *text);
LDK_API bool ldk_texture_atlas_load(LDKTextureAtlas *atlas, const char *filename);
LDK_API bool ldk_texture_atlas_save(const LDKTextureAtlas *atlas, const char *filename);

#ifdef __cplusplus
}
#endif
#endif
