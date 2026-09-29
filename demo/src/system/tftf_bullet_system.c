#include "tftf_bullet_system.h"

#include <component/ldk_instanced_mesh_source.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <ldk_material.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <math.h>

typedef struct TFTFBulletPattern
{
  TFTFBulletPatternName name;
  TFTFBulletPatternType type;
  TFTFProjectileMovement movement;
  u32 count;
  float speed;
  float acceleration;
  float lifetime;
  float radius;
  float damage;
  float scale;
  float height_offset;
  float angle_offset_degrees;
  float spread_degrees;
  float sine_amplitude;
  float sine_frequency;
  float sine_phase_degrees;
  u32 color;
} TFTFBulletPattern;

static TFTFBulletSystem *s_tftf_bullet_system = NULL;

static bool s_tftf_bullet_pattern_name_valid(TFTFBulletPatternName name)
{
  switch (name)
  {
  case TFTF_BULLET_PATTERN_NAME_RING:
  case TFTF_BULLET_PATTERN_NAME_RING_FAST:
  case TFTF_BULLET_PATTERN_NAME_RING_DENSE:
  case TFTF_BULLET_PATTERN_NAME_RING_BIG:
  case TFTF_BULLET_PATTERN_NAME_FAN:
    return true;
  case TFTF_BULLET_PATTERN_NAME_NONE:
  default:
    return false;
  }
}

static bool s_tftf_bullet_pattern_at(const TFTFBulletSystem *system,
    u32 index, TFTFBulletPattern *out_pattern)
{
  TFTFBulletPattern pattern = {0};

  if (!system || !out_pattern || index >= TFTF_BULLET_PATTERN_SLOT_COUNT)
  {
    return false;
  }

  switch (index)
  {
  case 0u:
    pattern.name = system->pattern_0_name;
    pattern.type = system->pattern_0_type;
    pattern.movement = system->pattern_0_movement;
    pattern.count = system->pattern_0_count;
    pattern.speed = system->pattern_0_speed;
    pattern.acceleration = system->pattern_0_acceleration;
    pattern.lifetime = system->pattern_0_lifetime;
    pattern.radius = system->pattern_0_radius;
    pattern.damage = system->pattern_0_damage;
    pattern.scale = system->pattern_0_scale;
    pattern.height_offset = system->pattern_0_height_offset;
    pattern.angle_offset_degrees = system->pattern_0_angle_offset_degrees;
    pattern.spread_degrees = system->pattern_0_spread_degrees;
    pattern.sine_amplitude = system->pattern_0_sine_amplitude;
    pattern.sine_frequency = system->pattern_0_sine_frequency;
    pattern.sine_phase_degrees = system->pattern_0_sine_phase_degrees;
    pattern.color = system->pattern_0_color;
    break;
  case 1u:
    pattern.name = system->pattern_1_name;
    pattern.type = system->pattern_1_type;
    pattern.movement = system->pattern_1_movement;
    pattern.count = system->pattern_1_count;
    pattern.speed = system->pattern_1_speed;
    pattern.acceleration = system->pattern_1_acceleration;
    pattern.lifetime = system->pattern_1_lifetime;
    pattern.radius = system->pattern_1_radius;
    pattern.damage = system->pattern_1_damage;
    pattern.scale = system->pattern_1_scale;
    pattern.height_offset = system->pattern_1_height_offset;
    pattern.angle_offset_degrees = system->pattern_1_angle_offset_degrees;
    pattern.spread_degrees = system->pattern_1_spread_degrees;
    pattern.sine_amplitude = system->pattern_1_sine_amplitude;
    pattern.sine_frequency = system->pattern_1_sine_frequency;
    pattern.sine_phase_degrees = system->pattern_1_sine_phase_degrees;
    pattern.color = system->pattern_1_color;
    break;
  case 2u:
    pattern.name = system->pattern_2_name;
    pattern.type = system->pattern_2_type;
    pattern.movement = system->pattern_2_movement;
    pattern.count = system->pattern_2_count;
    pattern.speed = system->pattern_2_speed;
    pattern.acceleration = system->pattern_2_acceleration;
    pattern.lifetime = system->pattern_2_lifetime;
    pattern.radius = system->pattern_2_radius;
    pattern.damage = system->pattern_2_damage;
    pattern.scale = system->pattern_2_scale;
    pattern.height_offset = system->pattern_2_height_offset;
    pattern.angle_offset_degrees = system->pattern_2_angle_offset_degrees;
    pattern.spread_degrees = system->pattern_2_spread_degrees;
    pattern.sine_amplitude = system->pattern_2_sine_amplitude;
    pattern.sine_frequency = system->pattern_2_sine_frequency;
    pattern.sine_phase_degrees = system->pattern_2_sine_phase_degrees;
    pattern.color = system->pattern_2_color;
    break;
  case 3u:
    pattern.name = system->pattern_3_name;
    pattern.type = system->pattern_3_type;
    pattern.movement = system->pattern_3_movement;
    pattern.count = system->pattern_3_count;
    pattern.speed = system->pattern_3_speed;
    pattern.acceleration = system->pattern_3_acceleration;
    pattern.lifetime = system->pattern_3_lifetime;
    pattern.radius = system->pattern_3_radius;
    pattern.damage = system->pattern_3_damage;
    pattern.scale = system->pattern_3_scale;
    pattern.height_offset = system->pattern_3_height_offset;
    pattern.angle_offset_degrees = system->pattern_3_angle_offset_degrees;
    pattern.spread_degrees = system->pattern_3_spread_degrees;
    pattern.sine_amplitude = system->pattern_3_sine_amplitude;
    pattern.sine_frequency = system->pattern_3_sine_frequency;
    pattern.sine_phase_degrees = system->pattern_3_sine_phase_degrees;
    pattern.color = system->pattern_3_color;
    break;
  default:
    return false;
  }

  *out_pattern = pattern;
  return true;
}

