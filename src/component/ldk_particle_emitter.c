#include <component/ldk_particle_emitter.h>

#include <component/ldk_instanced_mesh_source.h>
#include <ldk.h>
#include <ldk_material_asset.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>


LDKParticleEmitter ldk_particle_emitter_make_default(void)
{
  LDKParticleEmitter emitter;
  memset(&emitter, 0, sizeof(emitter));
  emitter.enabled = true;
  emitter.material = ldk_asset_material_null();
  emitter.emission_rate = 10.0f;
  emitter.emission_mode = LDK_PARTICLE_EMISSION_CONTINUOUS;
  emitter.play_on_start = true;
  emitter.emission_duration = 1.0f;
  emitter.burst_count = 1u;
  emitter.particles_per_burst = 16u;
  emitter.burst_interval = 0.1f;
  emitter.max_particles = 256u;
  emitter.lifetime = 1.0f;
  emitter.initial_velocity = vec3_make(0.0f, 1.0f, 0.0f);
  emitter.velocity_variation = vec3_make(0.0f, 0.0f, 0.0f);
  emitter.acceleration = vec3_make(0.0f, 0.0f, 0.0f);
  emitter.initial_scale = 1.0f;
  emitter.initial_scale_variation = 0.0f;
  emitter.final_scale = 1.0f;
  emitter.final_scale_variation = 0.0f;
  emitter.initial_alpha = 1.0f;
  emitter.initial_alpha_variation = 0.0f;
  emitter.final_alpha = 1.0f;
  emitter.final_alpha_variation = 0.0f;
  emitter.initial_tint = 0xffffffffu;
  emitter.initial_tint_variation = 0x000000ffu;
  emitter.final_tint = 0xffffffffu;
  emitter.final_tint_variation = 0x000000ffu;
  emitter.initial_rotation = 0.0f;
  emitter.initial_rotation_variation = 0.0f;
  emitter.final_rotation = 0.0f;
  emitter.final_rotation_variation = 0.0f;
  emitter.interpolation_curve = LDK_PARTICLE_INTERPOLATION_LINEAR;
  emitter.simulation_space = LDK_PARTICLE_SIMULATION_SPACE_LOCAL;
  emitter.sort_mode = LDK_PARTICLE_SORT_BACK_TO_FRONT;
  emitter.random_state = 0x6d2b79f5u;
  return emitter;
}

static bool s_particle_emitter_start(LDKParticleEmitter *emitter,
    LDKParticleEmissionMode mode, float duration, u32 bursts)
{
  if (!emitter || (mode != LDK_PARTICLE_EMISSION_CONTINUOUS &&
          mode != LDK_PARTICLE_EMISSION_DURATION &&
          mode != LDK_PARTICLE_EMISSION_BURSTS) ||
      (mode == LDK_PARTICLE_EMISSION_DURATION &&
          (!isfinite(duration) || duration <= 0.0f)) ||
      (mode == LDK_PARTICLE_EMISSION_BURSTS && !bursts))
  {
    return false;
  }

  emitter->playing = true;
  emitter->playback_started = true;
  emitter->active_emission_mode = mode;
  emitter->active_duration = duration;
  emitter->active_burst_count = bursts;
  emitter->emission_elapsed = 0.0f;
  emitter->bursts_emitted = 0u;
  emitter->emission_accumulator = 0.0f;
  return true;
}

bool ldk_particle_emitter_play(LDKParticleEmitter *emitter)
{
  if (!emitter)
  {
    return false;
  }
  if (emitter->playing)
  {
    return true;
  }
  return s_particle_emitter_start(emitter, emitter->emission_mode,
      emitter->emission_duration, emitter->burst_count);
}

bool ldk_particle_emitter_restart(LDKParticleEmitter *emitter)
{
  return emitter && s_particle_emitter_start(emitter, emitter->emission_mode,
      emitter->emission_duration, emitter->burst_count);
}

void ldk_particle_emitter_stop(LDKParticleEmitter *emitter)
{
  if (emitter)
  {
    emitter->playing = false;
    emitter->playback_started = true;
    emitter->emission_accumulator = 0.0f;
  }
}

bool ldk_particle_emitter_emit_bursts(LDKParticleEmitter *emitter, u32 count)
{
  return s_particle_emitter_start(emitter, LDK_PARTICLE_EMISSION_BURSTS,
      0.0f, count);
}

bool ldk_particle_emitter_emit_for(LDKParticleEmitter *emitter, float seconds)
{
  return s_particle_emitter_start(emitter, LDK_PARTICLE_EMISSION_DURATION,
      seconds, 0u);
}

void ldk_particle_emitter_clear(LDKParticleEmitter *emitter)
{
  if (!emitter)
  {
    return;
  }

  free(emitter->particles);
  emitter->particles = NULL;
  emitter->particle_count = 0;
  emitter->particle_capacity = 0;
  emitter->emission_accumulator = 0.0f;
  emitter->runtime_error_flags = 0u;
}

#ifdef LDK_ENGINE
static bool s_particle_emitter_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)entities;
  (void)components;
  (void)index;
  (void)user;

  LDKParticleEmitter *target = component;
  if (!target)
  {
    return false;
  }

  LDKParticleEmitter value = initial_value
      ? *(const LDKParticleEmitter *)initial_value
      : ldk_particle_emitter_make_default();
  value.particles = NULL;
  value.particle_count = 0;
  value.particle_capacity = 0;
  value.playing = false;
  value.playback_started = false;
  value.active_emission_mode = value.emission_mode;
  value.active_duration = value.emission_duration;
  value.active_burst_count = value.burst_count;
  value.emission_elapsed = 0.0f;
  value.bursts_emitted = 0u;
  value.emission_accumulator = 0.0f;
  value.runtime_error_flags = 0u;
  value.random_state ^= entity.index + 0x9e3779b9u;
  if (value.random_state == 0)
  {
    value.random_state = 0x6d2b79f5u;
  }
  *target = value;

  if (!ldk_entity_component_flags_add(entities, entity,
          LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE,
          LDK_COMPONENT_INSTANCE_FLAG_HIDE_IN_EDITOR))
  {
    return false;
  }

  return true;
}

static void s_particle_emitter_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  (void)index;
  (void)user;

  LDKInstancedMeshSource *mesh = ldk_entity_component_get(entities,
      components, entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
  if (mesh && !ldk_instanced_mesh_source_resize_instances(mesh, 0u))
  {
    ldk_log_error(
        "ParticleEmitter entity %u: failed to clear particle instances.\n",
        entity.index);
  }

  (void)ldk_entity_component_flags_remove(entities, entity,
      LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE,
      LDK_COMPONENT_INSTANCE_FLAG_HIDE_IN_EDITOR);

  ldk_particle_emitter_clear((LDKParticleEmitter *)component);
}

LDKComponentDesc ldk_particle_emitter_component_desc(u32 initial_capacity)
{
  static const u32 required_components[] = {
      LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE};
  LDKComponentDesc desc = {0};
  desc.name = "ParticleEmitter";
  desc.type = LDK_COMPONENT_TYPE_PARTICLE_EMITTER;
  desc.entry_size = sizeof(LDKParticleEmitter);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required_components;
  desc.required_component_count =
      (u32)(sizeof(required_components) / sizeof(required_components[0]));
  desc.attach = s_particle_emitter_attach;
  desc.destroy = s_particle_emitter_destroy;
  return desc;
}
#endif
