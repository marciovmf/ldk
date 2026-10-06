/**
 * @file ldk_terrain_follower.h
 * @brief Vertically follows the surface exposed by LDKTerrainSystem.
 */
#ifndef LDK_TERRAIN_FOLLOWER_H
#define LDK_TERRAIN_FOLLOWER_H

#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

//@component
typedef struct LDKTerrainFollowerComponent
{
  /* Distance from the heightmap surface to the entity pivot, in world units. */
  float height_offset;
} LDKTerrainFollowerComponent;

LDK_API LDKTerrainFollowerComponent
ldk_terrain_follower_component_make_default(void);

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_terrain_follower_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_FOLLOWER_H
