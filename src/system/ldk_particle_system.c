#include <system/ldk_particle_system.h>

#include <component/ldk_camera.h>
#include <component/ldk_instanced_mesh_source.h>
#include <component/ldk_particle_emitter.h>
#include <component/ldk_transform.h>
#include <ldk.h>
#include <ldk_mesh.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <math.h>
#include <stdlib.h>

typedef enum LDKParticleEmitterRuntimeError
{
  LDK_PARTICLE_EMITTER_ERROR_NONE = 0,
  LDK_PARTICLE_EMITTER_ERROR_INVALID_CONFIG = 1 << 0,
  LDK_PARTICLE_EMITTER_ERROR_TRANSFORM = 1 << 1,
  LDK_PARTICLE_EMITTER_ERROR_PROXY = 1 << 2,
  LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP = 1 << 3,
  LDK_PARTICLE_EMITTER_ERROR_STORAGE = 1 << 4,
  LDK_PARTICLE_EMITTER_ERROR_INSTANCES = 1 << 5,
  LDK_PARTICLE_EMITTER_ERROR_EMISSION = 1 << 6
} LDKParticleEmitterRuntimeError;

typedef enum LDKParticleCameraDiagnostic
{
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_VALID = 0,
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_MISSING,
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_MULTIPLE,
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_DISABLED,
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_TRANSFORM,
  LDK_PARTICLE_CAMERA_DIAGNOSTIC_UNSET = UINT32_MAX
} LDKParticleCameraDiagnostic;

static u32 s_camera_diagnostic = LDK_PARTICLE_CAMERA_DIAGNOSTIC_UNSET;
static bool s_dt_error_logged = false;

static bool s_vec3_is_finite(Vec3 value)
{
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool s_asset_handle_equal(XHandle a, XHandle b)
{
  return a.index == b.index && a.version == b.version;
}

static void s_emitter_error_set(LDKParticleEmitter *emitter,
    LDKEntity entity, u32 flag, const char *message)
{
  if (!emitter || !message)
  {
    return;
  }

  if ((emitter->runtime_error_flags & flag) == 0u)
  {
    ldk_log_error(
        "ParticleEmitter entity %u: %s\n", entity.index, message);
  }
  emitter->runtime_error_flags |= flag;
}

static void s_emitter_error_clear(LDKParticleEmitter *emitter, u32 flag)
{
  if (emitter)
  {
    emitter->runtime_error_flags &= ~flag;
  }
}

static void s_camera_diagnostic_set(u32 diagnostic)
{
  if (s_camera_diagnostic == diagnostic)
  {
    return;
  }

  s_camera_diagnostic = diagnostic;
  switch (diagnostic)
  {
  case LDK_PARTICLE_CAMERA_DIAGNOSTIC_MISSING:
    ldk_log_error(
        "ParticleSystem requires exactly one camera with role MAIN. "
        "No MAIN camera was found.\n");
    break;
  case LDK_PARTICLE_CAMERA_DIAGNOSTIC_MULTIPLE:
    ldk_log_error(
        "ParticleSystem requires exactly one camera with role MAIN. "
        "Multiple MAIN cameras were found.\n");
    break;
  case LDK_PARTICLE_CAMERA_DIAGNOSTIC_DISABLED:
    ldk_log_error("ParticleSystem MAIN camera is disabled.\n");
    break;
  case LDK_PARTICLE_CAMERA_DIAGNOSTIC_TRANSFORM:
    ldk_log_error(
        "ParticleSystem failed to resolve the MAIN camera transform.\n");
    break;
  default:
    break;
  }
}

static bool s_emitter_is_valid(const LDKParticleEmitter *emitter)
{
  return emitter && isfinite(emitter->emission_rate) &&
      emitter->emission_rate >= 0.0f &&
      emitter->max_particles <= UINT32_MAX / sizeof(LDKParticleRuntime) &&
      isfinite(emitter->lifetime) && emitter->lifetime > 0.0f &&
      s_vec3_is_finite(emitter->initial_velocity) &&
      s_vec3_is_finite(emitter->velocity_variation) &&
      s_vec3_is_finite(emitter->acceleration) &&
      isfinite(emitter->initial_scale) && emitter->initial_scale >= 0.0f &&
      isfinite(emitter->final_scale) && emitter->final_scale >= 0.0f &&
      (emitter->simulation_space == LDK_PARTICLE_SIMULATION_SPACE_LOCAL ||
          emitter->simulation_space == LDK_PARTICLE_SIMULATION_SPACE_WORLD) &&
      (emitter->sort_mode == LDK_PARTICLE_SORT_NONE ||
          emitter->sort_mode == LDK_PARTICLE_SORT_BACK_TO_FRONT);
}

static bool s_particle_reserve(LDKParticleEmitter *emitter, u32 count)
{
  if (count <= emitter->particle_capacity)
  {
    return true;
  }

  u32 capacity = emitter->particle_capacity ? emitter->particle_capacity : 16u;
  while (capacity < count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = count;
      break;
    }
    capacity *= 2u;
  }
  if (capacity > emitter->max_particles)
  {
    capacity = emitter->max_particles;
  }
  if (capacity < count || capacity > UINT32_MAX / sizeof(LDKParticleRuntime))
  {
    return false;
  }

  LDKParticleRuntime *particles = realloc(
      emitter->particles, (size_t)capacity * sizeof(LDKParticleRuntime));
  if (!particles)
  {
    return false;
  }
  emitter->particles = particles;
  emitter->particle_capacity = capacity;
  return true;
}

