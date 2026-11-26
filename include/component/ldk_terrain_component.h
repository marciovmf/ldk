/**
 * # Terrain component
 *
 * A terrain is authored and serialized as part of its owning scene entity.
 * The heightfield is stored in the component as an owned RGBA16 image; scene
 * serialization writes only the selected R16 height channel as base64.
 * Rendering geometry and grass instances are owned by their systems.
 * Terrain height queries require an upright transform (yaw is allowed).
 */
#ifndef LDK_TERRAIN_COMPONENT_H
#define LDK_TERRAIN_COMPONENT_H

#include <ldk_common.h>
#include <ldk_terrain.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Rectangle of changed samples in terrain grid coordinates. */
typedef struct LDKTerrainHeightmapRegion
{
  u32 x;
  u32 y;
  u32 width;
  u32 height;
} LDKTerrainHeightmapRegion;

/** Scene-owned R8 density masks in vegetation-layer order. Separate revision
 * and dirty rectangle per layer let GrassSystem update only brushed patches. */
typedef struct LDKTerrainVegetationDensity
{
  u8 *samples[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  u64 revisions[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  LDKTerrainHeightmapRegion dirty_regions[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  u64 dirty_base_revisions[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  bool dirty_valid[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
} LDKTerrainVegetationDensity;

/** @group ldk.component.terrain */
//@component
typedef struct LDKTerrainComponent
{
  //@begin_group "Geometry"
  /** Number of rendered mesh cells per chunk, independent per terrain. */
  //@inspect min=1 max=256
  u32 chunk_size;
  /** Source heightmap cells per mesh vertex (1 = full resolution). */
  //@inspect min=1 max=256
  u32 mesh_sample_step;
  //@end_group

  //@begin_group "Rendering"
  bool cull;
  bool casts_shadows;
  //@end_group

  /* Terrain is edited by the built-in Inspector. These opaque fields have
   * custom scene serialization, not reflection-based scalar serialization. */
  //@inspect hidden runtime
  LDKTerrain terrain;
  //@inspect hidden runtime
  LDKImage *heightmap;
  /* Optional R8 density masks, indexed by terrain vegetation layer.
   * The wrapper and its non-NULL buffers are owned by the component. */
  //@inspect hidden runtime
  LDKTerrainVegetationDensity *vegetation_density;
  /* Eight interleaved R8 weights per heightmap texel. NULL means no manual
   * painting (all weights zero). Sum of weights must never exceed 255. */
  //@inspect hidden runtime
  u8 *surface_paint;
  //@inspect hidden runtime
  u64 surface_paint_revision;
  /* Pending paint rectangle since surface_paint_dirty_base_revision. */
  //@inspect hidden runtime
  LDKTerrainHeightmapRegion surface_paint_dirty_region;
  //@inspect hidden runtime
  u64 surface_paint_dirty_base_revision;
  //@inspect hidden runtime
  bool surface_paint_dirty_valid;
  //@inspect hidden runtime
  u64 revision;
  /* Changes only when the owned heightmap or grid actually changes. */
  //@inspect hidden runtime
  u64 heightmap_revision;
  /* Runtime-only accumulated brush region. The renderer may process it
   * incrementally only when its cached revision matches the base revision. */
  //@inspect hidden runtime
  LDKTerrainHeightmapRegion heightmap_dirty_region;
  //@inspect hidden runtime
  u64 heightmap_dirty_base_revision;
  //@inspect hidden runtime
  bool heightmap_dirty_valid;
  /* GrassSystem consumes an independent rectangle; rendering either system
   * must not erase changes that the other has not processed yet. */
  //@inspect hidden runtime
  LDKTerrainHeightmapRegion heightmap_grass_dirty_region;
  //@inspect hidden runtime
  u64 heightmap_grass_dirty_base_revision;
  //@inspect hidden runtime
  bool heightmap_grass_dirty_valid;
} LDKTerrainComponent;

LDK_API LDKTerrainComponent ldk_terrain_component_make_default(void);
/** Atomically replace terrain data and optional heightmap (deep copy).
 * A NULL heightmap represents a flat grid. The metadata and image dimensions
 * must match. The component retains its prior state on failure.
 */
LDK_API bool ldk_terrain_component_set(LDKTerrainComponent *component,
    const LDKTerrain *terrain, const LDKImageInfo *heightmap);
/** Change terrain metadata without replacing its heightmap. Dimensions must
 * be unchanged; layer masks follow stable IDs if the layer order changes.
 * Use ldk_terrain_component_set for heightmap/sculpting changes. */
LDK_API bool ldk_terrain_component_metadata_set(
    LDKTerrainComponent *component, const LDKTerrain *terrain);
/** Update only the image and keep terrain metadata. */
LDK_API bool ldk_terrain_component_heightmap_set(
    LDKTerrainComponent *component, const LDKImageInfo *heightmap);
/** Publish only modified pixels from a full-size heightmap buffer, preserving
 * the component's image and accumulating a dirty rectangle for the renderer.
 * Full image or height-range changes must still use component_set(). */
LDK_API bool ldk_terrain_component_heightmap_region_set(
    LDKTerrainComponent *component, const LDKImageInfo *heightmap,
    u32 x, u32 y, u32 width, u32 height);
LDK_API bool ldk_terrain_component_heightmap_info_get(
    const LDKTerrainComponent *component, LDKImageInfo *out_info);
/** Replace a vegetation layer's R8 mask with a deep copy. NULL clears it.
 * The layer ID is stable even when layers move in the metadata array.
 */
/* Publish a changed rectangle from an interleaved 8-channel paint image.
 * The complete source buffer is borrowed, not retained. */
LDK_API bool ldk_terrain_component_surface_paint_region_set(
    LDKTerrainComponent *component, const u8 *samples,
    u32 x, u32 y, u32 width, u32 height);
LDK_API bool ldk_terrain_component_surface_paint_set(
    LDKTerrainComponent *component, const u8 *samples);
/* Keep paint attached to its Surface when the Inspector reorders slots. */
LDK_API bool ldk_terrain_component_surface_paint_swap(
    LDKTerrainComponent *component, u32 a, u32 b);
LDK_API bool ldk_terrain_component_surface_paint_remove(
    LDKTerrainComponent *component, u32 index);
LDK_API bool ldk_terrain_component_vegetation_density_set(
    LDKTerrainComponent *component, u64 layer_id, const u8 *samples);
/** Publish a rectangular edit from a complete R8 layer buffer without
 * replacing the owned mask. Unmodified samples and other layers survive. */
LDK_API bool ldk_terrain_component_vegetation_density_region_set(
    LDKTerrainComponent *component, u64 layer_id, const u8 *samples,
    u32 x, u32 y, u32 width, u32 height);
/** Add/remove a vegetation layer while keeping its owned mask aligned. */
LDK_API bool ldk_terrain_component_vegetation_layer_add(
    LDKTerrainComponent *component, const LDKGrassTypeDesc *type,
    float density_scale, u32 seed, u64 *out_id);
LDK_API bool ldk_terrain_component_vegetation_layer_remove(
    LDKTerrainComponent *component, u64 layer_id);
LDK_API const u8 *ldk_terrain_component_vegetation_density_get(
    const LDKTerrainComponent *component, u64 layer_id);

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_terrain_component_desc(u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif // LDK_TERRAIN_COMPONENT_H
