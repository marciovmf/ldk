/**
 * # Particle emitter component
 *
 * Particle emitter component.
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

  /** @group components.particle_emitter */
  //@enum
  typedef enum LDKParticleSimulationSpace
  {
    LDK_PARTICLE_SIMULATION_SPACE_LOCAL = 0,
    LDK_PARTICLE_SIMULATION_SPACE_WORLD
  } LDKParticleSimulationSpace;

  /** @group components.particle_emitter */
  //@enum
  typedef enum LDKParticleSortMode
  {
    LDK_PARTICLE_SORT_NONE = 0,
    LDK_PARTICLE_SORT_BACK_TO_FRONT
  } LDKParticleSortMode;

  /** @group components.particle_emitter */
  //@enum
  typedef enum LDKParticleInterpolationCurve
  {
    LDK_PARTICLE_INTERPOLATION_LINEAR = 0,
    LDK_PARTICLE_INTERPOLATION_EASE_IN_QUADRATIC,
    LDK_PARTICLE_INTERPOLATION_EASE_OUT_QUADRATIC,
    LDK_PARTICLE_INTERPOLATION_EASE_IN_OUT_QUADRATIC,
    LDK_PARTICLE_INTERPOLATION_SMOOTHSTEP
  } LDKParticleInterpolationCurve;

  /** @group components.particle_emitter */
  //@enum
  typedef enum LDKParticleEmissionMode
  {
    LDK_PARTICLE_EMISSION_CONTINUOUS = 0,
    LDK_PARTICLE_EMISSION_DURATION,
    LDK_PARTICLE_EMISSION_BURSTS
  } LDKParticleEmissionMode;

  /** @group components.particle_emitter */
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

  /** @group components.particle_emitter.emission */
  //@component
  typedef struct LDKParticleEmitter
  {
    bool enabled;
    LDKAssetMaterial material;
    float emission_rate;
    LDKParticleEmissionMode emission_mode;
    bool play_on_start;
    float emission_duration;
    u32 burst_count;
    u32 particles_per_burst;
    float burst_interval;
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
    LDKParticleInterpolationCurve interpolation_curve;
    LDKParticleSimulationSpace simulation_space;
    LDKParticleSortMode sort_mode;

    /* Playback commands select an active mode without changing authored
     * configuration. Existing particles survive Stop and Restart. */
    //@inspect hidden runtime
    bool playing;
    //@inspect hidden runtime
    bool playback_started;
    //@inspect hidden runtime
    LDKParticleEmissionMode active_emission_mode;
    //@inspect hidden runtime
    float active_duration;
    //@inspect hidden runtime
    u32 active_burst_count;
    //@inspect hidden runtime
    float emission_elapsed;
    //@inspect hidden runtime
    u32 bursts_emitted;

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

  /**
   * @brief Create default settings for particle emitter.
   * @return A value initialized with the engine defaults.
   * @group components.particle_emitter.emission
   */
  LDK_API LDKParticleEmitter ldk_particle_emitter_make_default(void);
  /** Start the configured mode if not playing.
   * @param emitter Particle emitter component being controlled.
   * @return True if the operation succeeds; false otherwise.
   * @group components.particle_emitter.emission
   */
  LDK_API bool ldk_particle_emitter_play(LDKParticleEmitter *emitter);
  /** Restart the configured mode without killing existing particles.
   * @param emitter Particle emitter component being controlled.
   * @return True if the operation succeeds; false otherwise.
   * @group components.particle_emitter.emission
   */
  LDK_API bool ldk_particle_emitter_restart(LDKParticleEmitter *emitter);
  /** Stop future emission; live particles continue simulating.
   * @param emitter Particle emitter component being controlled.
   * @group components.particle_emitter.emission
   */
  LDK_API void ldk_particle_emitter_stop(LDKParticleEmitter *emitter);
  /** Play the configured burst parameters for exactly count bursts.
   * @param emitter Particle emitter component being controlled.
   * @param count Number of items to process.
   * @return True if the operation succeeds; false otherwise.
   * @group components.particle_emitter.emission
   */
  LDK_API bool ldk_particle_emitter_emit_bursts(
      LDKParticleEmitter *emitter, u32 count);
  /** Play the configured emission rate for the specified duration.
   * @param emitter Particle emitter component being controlled.
   * @param seconds Duration in seconds.
   * @return True if the operation succeeds; false otherwise.
   * @group components.particle_emitter.emission
   */
  LDK_API bool ldk_particle_emitter_emit_for(
      LDKParticleEmitter *emitter, float seconds);
  /** Remove all live particles without stopping scheduled emission.
   * @param emitter Particle emitter component being controlled.
   * @group components.particle_emitter.emission
   */
  LDK_API void ldk_particle_emitter_clear(LDKParticleEmitter *emitter);

#ifdef LDK_ENGINE
  /**
   * @brief Build the ECS registration descriptor for the particle emitter component.
   * @param initial_capacity Initial number of entries reserved in the registry.
   * @return A descriptor for the requested component or resource.
   * @group components.particle_emitter.registration
   */
  LDK_API LDKComponentDesc ldk_particle_emitter_component_desc(
      u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_PARTICLE_EMITTER_H
