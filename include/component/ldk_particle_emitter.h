/**
 * @file ldk_particle_emitter.h
 * @brief Particle emitter component.
 */
#ifndef LDK_PARTICLE_EMITTER_H
#define LDK_PARTICLE_EMITTER_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_component.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //@enum
  typedef enum LDKParticleSimulationSpace
  {
    LDK_PARTICLE_SIMULATION_SPACE_LOCAL = 0,
    LDK_PARTICLE_SIMULATION_SPACE_WORLD
  } LDKParticleSimulationSpace;

  //@enum
  typedef enum LDKParticleSortMode
  {
    LDK_PARTICLE_SORT_NONE = 0,
    LDK_PARTICLE_SORT_BACK_TO_FRONT
  } LDKParticleSortMode;

  typedef struct LDKParticleRuntime
  {
    Vec3 position;
    Vec3 velocity;
    float age;
  } LDKParticleRuntime;

  //@component
  typedef struct LDKParticleEmitter
  {
    bool enabled;
    LDKAssetMaterial material;
    float emission_rate;
    u32 max_particles;
    float lifetime;
    Vec3 initial_velocity;
    Vec3 velocity_variation;
    Vec3 acceleration;
    float initial_scale;
    float final_scale;
    LDKParticleSimulationSpace simulation_space;
    LDKParticleSortMode sort_mode;

    //@inspect hidden runtime
    LDKParticleRuntime *particles;
    //@inspect readonly runtime
    u32 particle_count;
    //@inspect hidden runtime
    u32 particle_capacity;
    //@inspect hidden runtime
    float emission_accumulator;
    //@inspect hidden runtime
    u32 random_state;
    //@inspect hidden runtime
    u32 runtime_error_flags;
  } LDKParticleEmitter;

  LDK_API LDKParticleEmitter ldk_particle_emitter_make_default(void);
  LDK_API void ldk_particle_emitter_clear(LDKParticleEmitter *emitter);

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_particle_emitter_component_desc(
      u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_PARTICLE_EMITTER_H
