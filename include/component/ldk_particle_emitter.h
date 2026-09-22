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
    Vec3 initial_tint;
    Vec3 final_tint;
    float age;
    float initial_scale;
    float final_scale;
    float initial_alpha;
    float final_alpha;
    float initial_rotation;
    float final_rotation;
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

    /* Visual endpoint variations are absolute +/- ranges sampled at spawn.
     * Tint variation uses RGB channel magnitudes; its alpha byte is ignored.
     * Rotation is billboard-local +Z rotation in radians.
     */
    float initial_scale;
    float initial_scale_variation;
    float final_scale;
    float final_scale_variation;
    float initial_alpha;
    float initial_alpha_variation;
    float final_alpha;
    float final_alpha_variation;
    //@inspect widget=COLOR
    u32 initial_tint;
    //@inspect widget=COLOR
    u32 initial_tint_variation;
    //@inspect widget=COLOR
    u32 final_tint;
    //@inspect widget=COLOR
    u32 final_tint_variation;
    float initial_rotation;
    float initial_rotation_variation;
    float final_rotation;
    float final_rotation_variation;
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
