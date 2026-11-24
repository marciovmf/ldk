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

/** Optional owned R8 mask buffers, indexed in vegetation layer order. Keep
 * the C array outside the reflected component: Comet does not support C
 * arrays as inspector fields. */
typedef struct LDKTerrainVegetationDensity
{
  u8 *samples[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
} LDKTerrainVegetationDensity;

/** @group ldk.component.terrain */
//@component
typedef struct LDKTerrainComponent
{
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
  //@inspect hidden runtime
  u64 revision;
} LDKTerrainComponent;

LDK_API LDKTerrainComponent ldk_terrain_component_make_default(void);
/** Atomically replace terrain data and optional heightmap (deep copy).
 * A NULL heightmap represents a flat grid. The metadata and image dimensions
 * must match. The component retains its prior state on failure.
 */
LDK_API bool ldk_terrain_component_set(LDKTerrainComponent *component,
    const LDKTerrain *terrain, const LDKImageInfo *heightmap);
/** Update only the image and keep terrain metadata. */
LDK_API bool ldk_terrain_component_heightmap_set(
    LDKTerrainComponent *component, const LDKImageInfo *heightmap);
LDK_API bool ldk_terrain_component_heightmap_info_get(
    const LDKTerrainComponent *component, LDKImageInfo *out_info);
/** Replace a vegetation layer's R8 mask with a deep copy. NULL clears it.
 * The layer ID is stable even when layers move in the metadata array.
 */
LDK_API bool ldk_terrain_component_vegetation_density_set(
    LDKTerrainComponent *component, u64 layer_id, const u8 *samples);
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
