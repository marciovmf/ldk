/**
 * # Terrain data
 *
 * Heightfield metadata, surface definitions and vegetation distribution layers
 * belong to LDKTerrainComponent and are serialized with the scene.
 * Geometry, grass instances and GPU resources belong to the systems.
 * An absent heightmap represents a flat grid. A PNG is only an optional import
 * or export format; height samples are embedded in scene TML as R16 Base64.
 */

#ifndef LDK_TERRAIN_H
#define LDK_TERRAIN_H

#include <ldk_asset.h>
#include <ldk_image.h>
#include <system/ldk_grass_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_tml.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.terrain */
#define LDK_TERRAIN_SURFACE_COUNT 8u
/** @group ldk.terrain */
#define LDK_TERRAIN_VEGETATION_LAYER_COUNT 16u
/** @group ldk.terrain */
#define LDK_TERRAIN_LAYER_NAME_CAPACITY 64u

/** Four channels supported by the existing LDKTerrainSystem. */
/** @group ldk.terrain */
//@enum
typedef enum LDKTerrainChannel
{
  LDK_TERRAIN_CHANNEL_R = 0,
  LDK_TERRAIN_CHANNEL_G,
  LDK_TERRAIN_CHANNEL_B,
  LDK_TERRAIN_CHANNEL_A
} LDKTerrainChannel;

/** Existing terrain appearance rules plus optional manual paint mask.
 * A mask is a grayscale image in terrain UV space; an empty path means there
 * are no manual overrides for this surface. Mask blending is implemented by
 * the future TerrainSystem integration, not by the asset loader.
 * @group ldk.terrain
 */
typedef struct LDKTerrainSurface
{
  char name[LDK_TERRAIN_LAYER_NAME_CAPACITY];
  float min_height;
  float blend_width;
  rgba32 color;
  rgba32 noise_low_color;
  rgba32 noise_high_color;
  u64 sprite_id;
  Vec2 uv_scale;
  float texture_weight;
  bool use_slope;
  float min_slope_degrees;
  float slope_blend_degrees;
  LDKAssetPath paint_mask_path;
} LDKTerrainSurface;

/** One vegetation layer contains its full type description and distribution.
 * No registry, name lookup, or type IDs are involved. Density samples are
 * owned by LDKTerrainComponent; multiple layers overlap independently.
 * @group ldk.terrain
 */
typedef struct LDKTerrainVegetationLayer
{
  u64 id;
  LDKGrassTypeDesc type;
  float density_scale;
  /* Legacy scene field, retained for backward compatibility. Terrain-owned
   * distribution uses GrassSystem.seed, stable layer IDs and patch coordinates. */
  u32 seed;
} LDKTerrainVegetationLayer;

/** Portable terrain metadata. Run-time rendering settings such as chunk size,
 * culling and shadow submission deliberately stay in LDKTerrainSystem.
 * @group ldk.terrain
 */
