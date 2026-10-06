/**
 * @file ldk_terrain_follower_system.h
 * @brief Engine-native heightmap terrain following.
 */
#ifndef LDK_TERRAIN_FOLLOWER_SYSTEM_H
#define LDK_TERRAIN_FOLLOWER_SYSTEM_H

#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LDK_ENGINE
//@system name=LDKTerrainFollowerSystem flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE order=50
void ldk_terrain_follower_system_update(
    void *data, const LDKEntityGroup *group, float dt);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_TERRAIN_FOLLOWER_SYSTEM_H