static u32 s_random_next(LDKParticleEmitter *emitter)
{
  u32 value = emitter->random_state;
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  emitter->random_state = value ? value : 0x6d2b79f5u;
  return emitter->random_state;
}

static float s_random_signed(LDKParticleEmitter *emitter)
{
  return ((float)(s_random_next(emitter) >> 8u) / 16777215.0f) * 2.0f - 1.0f;
}

static Vec3 s_random_velocity(LDKParticleEmitter *emitter)
{
  Vec3 variation = vec3_make(
      emitter->velocity_variation.x * s_random_signed(emitter),
      emitter->velocity_variation.y * s_random_signed(emitter),
      emitter->velocity_variation.z * s_random_signed(emitter));
  return vec3_add(emitter->initial_velocity, variation);
}

static bool s_main_camera_get(Mat4 *out_view, Quat *out_rotation)
{
  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  XArray *cameras;
  XArray *owners;
  const LDKCamera *main_camera = NULL;
  const LDKEntity *main_entity = NULL;
  u32 main_count = 0;

  if (!components || !out_view || !out_rotation)
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_MISSING);
    return false;
  }

  cameras = ldk_component_store_get(components, LDK_COMPONENT_TYPE_CAMERA);
  owners = ldk_component_owners_get(components, LDK_COMPONENT_TYPE_CAMERA);
  if (!cameras || !owners)
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_MISSING);
    return false;
  }

  for (u32 i = 0; i < x_array_count(cameras); ++i)
  {
    const LDKCamera *camera = x_array_get(cameras, i);
    const LDKEntity *entity = x_array_get(owners, i);
    if (!camera || !entity || camera->role != LDK_CAMERA_ROLE_MAIN)
    {
      continue;
    }

    ++main_count;
    main_camera = camera;
    main_entity = entity;
  }

  if (main_count == 0u)
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_MISSING);
    return false;
  }
  if (main_count != 1u)
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_MULTIPLE);
    return false;
  }
  if (!main_camera->enabled)
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_DISABLED);
    return false;
  }

  Mat4 camera_world = mat4_identity();
  Vec3 translation = vec3_make(0.0f, 0.0f, 0.0f);
  Vec3 scale = vec3_make(1.0f, 1.0f, 1.0f);
  if (!ldk_scenegraph_update_entity(*main_entity) ||
      !ldk_camera_get_world_matrix(*main_entity, &camera_world))
  {
    s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_TRANSFORM);
    return false;
  }

  mat4_decompose(camera_world, &translation, out_rotation, &scale);
  *out_view = mat4_inverse_affine(camera_world);
  s_camera_diagnostic_set(LDK_PARTICLE_CAMERA_DIAGNOSTIC_VALID);
  return true;
}

