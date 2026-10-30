#ifndef DEMO_ISLAND_TERRAIN_H
#define DEMO_ISLAND_TERRAIN_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <ldk_resource.h>
#include <module/ldk_system.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

int island_terrain_system_initialize(void *data);
void island_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void island_terrain_system_terminate(void *data);

bool island_terrain_grass_state_at(Vec3 world_position,
    float *out_density, float *out_min_height);
bool island_terrain_grass_density_multiply_at(
    Vec3 world_position, float multiplier);

/** Game-specific, non-rendering interpretation of the generated island tile. */
//@enum
typedef enum TFTFIslandTileKind
{
  TFTF_ISLAND_TILE_DEEP_WATER = 0,
  TFTF_ISLAND_TILE_SHALLOW_WATER,
  TFTF_ISLAND_TILE_SHORE,
  TFTF_ISLAND_TILE_DRY,
  TFTF_ISLAND_TILE_GRASS,
  TFTF_ISLAND_TILE_FOREST,
  TFTF_ISLAND_TILE_THORNS,
  TFTF_ISLAND_TILE_MOUNTAIN,
  TFTF_ISLAND_TILE_COUNT
} TFTFIslandTileKind;

bool island_terrain_world_to_cell(
    Vec3 world_position, i32 *out_x, i32 *out_y);
bool island_terrain_cell_world_bounds(
    i32 cell_x, i32 cell_y, Vec2 *out_min, Vec2 *out_max);
bool tftf_island_terrain_tile_kind_at_cell(
    i32 cell_x, i32 cell_y, TFTFIslandTileKind *out_kind);
bool tftf_island_terrain_tile_kind_at(
    Vec3 world_position, TFTFIslandTileKind *out_kind);

typedef enum TFTFIslandMapChannel
{
  TFTF_ISLAND_MAP_CHANNEL_HEIGHT = 0,
  TFTF_ISLAND_MAP_CHANNEL_DECORATION,
  TFTF_ISLAND_MAP_CHANNEL_RESOURCE,
  TFTF_ISLAND_MAP_CHANNEL_RESERVED,
  TFTF_ISLAND_MAP_CHANNEL_COUNT
} TFTFIslandMapChannel;

u8 island_terrain_map_decoration_mask_get(u16 packed);
float island_terrain_map_density_get(u16 packed);

/**
 * Read-only view of the generated RGBA16 island map.
 * Channels:
 * R = height.
 * G = decoration mask in the high byte, shared density in the low byte.
 * B = resource type in the high byte, density in the low byte.
 * A = reserved.
 */
typedef struct IslandMapDataView
{
  const u16 *texels;
  u32 width;
  u32 height;
  u32 revision;
  u32 seed;
} IslandMapDataView;

bool island_terrain_map_data_get(IslandMapDataView *out_view);

/* Compatibility tags for existing TFTF gameplay. These identifiers do not
 * influence island generation or biome classification. New gameplay code can
 * query a biome index and its configured descriptor instead. */
/** A biome is selected by numeric intervals on four spatial fields.
 * Rules are tested in array order; first match wins. An uncovered cell is
 * a configuration error, not a request for a default biome.
 * Land is the radial island mask plus the land fBm contribution, and may be
 * negative outside the island. The other noise fields range from 0 to 1.
 */
typedef struct IslandBiomeDesc
{
  //@inspect headline
  XSmallstr name;
  /** Land field, which includes island shape; may be below zero. */
  float land_min;
  float land_max;
  //@inspect slider min=0 max=1
  float terrain_min;
  //@inspect slider min=0 max=1
  float terrain_max;
  //@inspect slider min=0 max=1
  float moisture_min;
  //@inspect slider min=0 max=1
  float moisture_max;
  //@inspect slider min=0 max=1
  float distribution_min;
  //@inspect slider min=0 max=1
  float distribution_max;

  //@inspect slider min=0 max=1
  float min_height;
  //@inspect slider min=0 max=1
  float max_height;
  /** 0 = terrain fBm relief; 1 = land-field progression. */
  //@inspect slider min=0 max=1
  float land_height_weight;
  /** Contribution to global height smoothing (0 disables smoothing). */
  //@inspect slider min=0 max=1
  float height_smoothing_weight;
  /** Contribution as a neighbor sample during height smoothing. */
  //@inspect slider min=0 max=1
  float height_sample_weight;
  XSmallstr grass_type_name;
  bool allow_inland_pools;

  /** Opaque gameplay identifier; 255 means no legacy TFTF tile kind.
   * This never influences biome generation. */
  //@inspect min=0 max=255
  u32 gameplay_tag;
  /** Optional legacy atlas layer: 0 ocean, 1 shallow, 2 sand,
   * 3 dark grass, 4 grass, 5 rock. This does not affect the heightmap or
   * the LDKTerrainSystem renderer. */
  //@inspect min=0 max=5
  u32 legacy_atlas_layer;

  //@inspect slider min=0 max=1
  float resource_chance;
  //@inspect min=0 max=255
  u32 resource_type;
  bool resource_center_bias;
  bool inherit_decoration_chance;

  //@inspect slider min=0 max=1
  float decoration_0_chance;
  //@inspect slider min=0 max=1
  float decoration_1_chance;
  //@inspect slider min=0 max=1
  float decoration_2_chance;
  //@inspect slider min=0 max=1
  float decoration_3_chance;
  //@inspect slider min=0 max=1
  float decoration_4_chance;
  //@inspect slider min=0 max=1
  float decoration_5_chance;
  //@inspect slider min=0 max=1
  float decoration_6_chance;
  //@inspect slider min=0 max=1
  float decoration_7_chance;
} IslandBiomeDesc;

