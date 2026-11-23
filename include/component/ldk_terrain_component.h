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

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_terrain_component_desc(u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif // LDK_TERRAIN_COMPONENT_H
