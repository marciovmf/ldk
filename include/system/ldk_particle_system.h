/**
 * # Particle system
 *
 * Engine-native particle simulation system.
 */

#ifndef LDK_PARTICLE_SYSTEM_H
#define LDK_PARTICLE_SYSTEM_H

#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef LDK_ENGINE
/**
 * @brief Initialize the particle system.
 * @param data Source data or system-owned context for this operation.
 * @return System initialization status code.
 * @group systems.particle
 */
int ldk_particle_system_initialize(void *data);
/**
 * @brief Shut down the particle system.
 * @param data Source data or system-owned context for this operation.
 * @group systems.particle
 */
void ldk_particle_system_terminate(void *data);
/**
 * @brief Update the particle system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group systems.particle
 */
//@system name=ParticleSystem initialize=ldk_particle_system_initialize terminate=ldk_particle_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE bucket=LDK_SYSTEM_BUCKET_POST_UPDATE
void ldk_particle_system_update(
    void *data, const LDKEntityGroup *group, float dt);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_PARTICLE_SYSTEM_H