static bool s_particle_spawn(LDKParticleEmitter *emitter, Mat4 emitter_world,
    Quat emitter_rotation)
{
  if (emitter->particle_count >= emitter->max_particles)
  {
    return true;
  }
  if (!s_particle_reserve(emitter, emitter->particle_count + 1u))
  {
    return false;
  }

  LDKParticleRuntime *particle =
      &emitter->particles[emitter->particle_count++];
  particle->age = 0.0f;
  particle->velocity = s_random_velocity(emitter);
  particle->position = vec3_make(0.0f, 0.0f, 0.0f);

  if (emitter->simulation_space == LDK_PARTICLE_SIMULATION_SPACE_WORLD)
  {
    particle->position = mat4_mul_point(emitter_world, particle->position);
    particle->velocity = quat_mul_vec3(emitter_rotation, particle->velocity);
  }
  return true;
}

static void s_particle_simulate(LDKParticleEmitter *emitter, float dt)
{
  u32 i = 0;
  while (i < emitter->particle_count)
  {
    LDKParticleRuntime *particle = &emitter->particles[i];
    particle->age += dt;
    if (particle->age >= emitter->lifetime)
    {
      emitter->particles[i] = emitter->particles[--emitter->particle_count];
      continue;
    }

    particle->velocity =
        vec3_add(particle->velocity, vec3_mul(emitter->acceleration, dt));
    particle->position =
        vec3_add(particle->position, vec3_mul(particle->velocity, dt));
    ++i;
  }
}

static void s_particle_emit(LDKParticleEmitter *emitter, LDKEntity entity,
    Mat4 emitter_world, Quat emitter_rotation, float dt)
{
  if (!emitter->enabled || emitter->emission_rate <= 0.0f ||
      emitter->max_particles == 0)
  {
    emitter->emission_accumulator = 0.0f;
    s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_EMISSION);
    return;
  }

  emitter->emission_accumulator += emitter->emission_rate * dt;
  if (!isfinite(emitter->emission_accumulator))
  {
    emitter->emission_accumulator = 0.0f;
    s_emitter_error_set(emitter, entity, LDK_PARTICLE_EMITTER_ERROR_EMISSION,
        "emission accumulator became invalid.");
    return;
  }
  s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_EMISSION);

  u32 available = emitter->max_particles - emitter->particle_count;
  if (!available)
  {
    emitter->emission_accumulator = fminf(emitter->emission_accumulator, 1.0f);
    return;
  }

  u32 spawn_count = available;
  if (emitter->emission_accumulator < (float)available)
  {
    spawn_count = (u32)emitter->emission_accumulator;
  }

  emitter->emission_accumulator -= (float)spawn_count;
  if (spawn_count == available)
  {
    emitter->emission_accumulator = fminf(emitter->emission_accumulator, 1.0f);
  }

  for (u32 i = 0; i < spawn_count; ++i)
  {
    if (!s_particle_spawn(emitter, emitter_world, emitter_rotation))
    {
      s_emitter_error_set(emitter, entity, LDK_PARTICLE_EMITTER_ERROR_STORAGE,
          "failed to allocate particle storage.");
      return;
    }
  }
  s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_STORAGE);
}

static bool s_particle_proxy_prepare(LDKParticleEmitter *emitter,
    LDKEntity entity, LDKInstancedMeshSource *mesh)
{
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  LDKAssetMesh quad;

  if (!assets)
  {
    s_emitter_error_set(emitter, entity,
        LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP,
        "asset manager is unavailable.");
    return false;
  }

  quad = ldk_mesh_primitive_asset_get(assets, LDK_MESH_PRIMITIVE_QUAD);
  if (x_handle_is_null(quad.h))
  {
    s_emitter_error_set(emitter, entity,
        LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP,
        "failed to create the builtin particle quad.");
    return false;
  }

  if (!s_asset_handle_equal(mesh->source.source_asset.h, quad.h) &&
      !ldk_mesh_source_set_data(&mesh->source, quad))
  {
    s_emitter_error_set(emitter, entity,
        LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP,
        "failed to assign the builtin particle quad.");
    return false;
  }

  mesh->source.casts_shadows = false;
  if (!x_handle_is_null(emitter->material.h))
  {
    if (!ldk_mesh_source_set_material_asset(
            &mesh->source, assets, emitter->material))
    {
      s_emitter_error_set(emitter, entity,
          LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP,
          "failed to bind the particle material.");
      return false;
    }
  }
  else
  {
    LDKMaterialDesc material;
    ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &material);
    if (!ldk_mesh_source_set_material(&mesh->source, &material))
    {
      s_emitter_error_set(emitter, entity,
          LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP,
          "failed to restore the default particle material.");
      return false;
    }
  }

  s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_PROXY_SETUP);
  return true;
}