static bool s_tftf_bullet_pattern_valid(const TFTFBulletPattern *pattern)
{
  bool empty;
  bool type_valid;
  bool movement_valid;

  if (!pattern)
  {
    return false;
  }

  empty = pattern->name == TFTF_BULLET_PATTERN_NAME_NONE &&
      pattern->type == TFTF_BULLET_PATTERN_TYPE_NONE;
  if (empty)
  {
    return true;
  }

  type_valid = pattern->type == TFTF_BULLET_PATTERN_TYPE_RING ||
      pattern->type == TFTF_BULLET_PATTERN_TYPE_FAN;
  movement_valid = pattern->movement == TFTF_PROJECTILE_MOVEMENT_LINEAR ||
      pattern->movement == TFTF_PROJECTILE_MOVEMENT_SINE;

  if (!s_tftf_bullet_pattern_name_valid(pattern->name) || !type_valid ||
      !movement_valid || pattern->count == 0u || pattern->count > 4096u ||
      !isfinite(pattern->speed) || pattern->speed < 0.0f ||
      !isfinite(pattern->acceleration) || !isfinite(pattern->lifetime) ||
      pattern->lifetime <= 0.0f || !isfinite(pattern->radius) ||
      pattern->radius < 0.0f || !isfinite(pattern->damage) ||
      pattern->damage < 0.0f || !isfinite(pattern->scale) ||
      pattern->scale <= 0.0f || !isfinite(pattern->height_offset) ||
      !isfinite(pattern->angle_offset_degrees) ||
      !isfinite(pattern->spread_degrees) || pattern->spread_degrees < 0.0f ||
      pattern->spread_degrees > 360.0f || !isfinite(pattern->sine_amplitude) ||
      pattern->sine_amplitude < 0.0f || !isfinite(pattern->sine_frequency) ||
      pattern->sine_frequency < 0.0f ||
      !isfinite(pattern->sine_phase_degrees))
  {
    return false;
  }

  return true;
}

