/**
 * @file ldk_trail.c
 * @brief World-space trail recording on top of LineComponent.
 */

#include <component/ldk_trail.h>

#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

LDKTrailComponent ldk_trail_component_make_default(void)
{
  LDKTrailComponent trail = {0};
  trail.enabled = true;
  trail.lifetime = 1.0f;
  trail.min_distance = 0.1f;
  trail.initial_color = 0xffffffffu;
  trail.final_color = 0xffffffffu;
  trail.initial_width = 0.0f;
  trail.final_width = 0.1f;
  trail.initial_alpha = 0.0f;
  trail.final_alpha = 1.0f;
  return trail;
}

void ldk_trail_clear(LDKTrailComponent *trail, LDKLineComponent *line)
{
  if (!trail)
  {
    return;
  }
  trail->point_count = 0u;
  if (line)
  {
    ldk_line_clear(line);
  }
}

#ifdef LDK_ENGINE
static bool s_trail_valid(const LDKTrailComponent *trail)
{
  return trail && isfinite(trail->lifetime) && trail->lifetime > 0.0f &&
      isfinite(trail->min_distance) && trail->min_distance >= 0.0f &&
      isfinite(trail->initial_width) && trail->initial_width >= 0.0f &&
      isfinite(trail->final_width) && trail->final_width >= 0.0f &&
      isfinite(trail->initial_alpha) && trail->initial_alpha >= 0.0f &&
      trail->initial_alpha <= 1.0f && isfinite(trail->final_alpha) &&
      trail->final_alpha >= 0.0f && trail->final_alpha <= 1.0f &&
      isfinite(trail->local_offset.x) &&
      isfinite(trail->local_offset.y) && isfinite(trail->local_offset.z);
}

static bool s_trail_points_reserve(
    LDKTrailComponent *trail, u32 count, u32 maximum)
{
  if (count <= trail->point_capacity)
  {
    return true;
  }
  if (count > maximum || maximum > UINT32_MAX / sizeof(*trail->points))
  {
    return false;
  }
  u32 capacity = trail->point_capacity ? trail->point_capacity : 8u;
  while (capacity < count)
  {
    if (capacity > maximum / 2u)
    {
      capacity = maximum;
      break;
    }
    capacity *= 2u;
  }
  if (capacity > maximum)
  {
    capacity = maximum;
  }
  LDKTrailPoint *points = realloc(
      trail->points, (size_t)capacity * sizeof(*trail->points));
  if (!points)
  {
    return false;
  }
  trail->points = points;
  trail->point_capacity = capacity;
  return true;
}

static float s_trail_lerp(float start, float end, float t)
{
  return start + (end - start) * t;
}

static u32 s_trail_color_lerp(u32 start, u32 end, float t)
{
  u32 color = 0u;
  for (u32 shift = 0u; shift < 32u; shift += 8u)
  {
    float a = (float)((start >> shift) & 0xffu);
    float b = (float)((end >> shift) & 0xffu);
    color |= (u32)(s_trail_lerp(a, b, t) + 0.5f) << shift;
  }
  return color;
}

static bool s_trail_line_sync(LDKTrailComponent *trail,
    LDKLineComponent *line, Mat4 inverse_world)
{
  u32 count = trail->point_count ? trail->point_count - 1u : 0u;
  /* Apply line data through its public API. Do not touch its mesh buffers. */
  for (u32 i = 0; i < count; ++i)
  {
    const LDKTrailPoint *a = &trail->points[i];
    const LDKTrailPoint *b = &trail->points[i + 1u];
    float ta = fmaxf(0.0f, 1.0f - a->age / trail->lifetime);
    float tb = fmaxf(0.0f, 1.0f - b->age / trail->lifetime);
    LDKLineSegment segment = ldk_line_segment_default();
    segment.pos_start = mat4_mul_point(inverse_world, a->position);
    segment.pos_end = mat4_mul_point(inverse_world, b->position);
    segment.initial_color = s_trail_color_lerp(
        trail->initial_color, trail->final_color, ta);
    segment.final_color = s_trail_color_lerp(
        trail->initial_color, trail->final_color, tb);
    segment.initial_width = s_trail_lerp(
        trail->initial_width, trail->final_width, ta);
    segment.final_width = s_trail_lerp(
        trail->initial_width, trail->final_width, tb);
    segment.initial_alpha = s_trail_lerp(
        trail->initial_alpha, trail->final_alpha, ta);
    segment.final_alpha = s_trail_lerp(
        trail->initial_alpha, trail->final_alpha, tb);
    if (i < line->segment_count)
    {
      if (!ldk_line_segment_set(line, i, &segment))
      {
        return false;
      }
    }
    else if (!ldk_line_segment_add(line, &segment, NULL))
    {
      return false;
    }
  }
  while (line->segment_count > count)
  {
    if (!ldk_line_segment_remove(line, line->segment_count - 1u))
    {
      return false;
    }
  }
  return true;
}

