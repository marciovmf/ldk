/**
 * # Terrain component
 *
 * An entity-local reference to an editable .terrain asset. Geometry and
 * vegetation belong to the asset; position/orientation come from Transform.
 * Terrain heights are single-valued: keep the entity upright (yaw is allowed).
 */
#ifndef LDK_TERRAIN_COMPONENT_H
#define LDK_TERRAIN_COMPONENT_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.component.terrain */
//@component
typedef struct LDKTerrainComponent
{
  /** Shared terrain definition and authored vegetation density masks. */
  LDKAssetTerrain terrain;
} LDKTerrainComponent;

LDK_API LDKTerrainComponent ldk_terrain_component_make_default(void);
#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_terrain_component_desc(u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif // LDK_TERRAIN_COMPONENT_H