static bool s_tftf_bullet_patterns_validate(const TFTFBulletSystem *system)
{
  for (u32 i = 0u; i < TFTF_BULLET_PATTERN_SLOT_COUNT; ++i)
  {
    TFTFBulletPattern left;

    if (!s_tftf_bullet_pattern_at(system, i, &left) ||
        !s_tftf_bullet_pattern_valid(&left))
    {
      return false;
    }

    if (left.name == TFTF_BULLET_PATTERN_NAME_NONE)
    {
      continue;
    }

    for (u32 j = i + 1u; j < TFTF_BULLET_PATTERN_SLOT_COUNT; ++j)
    {
      TFTFBulletPattern right;

      if (!s_tftf_bullet_pattern_at(system, j, &right))
      {
        return false;
      }
      if (left.name == right.name)
      {
        return false;
      }
    }
  }

  return true;
}

static bool s_tftf_bullet_pattern_find(const TFTFBulletSystem *system,
    TFTFBulletPatternName name, TFTFBulletPattern *out_pattern)
{
  if (!system || !s_tftf_bullet_pattern_name_valid(name) || !out_pattern)
  {
    return false;
  }

  for (u32 i = 0u; i < TFTF_BULLET_PATTERN_SLOT_COUNT; ++i)
  {
    TFTFBulletPattern pattern;

    if (!s_tftf_bullet_pattern_at(system, i, &pattern))
    {
      return false;
    }

    if (pattern.name == name)
    {
      *out_pattern = pattern;
      return true;
    }
  }

  return false;
}

static bool s_tftf_bullet_render_proxy_create(TFTFBulletSystem *system)
{
  LDKAssetManager *assets;
  LDKAssetMesh mesh;
  LDKMaterialDesc material;
  LDKInstancedMeshSource source = {0};
  LDKEntity entity;

  if (!system)
  {
    return false;
  }

  assets = (LDKAssetManager *)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets)
  {
    return false;
  }

  mesh = system->projectile_mesh;
  if (x_handle_is_null(mesh.h))
  {
    mesh = ldk_mesh_primitive_asset_get(assets, LDK_MESH_PRIMITIVE_QUAD);
  }
  if (x_handle_is_null(mesh.h) ||
      !ldk_mesh_source_set_data(&source.source, mesh))
  {
    return false;
  }

  source.source.casts_shadows = false;
  source.source.billboard = true;
  if (!x_handle_is_null(system->projectile_material.h))
  {
    if (!ldk_mesh_source_set_material_asset(
            &source.source, assets, system->projectile_material))
    {
      return false;
    }
  }
  else
  {
    if (!ldk_material_desc_defaults(
            LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, &material) ||
        !ldk_mesh_source_set_material(&source.source, &material))
    {
      return false;
    }
  }

  entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return false;
  }

  if (!ldk_ecs_component_add(
          entity, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE, &source))
  {
    ldk_ecs_entity_destroy(entity);
    return false;
  }

  (void)ldk_ecs_entity_name_set(entity, "TFTF Bullet Render Proxy");
  system->render_proxy = entity;
  return true;
}

static bool s_tftf_bullet_spawn(
    const TFTFBulletPattern *pattern, Vec3 position, Vec3 direction)
{
  TFTFProjectileComponent projectile = {0};
  LDKEntity entity;

  if (!pattern || !isfinite(direction.x) || !isfinite(direction.y) ||
      !isfinite(direction.z))
  {
    return false;
  }

  direction.y = 0.0f;
  if (vec3_len2(direction) <= 1.0e-8f)
  {
    return false;
  }
  direction = vec3_norm(direction);

  projectile.flags = TFTF_PROJECTILE_FLAG_NONE;
  projectile.movement = pattern->movement;
  projectile.previous_position = position;
  projectile.position = position;
  projectile.velocity = vec3_mul(direction, pattern->speed);
  projectile.spawn_position = position;
  projectile.forward = direction;
  projectile.side = vec3_make(-direction.z, 0.0f, direction.x);
  projectile.initial_speed = pattern->speed;
  projectile.acceleration = pattern->acceleration;
  projectile.sine_amplitude = pattern->sine_amplitude;
  projectile.sine_frequency = pattern->sine_frequency;
  projectile.sine_phase_degrees = pattern->sine_phase_degrees;
  projectile.age = 0.0f;
  projectile.lifetime = pattern->lifetime;
  projectile.radius = pattern->radius;
  projectile.damage = pattern->damage;
  projectile.scale = pattern->scale;
  projectile.color = pattern->color;

  entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return false;
  }

  if (!ldk_ecs_component_add(
          entity, ldk_component_type(TFTFProjectileComponent), &projectile))
  {
    ldk_ecs_entity_destroy(entity);
    return false;
  }

  return true;
}