bool ldk_trail_update(LDKTrailComponent *trail,
    LDKLineComponent *line, Mat4 world, float dt)
{
  if (!trail || !line || !isfinite(dt) || dt < 0.0f)
  {
    return false;
  }
  if (!trail->enabled || !line->max_segments)
  {
    ldk_trail_clear(trail, line);
    return true;
  }
  if (!s_trail_valid(trail) || line->max_segments == UINT32_MAX)
  {
    return false;
  }
  bool inverse_ok = false;
  Mat4 inverse_world = mat4_inverse_full(world, &inverse_ok);
  if (!inverse_ok)
  {
    return false;
  }
  Vec3 position = mat4_mul_point(world, trail->local_offset);
  if (!isfinite(position.x) || !isfinite(position.y) ||
      !isfinite(position.z))
  {
    return false;
  }

  /* Age and evict points in insertion order; no time data is stored in Line. */
  for (u32 i = 0u; i < trail->point_count; ++i)
  {
    trail->points[i].age += dt;
  }
  u32 expired = 0u;
  while (expired < trail->point_count &&
      trail->points[expired].age >= trail->lifetime)
  {
    ++expired;
  }
  if (expired)
  {
    trail->point_count -= expired;
    memmove(trail->points, trail->points + expired,
        (size_t)trail->point_count * sizeof(*trail->points));
  }

  u32 capacity = line->max_segments + 1u;
  if (trail->point_count > capacity)
  {
    u32 removed = trail->point_count - capacity;
    trail->point_count = capacity;
    memmove(trail->points, trail->points + removed,
        (size_t)capacity * sizeof(*trail->points));
  }
  if (!trail->point_count)
  {
    if (!s_trail_points_reserve(trail, 1u, capacity))
    {
      return false;
    }
    trail->points[0] = (LDKTrailPoint){.position = position, .age = 0.0f};
    trail->point_count = 1u;
  }
  else
  {
    Vec3 difference = vec3_sub(position,
        trail->points[trail->point_count - 1u].position);
    float distance2 = vec3_len2(difference);
    if (distance2 > 0.0f &&
        distance2 >= trail->min_distance * trail->min_distance)
    {
      if (trail->point_count == capacity)
      {
        --trail->point_count;
        memmove(trail->points, trail->points + 1u,
            (size_t)trail->point_count * sizeof(*trail->points));
      }
      if (!s_trail_points_reserve(trail, trail->point_count + 1u, capacity))
      {
        return false;
      }
      trail->points[trail->point_count++] =
          (LDKTrailPoint){.position = position, .age = 0.0f};
    }
  }
  return s_trail_line_sync(trail, line, inverse_world);
}

void ldk_trail_update_all(float dt)
{
  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  if (!components)
  {
    return;
  }
  XArray *all = ldk_component_store_get(
      components, LDK_COMPONENT_TYPE_TRAIL);
  XArray *owners = ldk_component_owners_get(
      components, LDK_COMPONENT_TYPE_TRAIL);
  if (!all || !owners)
  {
    return;
  }
  for (u32 i = 0u; i < x_array_count(all); ++i)
  {
    LDKTrailComponent *trail = x_array_get(all, i);
    LDKEntity *entity = x_array_get(owners, i);
    if (!trail || !entity)
    {
      continue;
    }
    LDKLineComponent *line = ldk_ecs_component_get(
        *entity, LDK_COMPONENT_TYPE_LINE);
    if (!ldk_ecs_component_is_enabled(*entity, LDK_COMPONENT_TYPE_TRAIL))
    {
      ldk_trail_clear(trail, line);
      continue;
    }
    Mat4 world;
    if (!line || !ldk_ecs_component_is_enabled(*entity,
                      LDK_COMPONENT_TYPE_LINE) ||
        !ldk_scenegraph_update_entity(*entity) ||
        !ldk_transform_get_world_matrix(*entity, &world))
    {
      continue;
    }
    if (!ldk_trail_update(trail, line, world, dt))
    {
      if (!trail->error_reported)
      {
        ldk_log_error("Trail entity %u: failed to update segments.\n",
            entity->index);
        trail->error_reported = true;
      }
    }
    else
    {
      trail->error_reported = false;
    }
  }
}

static bool s_trail_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  (void)entities;
  (void)components;
  (void)entity;
  (void)index;
  (void)user;
  LDKTrailComponent *trail = component;
  if (!trail)
  {
    return false;
  }
  *trail = initial_value ? *(const LDKTrailComponent *)initial_value
      : ldk_trail_component_make_default();
  trail->points = NULL;
  trail->point_count = 0u;
  trail->point_capacity = 0u;
  trail->error_reported = false;
  return true;
}

static void s_trail_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  (void)index;
  (void)user;
  LDKTrailComponent *trail = component;
  if (trail)
  {
    /* A detached Trail must not leave its previous geometry behind. */
    LDKLineComponent *line = entities && components
        ? ldk_entity_component_get(entities, components, entity,
              LDK_COMPONENT_TYPE_LINE)
        : NULL;
    ldk_trail_clear(trail, line);
    free(trail->points);
    trail->points = NULL;
    trail->point_capacity = 0u;
  }
}

LDKComponentDesc ldk_trail_component_desc(u32 initial_capacity)
{
  static const u32 required[] = {
      LDK_COMPONENT_TYPE_TRANSFORM, LDK_COMPONENT_TYPE_LINE};
  LDKComponentDesc desc = {0};
  desc.name = "Trail";
  desc.type = LDK_COMPONENT_TYPE_TRAIL;
  desc.entry_size = sizeof(LDKTrailComponent);
  desc.initial_capacity = initial_capacity;
  desc.required_components = required;
  desc.required_component_count =
      (u32)(sizeof(required) / sizeof(required[0]));
  desc.attach = s_trail_attach;
  desc.destroy = s_trail_destroy;
  return desc;
}
#endif // LDK_ENGINE