X_ARRAY_TYPE(IslandBiomeDesc)

/* Source-level compatibility for existing games using the old descriptor. */
typedef IslandBiomeDesc TFTFIslandBiomeDesc;
X_ARRAY_TYPE(TFTFIslandBiomeDesc)

/** Defaults used when a new biome entry is added in the inspector. */
void island_terrain_biome_initialize(void *element);

/** Retrieve the active biome index; indices refer to IslandTerrain.biomes. */
bool island_terrain_biome_index_at_cell(
    i32 cell_x, i32 cell_y, u32 *out_index);
bool island_terrain_biome_index_at(
    Vec3 world_position, u32 *out_index);

//@system components=LDK_COMPONENT_TYPE_CAMERA initialize=island_terrain_system_initialize update=island_terrain_system_update terminate=island_terrain_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED
typedef struct IslandTerrain
{
  //@begin_group "Island Generation"
  u32 seed;
  u32 map_size;
  float land_scale;
  float island_radius;
  float terrain_scale;

  /*
   * Small-scale height variation within biomes that use terrain relief.
   */
  float surface_noise_scale;
  //@inspect min=1 max=8
  u32 surface_noise_octaves;
  //@inspect slider min=0 max=0.5
  float surface_noise_strength;

  float moisture_scale;
  /** Independent distribution noise, applicable to every biome. */
  float distribution_noise_scale;
  u32 land_octaves;
  u32 terrain_octaves;
  u32 moisture_octaves;
  float land_noise_strength;
  /** Smooth abrupt height discontinuities where the per-biome settings permit it. */
  //@inspect slider min=0 max=1
  float height_smoothing_strength;
  /** Width, in heightmap cells, of the softened band around sharp height transitions. */
  //@inspect min=0 max=24
  u32 height_smoothing_radius;
  /**
   * Minimum normalized neighbor-height difference considered a hard edge.
   * This catches discontinuities without blurring normal surface noise
   * inside a biome.
   */
  //@inspect slider min=0 max=0.1
  float height_smoothing_edge_threshold;
  //@inspect min=0 max=8
  u32 height_smoothing_passes;
  //@end_group

  //@begin_group "Biomes"
  /** Ordered selection rules. At least one is required; none is added at Play. */
  //@inspect element_type=IslandBiomeDesc initializer=island_terrain_biome_initialize max_count=255
  XArray_IslandBiomeDesc *biomes;
  //@end_group

  //@begin_group "Inland Pools"
  /** Candidate spacing in heightmap cells. Zero disables inland pools. */
  //@inspect min=0
  float pool_spacing;
  //@inspect slider min=0 max=1
  float pool_chance;
  //@inspect min=1
  float pool_min_radius;
  //@inspect min=1
  float pool_max_radius;
  //@inspect min=1
  float pool_bank_width;
  /** Pool centres must have this minimum original Land field value. */
  float pool_min_land;
  /** The target Land field inside the basin (for biome classification). */
  float pool_land_value;
  /** Normalized target elevation at the centre of the basin. */
  //@inspect slider min=0 max=1
  float pool_height;
  //@end_group

  //@begin_group "Island Rendering"
  u32 radius;
  float cell_size;
  float elevation;
  float forward_offset;
  LDKAssetMaterial material;
  //@end_group

  //@begin_group "Runtime"
  //@inspect runtime
  i32 center_x;
  //@inspect runtime
  i32 center_y;
  //@inspect runtime
  u32 cached_radius;
  //@inspect runtime
  float cached_cell_size;
  //@inspect runtime
  float cached_elevation;
  //@inspect runtime
  u32 map_revision;
  //@inspect runtime
  u32 active_tile_count;
  //@inspect runtime
  u32 tiles_added;
  //@inspect runtime
  u32 tiles_removed;
  //@inspect runtime
  u32 mesh_update_count;
  //@inspect runtime
  bool mesh_dirty;
  //@inspect runtime
  bool has_geometry;
  //@inspect runtime
  bool map_loaded_from_cache;
  //@inspect runtime
  LDKResourceMesh mesh;
  //@end_group
} IslandTerrain;

#endif // DEMO_ISLAND_TERRAIN_H