static bool s_tftf_burst_ring(
    const TFTFBulletPattern *pattern, Vec3 origin)
{
  float first_angle;
  float step;

  if (!pattern || pattern->count == 0u)
  {
    return false;
  }

  origin.y += pattern->height_offset;
  first_angle = deg_to_rad(pattern->angle_offset_degrees);
  step = (2.0f * STDXM_PI) / (float)pattern->count;

  for (u32 i = 0u; i < pattern->count; ++i)
  {
    float angle = first_angle + step * (float)i;
    Vec3 direction = vec3_make(cosf(angle), 0.0f, sinf(angle));

    if (!s_tftf_bullet_spawn(pattern, origin, direction))
    {
      return false;
    }
  }

  return true;
}

static bool s_tftf_burst_fan(
    const TFTFBulletPattern *pattern, Vec3 origin, Vec3 direction)
{
  float center_angle;
  float first_angle;
  float step;

  if (!pattern || pattern->count == 0u || !isfinite(direction.x) ||
      !isfinite(direction.z))
  {
    return false;
  }

  direction.y = 0.0f;
  if (vec3_len2(direction) <= 1.0e-8f)
  {
    return false;
  }

  origin.y += pattern->height_offset;
  direction = vec3_norm(direction);
  center_angle = atan2f(direction.z, direction.x) +
      deg_to_rad(pattern->angle_offset_degrees);

  if (pattern->count == 1u)
  {
    first_angle = center_angle;
    step = 0.0f;
  }
  else
  {
    float spread = deg_to_rad(pattern->spread_degrees);
    first_angle = center_angle - spread * 0.5f;
    step = spread / (float)(pattern->count - 1u);
  }

  for (u32 i = 0u; i < pattern->count; ++i)
  {
    float angle = first_angle + step * (float)i;
    Vec3 bullet_direction = vec3_make(cosf(angle), 0.0f, sinf(angle));

    if (!s_tftf_bullet_spawn(pattern, origin, bullet_direction))
    {
      return false;
    }
  }

  return true;
}

bool tftf_burst(
    TFTFBulletPatternName name, Vec3 origin, Vec3 direction)
{
  TFTFBulletPattern pattern;

  if (!s_tftf_bullet_system ||
      !s_tftf_bullet_pattern_find(s_tftf_bullet_system, name, &pattern) ||
      !s_tftf_bullet_pattern_valid(&pattern))
  {
    return false;
  }

  switch (pattern.type)
  {
  case TFTF_BULLET_PATTERN_TYPE_RING:
    return s_tftf_burst_ring(&pattern, origin);
  case TFTF_BULLET_PATTERN_TYPE_FAN:
    return s_tftf_burst_fan(&pattern, origin, direction);
  case TFTF_BULLET_PATTERN_TYPE_NONE:
  default:
    return false;
  }
}