static bool s_particle_instances_clear(LDKParticleEmitter *emitter,
    LDKEntity entity, LDKInstancedMeshSource *mesh)
{
  if (!mesh)
  {
    return true;
  }

  if (!ldk_instanced_mesh_source_resize_instances(mesh, 0u))
  {
    s_emitter_error_set(emitter, entity, LDK_PARTICLE_EMITTER_ERROR_INSTANCES,
        "failed to clear particle instance storage.");
    return false;
  }

  s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_INSTANCES);
  return true;
}

static bool s_particle_instances_write(LDKParticleEmitter *emitter,
    LDKEntity entity, LDKInstancedMeshSource *mesh, Mat4 emitter_world,
    Quat camera_rotation)
{
  if (!ldk_instanced_mesh_source_resize_instances(
          mesh, emitter->particle_count))
  {
    s_emitter_error_set(emitter, entity,
        LDK_PARTICLE_EMITTER_ERROR_INSTANCES,
        "failed to resize particle instance storage.");
    return false;
  }
  s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_INSTANCES);

  Mat4 inverse_emitter = mat4_inverse_affine(emitter_world);
  for (u32 i = 0; i < emitter->particle_count; ++i)
  {
    LDKParticleRuntime *particle = &emitter->particles[i];
    float t = particle->age / emitter->lifetime;
    float scale = emitter->initial_scale +
        (emitter->final_scale - emitter->initial_scale) * t;
    Vec3 position = particle->position;
    if (emitter->simulation_space == LDK_PARTICLE_SIMULATION_SPACE_LOCAL)
    {
      position = mat4_mul_point(emitter_world, position);
    }

    Mat4 particle_world = mat4_compose(position, camera_rotation,
        vec3_make(scale, scale, scale));
    mesh->instances[i] = mat4_mul(inverse_emitter, particle_world);
  }
  return true;
}

static float s_particle_instance_depth(
    Mat4 local, Mat4 emitter_world, Mat4 camera_view)
{
  Mat4 world = mat4_mul(emitter_world, local);
  Vec3 position = vec3_make(world.m[12], world.m[13], world.m[14]);
  return -mat4_mul_point(camera_view, position).z;
}

static void s_particle_instances_sort_back_to_front(
    LDKInstancedMeshSource *mesh, Mat4 emitter_world, Mat4 camera_view)
{
  for (u32 i = 1; i < mesh->instance_count; ++i)
  {
    Mat4 instance = mesh->instances[i];
    float depth =
        s_particle_instance_depth(instance, emitter_world, camera_view);
    u32 j = i;
    while (j > 0)
    {
      float previous_depth = s_particle_instance_depth(
          mesh->instances[j - 1u], emitter_world, camera_view);
      if (previous_depth >= depth)
      {
        break;
      }
      mesh->instances[j] = mesh->instances[j - 1u];
      --j;
    }
    mesh->instances[j] = instance;
  }
}

