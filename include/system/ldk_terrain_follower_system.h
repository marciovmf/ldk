/**
 * # Terrain follower system
 *
 * Engine-native heightmap terrain following.
 */

#ifndef LDK_TERRAIN_FOLLOWER_SYSTEM_H
#define LDK_TERRAIN_FOLLOWER_SYSTEM_H

#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LDK_ENGINE
/**
 * @brief Update the terrain follower system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 */
//@system components=LDK_COMPONENT_TYPE_TERRAIN_FOLLOWER name=LDKTerrainFollowerSystem flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE order=50
void ldk_terrain_follower_system_update(
    void *data, const LDKEntityGroup *group, float dt);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_FOLLOWER_SYSTEM_H
