/**
 * # Terrain follower component
 *
 * Vertically follows the surface exposed by LDKTerrainSystem.
 */

#ifndef LDK_TERRAIN_FOLLOWER_H
#define LDK_TERRAIN_FOLLOWER_H

#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group components.terrain_follower */
//@component
typedef struct LDKTerrainFollowerComponent
{
  /* Distance from the heightmap surface to the entity pivot, in world units. */
  float height_offset;
} LDKTerrainFollowerComponent;

/**
 * @brief Create default settings for terrain follower component.
 * @return A value initialized with the engine defaults.
 * @group components.terrain_follower
 */
LDK_API LDKTerrainFollowerComponent
ldk_terrain_follower_component_make_default(void);

#ifdef LDK_ENGINE
/**
 * @brief Build the ECS registration descriptor for the terrain follower component.
 * @param initial_capacity Initial number of entries reserved in the registry.
 * @return A descriptor for the requested component or resource.
 * @group components.terrain_follower.registration
 */
LDK_API LDKComponentDesc ldk_terrain_follower_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_FOLLOWER_H