static void s_particle_runtime_reset(void)
{
  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  XArray *emitters;
  XArray *owners;

  if (!components)
  {
    return;
  }

  emitters = ldk_component_store_get(
      components, LDK_COMPONENT_TYPE_PARTICLE_EMITTER);
  owners = ldk_component_owners_get(
      components, LDK_COMPONENT_TYPE_PARTICLE_EMITTER);
  if (!emitters || !owners)
  {
    return;
  }

  for (u32 i = 0; i < x_array_count(emitters); ++i)
  {
    LDKParticleEmitter *emitter = x_array_get(emitters, i);
    LDKEntity *entity = x_array_get(owners, i);
    if (!emitter || !entity)
    {
      continue;
    }

    emitter->particle_count = 0;
    emitter->emission_accumulator = 0.0f;
    emitter->runtime_error_flags = 0u;

    LDKInstancedMeshSource *mesh = ldk_ecs_component_get(
        *entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
    if (mesh && !ldk_instanced_mesh_source_resize_instances(mesh, 0u))
    {
      ldk_log_error(
          "ParticleEmitter entity %u: failed to clear particle instances.\n",
          entity->index);
    }
  }
}

int ldk_particle_system_initialize(void *data)
{
  (void)data;
  s_camera_diagnostic = LDK_PARTICLE_CAMERA_DIAGNOSTIC_UNSET;
  s_dt_error_logged = false;
  s_particle_runtime_reset();
  return 0;
}

void ldk_particle_system_terminate(void *data)
{
  (void)data;
  s_particle_runtime_reset();
  s_camera_diagnostic = LDK_PARTICLE_CAMERA_DIAGNOSTIC_UNSET;
  s_dt_error_logged = false;
}

void ldk_particle_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  (void)data;
  (void)group;

  if (!isfinite(dt) || dt < 0.0f)
  {
    if (!s_dt_error_logged)
    {
      ldk_log_error("ParticleSystem received an invalid delta time.\n");
      s_dt_error_logged = true;
    }
    return;
  }
  s_dt_error_logged = false;
  if (dt == 0.0f)
  {
    return;
  }

  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  if (!components)
  {
    ldk_log_error("ParticleSystem component registry is unavailable.\n");
    return;
  }

  XArray *emitters = ldk_component_store_get(
      components, LDK_COMPONENT_TYPE_PARTICLE_EMITTER);
  XArray *owners = ldk_component_owners_get(
      components, LDK_COMPONENT_TYPE_PARTICLE_EMITTER);
  if (!emitters || !owners)
  {
    ldk_log_error("ParticleSystem emitter storage is unavailable.\n");
    return;
  }

  Mat4 camera_view = mat4_identity();
  Quat camera_rotation = quat_id();
  bool has_camera = s_main_camera_get(&camera_view, &camera_rotation);
  for (u32 i = 0; i < x_array_count(emitters); ++i)
  {
    LDKParticleEmitter *emitter = x_array_get(emitters, i);
    LDKEntity *entity = x_array_get(owners, i);
    if (!emitter || !entity)
    {
      continue;
    }

    LDKInstancedMeshSource *mesh = ldk_ecs_component_get(
        *entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
    if (!mesh)
    {
      s_emitter_error_set(emitter, *entity, LDK_PARTICLE_EMITTER_ERROR_PROXY,
          "required InstancedMeshSource component is missing.");
    }
    else
    {
      s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_PROXY);
    }

    if (!s_emitter_is_valid(emitter))
    {
      s_emitter_error_set(emitter, *entity,
          LDK_PARTICLE_EMITTER_ERROR_INVALID_CONFIG,
          "configuration is invalid.");
      emitter->particle_count = 0;
      emitter->emission_accumulator = 0.0f;
      if (mesh)
      {
        (void)s_particle_instances_clear(emitter, *entity, mesh);
      }
      continue;
    }
    s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_INVALID_CONFIG);

    Mat4 emitter_world = mat4_identity();
    if (!ldk_scenegraph_update_entity(*entity) ||
        !ldk_transform_get_world_matrix(*entity, &emitter_world))
    {
      s_emitter_error_set(emitter, *entity,
          LDK_PARTICLE_EMITTER_ERROR_TRANSFORM,
          "failed to resolve emitter transform.");
      if (mesh)
      {
        (void)s_particle_instances_clear(emitter, *entity, mesh);
      }
      continue;
    }
    s_emitter_error_clear(emitter, LDK_PARTICLE_EMITTER_ERROR_TRANSFORM);

    Vec3 translation;
    Vec3 scale;
    Quat rotation;
    mat4_decompose(emitter_world, &translation, &rotation, &scale);
    (void)translation;
    (void)scale;

    if (emitter->particle_count > emitter->max_particles)
    {
      emitter->particle_count = emitter->max_particles;
    }
    s_particle_simulate(emitter, dt);
    s_particle_emit(emitter, *entity, emitter_world, rotation, dt);

    if (!mesh)
    {
      continue;
    }
    if (!has_camera)
    {
      (void)s_particle_instances_clear(emitter, *entity, mesh);
      continue;
    }
    if (!s_particle_proxy_prepare(emitter, *entity, mesh))
    {
      (void)s_particle_instances_clear(emitter, *entity, mesh);
      continue;
    }
    if (!s_particle_instances_write(
            emitter, *entity, mesh, emitter_world, camera_rotation))
    {
      continue;
    }

    if (emitter->sort_mode == LDK_PARTICLE_SORT_BACK_TO_FRONT)
    {
      s_particle_instances_sort_back_to_front(
          mesh, emitter_world, camera_view);
    }
  }
}