static bool s_tftf_projectile_update_position(
    TFTFProjectileComponent *projectile)
{
  float distance;
  float speed;
  Vec3 position;
  Vec3 velocity;

  if (!projectile || !isfinite(projectile->age) ||
      !isfinite(projectile->initial_speed) ||
      !isfinite(projectile->acceleration))
  {
    return false;
  }

  distance = projectile->initial_speed * projectile->age +
      0.5f * projectile->acceleration * projectile->age * projectile->age;
  speed = projectile->initial_speed +
      projectile->acceleration * projectile->age;
  position = vec3_add(projectile->spawn_position,
      vec3_mul(projectile->forward, distance));
  velocity = vec3_mul(projectile->forward, speed);

  switch (projectile->movement)
  {
  case TFTF_PROJECTILE_MOVEMENT_LINEAR:
    break;

  case TFTF_PROJECTILE_MOVEMENT_SINE:
  {
    float angular_frequency;
    float phase;
    float initial_phase;
    float lateral_offset;
    float lateral_speed;

    if (!isfinite(projectile->sine_amplitude) ||
        projectile->sine_amplitude < 0.0f ||
        !isfinite(projectile->sine_frequency) ||
        projectile->sine_frequency < 0.0f ||
        !isfinite(projectile->sine_phase_degrees))
    {
      return false;
    }

    angular_frequency = 2.0f * STDXM_PI * projectile->sine_frequency;
    initial_phase = deg_to_rad(projectile->sine_phase_degrees);
    phase = initial_phase + angular_frequency * projectile->age;
    lateral_offset = projectile->sine_amplitude *
        (sinf(phase) - sinf(initial_phase));
    lateral_speed = projectile->sine_amplitude * angular_frequency *
        cosf(phase);

    position = vec3_add(
        position, vec3_mul(projectile->side, lateral_offset));
    velocity = vec3_add(
        velocity, vec3_mul(projectile->side, lateral_speed));
  }
  break;

  default:
    return false;
  }

  if (!isfinite(position.x) || !isfinite(position.y) ||
      !isfinite(position.z) || !isfinite(velocity.x) ||
      !isfinite(velocity.y) || !isfinite(velocity.z))
  {
    return false;
  }

  projectile->position = position;
  projectile->velocity = velocity;
  return true;
}

int tftf_bullet_system_initialize(void *data)
{
  TFTFBulletSystem *system = (TFTFBulletSystem *)data;

  if (!system || s_tftf_bullet_system ||
      !s_tftf_bullet_patterns_validate(system))
  {
    ldk_log_error("TFTF BulletSystem has invalid pattern configuration.\n");
    return -1;
  }

  system->render_proxy = x_handle_null();
  if (!s_tftf_bullet_render_proxy_create(system))
  {
    ldk_log_error("TFTF BulletSystem failed to create render proxy.\n");
    return -1;
  }

  s_tftf_bullet_system = system;
  return 0;
}

void tftf_bullet_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  TFTFBulletSystem *system = (TFTFBulletSystem *)data;
  LDKInstancedMeshSource *proxy;
  u32 projectile_type = ldk_component_type(TFTFProjectileComponent);
  u32 write_count = 0u;

  if (!system || !group || !isfinite(dt) || dt < 0.0f ||
      x_handle_is_null(system->render_proxy))
  {
    return;
  }

  proxy = (LDKInstancedMeshSource *)ldk_ecs_component_get(
      system->render_proxy, LDK_COMPONENT_TYPE_INSTANCED_MESH_SOURCE);
  if (!proxy ||
      !ldk_instanced_mesh_source_resize_instances(proxy, group->count))
  {
    return;
  }

  for (u32 i = 0u; i < group->count; ++i)
  {
    LDKEntity entity = group->entities[i];
    TFTFProjectileComponent *projectile =
        (TFTFProjectileComponent *)ldk_ecs_component_get(
            entity, projectile_type);

    if (!projectile)
    {
      continue;
    }

    projectile->previous_position = projectile->position;
    projectile->age += dt;
    if (!isfinite(projectile->age) || projectile->age >= projectile->lifetime)
    {
      ldk_ecs_entity_destroy(entity);
      continue;
    }

    if (!s_tftf_projectile_update_position(projectile) ||
        !isfinite(projectile->scale) || projectile->scale <= 0.0f)
    {
      ldk_ecs_entity_destroy(entity);
      continue;
    }

    proxy->instances[write_count] = mat4_compose(projectile->position,
        quat_id(), vec3_make(projectile->scale, projectile->scale,
                       projectile->scale));
    proxy->instance_colors[write_count] = projectile->color;
    ++write_count;
  }

  if (write_count != proxy->instance_count)
  {
    (void)ldk_instanced_mesh_source_resize_instances(proxy, write_count);
  }
}

void tftf_bullet_system_terminate(void *data)
{
  TFTFBulletSystem *system = (TFTFBulletSystem *)data;

  if (!system)
  {
    return;
  }

  if (!x_handle_is_null(system->render_proxy))
  {
    ldk_ecs_entity_destroy(system->render_proxy);
    system->render_proxy = x_handle_null();
  }

  if (s_tftf_bullet_system == system)
  {
    s_tftf_bullet_system = NULL;
  }
}