typedef struct LDKTerrain
{
  LDKAssetPath heightmap_path;
  LDKTerrainChannel height_channel;
  /** Heightmap dimensions, or the default flat grid dimensions if absent. */
  u32 width;
  u32 height;
  float cell_size;
  float height_scale;
  float height_offset;
  bool center_on_origin;
  float origin_x;
  float origin_z;

  LDKAssetPath material_path;
  LDKAssetPath texture_atlas_path;
  LDKAssetPath color_noise_path;
  float color_noise_scale;
  float color_noise_strength;

  u32 surface_count;
  LDKTerrainSurface surfaces[LDK_TERRAIN_SURFACE_COUNT];

  u32 vegetation_layer_count;
  LDKTerrainVegetationLayer
      vegetation_layers[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  u64 next_vegetation_layer_id;
} LDKTerrain;

/** Initialize a flat, editable default terrain with one white surface. */
LDK_API void ldk_terrain_init(LDKTerrain *terrain);
/** Validate dimensions, terrain parameters, identities and asset paths. */
LDK_API bool ldk_terrain_validate(const LDKTerrain *terrain);
/** Copy validated metadata. Safe when destination and source are the same. */
LDK_API bool ldk_terrain_copy(LDKTerrain *dst, const LDKTerrain *src);

/** Sample the same heightfield triangles used by TerrainSystem at world X/Z.
 * heightmap_info may be NULL for a flat default terrain, otherwise it must
 * describe the terrain's RGBA8/RGBA16 heightmap with matching dimensions.
 * Either output pointer may be NULL, but not both. No engine module required.
 */
LDK_API bool ldk_terrain_surface_sample(const LDKTerrain *terrain,
    const LDKImageInfo *heightmap_info, float x, float z,
    float *out_height, Vec3 *out_normal);

/** Initialize a terrain from an already loaded image (RGBA8/RGBA16).
 * The image is borrowed, not retained. A nonempty logical path may name
 * an external image in the AssetSource; NULL/empty selects embedded data,
 * supplied separately when serializing the scene.
 * Previous terrain data is replaced only on success.
 * The caller can adjust cell/height scale before persisting the component.
 */
LDK_API bool ldk_terrain_import_heightmap(LDKTerrain *terrain,
    const char *image_path, const LDKImage *image,
    LDKTerrainChannel channel);

/** Downsample/resize a source RGBA8/RGBA16 heightmap to an explicit terrain
 * grid resolution. An area-weighted filter preserves average heights when
 * shrinking; RGBA16 output retains precision for subsequent sculpting.
 * The caller owns the returned image and must destroy it with
 * ldk_image_destroy(). The source is never modified.
 * @group ldk.terrain
 */
LDK_API LDKImage *ldk_terrain_heightmap_resample(
    const LDKImage *source, u32 target_width, u32 target_height);

/** Expand the world-space height interval of an editable RGBA16 heightmap.
 * Keeps every existing sample at its current world height (within 16-bit
 * quantization) and preserves elevation-based surface thresholds. The other
 * three RGBA channels are untouched. Growth is geometric, not one increment
 * per brush frame. An interval already containing the requested heights is
 * a no-op. Both metadata and pixels remain unchanged on failure.
 * The caller owns the pixels and may persist them inside the scene.
 * @group ldk.terrain
 */
LDK_API bool ldk_terrain_heightmap_range_expand(LDKTerrain *terrain,
    u16 *rgba16_pixels, float required_min, float required_max);

/** Add a vegetation distribution layer; an empty mask starts unpainted.
 * The returned ID is stable; IDs are never reused within the terrain.
 */
LDK_API bool ldk_terrain_vegetation_layer_add(LDKTerrain *terrain,
    const LDKGrassTypeDesc *type, float density_scale,
    u32 seed, u64 *out_id);
LDK_API bool ldk_terrain_vegetation_layer_remove(
    LDKTerrain *terrain, u64 layer_id);

/** Serialize/parse terrain metadata in TML form. The returned text must
 * be freed. Prefer _with_heightmap when persisting editable height data. */
LDK_API bool ldk_terrain_to_tml(const LDKTerrain *terrain, char **out_text);
LDK_API bool ldk_terrain_from_tml(LDKTerrain *terrain, const char *text);

/** Include the normalized height samples inside the TML document as portable
 * little-endian u16 data encoded with base64. Only the configured height
 * channel is stored; no rendered meshes or redundant RGBA channels are saved.
 * A NULL heightmap produces a metadata-only/flat terrain. out_image is owned
 * by the caller and must be destroyed with ldk_image_destroy().
 * On failure, outputs and existing terrain metadata remain unchanged.
 */
LDK_API bool ldk_terrain_to_tml_with_heightmap(const LDKTerrain *terrain,
    const LDKImageInfo *heightmap, char **out_text);
LDK_API bool ldk_terrain_from_tml_with_heightmap(LDKTerrain *terrain,
    LDKImage **out_image, const char *text);
/** Read the same terrain TML representation from a node embedded in a scene.
 * This avoids copying or truncating the base64 samples through XSmallstr.
 * The caller retains the TML document and owns the decoded image.
 */
LDK_API bool ldk_terrain_from_tml_node_with_heightmap(LDKTerrain *terrain,
    LDKImage **out_image, const TMLDocument *doc, const TMLNode *node);
/** Scene-owned R8 vegetation masks use one Base64 string per layer. An
 * absent mask is NULL. Each present mask has width * height bytes. The
 * decoder allocates every output; the caller must free each non-NULL mask. */
LDK_API bool ldk_terrain_to_tml_with_component_data(
    const LDKTerrain *terrain, const LDKImageInfo *heightmap,
    const u8 *const masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    char **out_text);
/* Extended scene format: R8 painting weights attached to existing Surfaces,
 * stored as eight interleaved channels. NULL/absent means procedural only. */
LDK_API bool ldk_terrain_to_tml_with_surface_paint(
    const LDKTerrain *terrain, const LDKImageInfo *heightmap,
    const u8 *const masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    const u8 *surface_paint, char **out_text);
LDK_API bool ldk_terrain_from_tml_node_with_surface_paint(
    LDKTerrain *terrain, LDKImage **out_image,
    u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT], u8 **surface_paint,
    const TMLDocument *doc, const TMLNode *node);
LDK_API bool ldk_terrain_from_tml_node_with_component_data(
    LDKTerrain *terrain, LDKImage **out_image,
    u8 *masks[LDK_TERRAIN_VEGETATION_LAYER_COUNT],
    const TMLDocument *doc, const TMLNode *node);

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_H
