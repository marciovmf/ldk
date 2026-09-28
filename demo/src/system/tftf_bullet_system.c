#include "tftf_bullet_system.h"

#include <component/ldk_instanced_mesh_source.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <ldk_material.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <math.h>
#include <string.h>

typedef struct TFTFBulletPattern
{
  TFTFBulletPatternName name;
  TFTFBulletPatternType type;
  TFTFProjectileMovement movement;
  u32 count;
  float speed;
  float lifetime;
  float radius;
  float damage;
  float scale;
  float height_offset;
  float angle_offset_degrees;
  u32 color;
} TFTFBulletPattern;

static TFTFBulletSystem *s_tftf_bullet_system = NULL;

static const char *s_tftf_bullet_pattern_name(
    TFTFBulletPatternName name)
{
  switch (name)
  {
  case TFTF_BULLET_PATTERN_NAME_RING:
    return "RING";
  case TFTF_BULLET_PATTERN_NAME_RING_FAST:
    return "RING_FAST";
  case TFTF_BULLET_PATTERN_NAME_RING_DENSE:
    return "RING_DENSE";
  case TFTF_BULLET_PATTERN_NAME_RING_BIG:
    return "RING_BIG";
  case TFTF_BULLET_PATTERN_NAME_NONE:
  default:
    return NULL;
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
    pattern.lifetime = system->pattern_0_lifetime;
    pattern.radius = system->pattern_0_radius;
    pattern.damage = system->pattern_0_damage;
    pattern.scale = system->pattern_0_scale;
    pattern.height_offset = system->pattern_0_height_offset;
    pattern.angle_offset_degrees = system->pattern_0_angle_offset_degrees;
    pattern.color = system->pattern_0_color;
    break;
  case 1u:
    pattern.name = system->pattern_1_name;
    pattern.type = system->pattern_1_type;
    pattern.movement = system->pattern_1_movement;
    pattern.count = system->pattern_1_count;
    pattern.speed = system->pattern_1_speed;
    pattern.lifetime = system->pattern_1_lifetime;
    pattern.radius = system->pattern_1_radius;
    pattern.damage = system->pattern_1_damage;
    pattern.scale = system->pattern_1_scale;
    pattern.height_offset = system->pattern_1_height_offset;
    pattern.angle_offset_degrees = system->pattern_1_angle_offset_degrees;
    pattern.color = system->pattern_1_color;
    break;
  case 2u:
    pattern.name = system->pattern_2_name;
    pattern.type = system->pattern_2_type;
    pattern.movement = system->pattern_2_movement;
    pattern.count = system->pattern_2_count;
    pattern.speed = system->pattern_2_speed;
    pattern.lifetime = system->pattern_2_lifetime;
    pattern.radius = system->pattern_2_radius;
    pattern.damage = system->pattern_2_damage;
    pattern.scale = system->pattern_2_scale;
    pattern.height_offset = system->pattern_2_height_offset;
    pattern.angle_offset_degrees = system->pattern_2_angle_offset_degrees;
    pattern.color = system->pattern_2_color;
    break;
  case 3u:
    pattern.name = system->pattern_3_name;
    pattern.type = system->pattern_3_type;
    pattern.movement = system->pattern_3_movement;
    pattern.count = system->pattern_3_count;
    pattern.speed = system->pattern_3_speed;
    pattern.lifetime = system->pattern_3_lifetime;
    pattern.radius = system->pattern_3_radius;
    pattern.damage = system->pattern_3_damage;
    pattern.scale = system->pattern_3_scale;
    pattern.height_offset = system->pattern_3_height_offset;
    pattern.angle_offset_degrees = system->pattern_3_angle_offset_degrees;
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

  if (!s_tftf_bullet_pattern_name(pattern->name) ||
      pattern->type != TFTF_BULLET_PATTERN_TYPE_RING ||
      pattern->movement != TFTF_PROJECTILE_MOVEMENT_LINEAR ||
      pattern->count == 0u || pattern->count > 4096u ||
      !isfinite(pattern->speed) || pattern->speed < 0.0f ||
      !isfinite(pattern->lifetime) || pattern->lifetime <= 0.0f ||
      !isfinite(pattern->radius) || pattern->radius < 0.0f ||
      !isfinite(pattern->damage) || pattern->damage < 0.0f ||
      !isfinite(pattern->scale) || pattern->scale <= 0.0f ||
      !isfinite(pattern->height_offset) ||
      !isfinite(pattern->angle_offset_degrees))
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

static bool s_tftf_bullet_pattern_find(
    const TFTFBulletSystem *system, const char *name,
    TFTFBulletPattern *out_pattern)
{
  if (!system || !name || !name[0] || !out_pattern)
  {
    return false;
  }

  for (u32 i = 0u; i < TFTF_BULLET_PATTERN_SLOT_COUNT; ++i)
  {
    TFTFBulletPattern pattern;
    const char *pattern_name;

    if (!s_tftf_bullet_pattern_at(system, i, &pattern))
    {
      return false;
    }

    pattern_name = s_tftf_bullet_pattern_name(pattern.name);
    if (pattern_name && strcmp(pattern_name, name) == 0)
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
    mesh = ldk_mesh_primitive_asset_get(assets, LDK_MESH_PRIMITIVE_SPHERE);
  }
  if (x_handle_is_null(mesh.h) ||
      !ldk_mesh_source_set_data(&source.source, mesh))
  {
    return false;
  }

  source.source.casts_shadows = false;
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
    const TFTFBulletPattern *pattern, Vec3 position, Vec3 velocity)
{
  TFTFProjectileComponent projectile = {0};
  LDKEntity entity;

  if (!pattern)
  {
    return false;
  }

  projectile.flags = TFTF_PROJECTILE_FLAG_NONE;
  projectile.movement = pattern->movement;
  projectile.previous_position = position;
  projectile.position = position;
  projectile.velocity = velocity;
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
    Vec3 velocity = vec3_mul(direction, pattern->speed);

    if (!s_tftf_bullet_spawn(pattern, origin, velocity))
    {
      return false;
    }
  }

  return true;
}

bool tftf_burst_by_name(const char *name, Vec3 origin)
{
  TFTFBulletPattern pattern;

  if (!s_tftf_bullet_system || !s_tftf_bullet_pattern_find(
                                  s_tftf_bullet_system, name, &pattern) ||
      !s_tftf_bullet_pattern_valid(&pattern))
  {
    return false;
  }

  switch (pattern.type)
  {
  case TFTF_BULLET_PATTERN_TYPE_RING:
    return s_tftf_burst_ring(&pattern, origin);
  case TFTF_BULLET_PATTERN_TYPE_NONE:
  default:
    return false;
  }
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

    switch (projectile->movement)
    {
    case TFTF_PROJECTILE_MOVEMENT_LINEAR:
      projectile->position = vec3_add(
          projectile->position, vec3_mul(projectile->velocity, dt));
      break;
    default:
      ldk_ecs_entity_destroy(entity);
      continue;
    }

    if (!isfinite(projectile->position.x) ||
        !isfinite(projectile->position.y) ||
        !isfinite(projectile->position.z) || !isfinite(projectile->scale) ||
        projectile->scale <= 0.0f)
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
