#include <system/ldk_grass.h>

#include <ldk_mesh.h>
#include <module/ldk_renderer.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LDK_GRASS_PI 3.14159265358979323846f
#define LDK_GRASS_INVALID_BATCH UINT32_MAX
#define LDK_GRASS_RIBBON_VERTEX_COUNT                                        \
  (LDK_GRASS_PROFILE_POINT_COUNT * 2u)
#define LDK_GRASS_THORN_COUNT 6u
#define LDK_GRASS_THORN_VERTEX_COUNT 4u
#define LDK_GRASS_THORN_INDEX_COUNT 9u
#define LDK_GRASS_MAX_MESH_VERTEX_COUNT                                      \
  (LDK_GRASS_PROFILE_POINT_COUNT * LDK_GRASS_MAX_SIDE_COUNT +               \
      LDK_GRASS_MAX_SIDE_COUNT + 1u +                                       \
      LDK_GRASS_THORN_COUNT * LDK_GRASS_THORN_VERTEX_COUNT)
#define LDK_GRASS_MAX_MESH_INDEX_COUNT                                       \
  ((LDK_GRASS_PROFILE_POINT_COUNT - 1u) * LDK_GRASS_MAX_SIDE_COUNT * 6u +   \
      LDK_GRASS_MAX_SIDE_COUNT * 3u +                                       \
      LDK_GRASS_THORN_COUNT * LDK_GRASS_THORN_INDEX_COUNT)
#define LDK_GRASS_THORN_MAX_LENGTH_SCALE 0.75f
#define LDK_GRASS_THORNY_BEND_MAX_SCALE 1.25f

typedef struct LDKGrassPatchEntry
{
  LDKGrassPatchDesc desc;
  Mat4 *instances;
  u32 instance_count;
  u32 instance_offset;
  u32 batch_index;
  u32 version;
  Vec3 bounds_min;
  Vec3 bounds_max;
  bool alive;
} LDKGrassPatchEntry;

typedef struct LDKGrassBatch
{
  LDKGrassBladeShape shape;
  rgba32 bottom_color;
  rgba32 top_color;
  float specular;
  float shininess;
  float emission;
  float curvature;
  float wind_strength;
  float wind_speed;
  bool casts_shadows;

  Mat4 *instances;
  u32 instance_count;
  u32 instance_capacity;
  u32 index_count;
  LDKResourceMesh mesh;
  LDKResourceMaterial material;
  LDKResourceInstanceSet instance_set;
  u32 submit_offset;
  u32 submit_count;
  bool submit_ready;
  bool dirty;
} LDKGrassBatch;

typedef struct LDKGrassRuntime
{
  LDKRenderer *renderer;
  LDKGrassPatchEntry *patches;
  u32 patch_count;
  u32 patch_capacity;
  LDKGrassBatch *batches;
  u32 batch_count;
  u32 batch_capacity;
  bool initialized;
} LDKGrassRuntime;

static LDKGrassRuntime s_grass;

static bool s_allocation_size_is_valid(u32 count, size_t element_size)
{
  return element_size != 0u &&
         (uintmax_t)count <= (uintmax_t)SIZE_MAX / (uintmax_t)element_size;
}

static bool s_vec3_is_finite(Vec3 value)
{
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static float s_shininess_resolve(float shininess)
{
  return shininess == 0.0f ? 32.0f : shininess;
}

static u32 s_random_next(u32 *state)
{
  u32 value = *state;

  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  *state = value ? value : 0x6d2b79f5u;
  return *state;
}

static float s_random_unit(u32 *state)
{
  return (float)(s_random_next(state) >> 8u) / 16777215.0f;
}

static u32 s_random_state(u32 seed)
{
  u32 value = seed + 0x9e3779b9u;

  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value ? value : 0x6d2b79f5u;
}

static bool s_topology_is_valid(LDKGrassBladeTopology topology)
{
  return topology == LDK_GRASS_BLADE_TOPOLOGY_RIBBON ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM;
}

static bool s_shape_is_valid(const LDKGrassBladeShape *shape)
{
  if (!shape || !s_topology_is_valid(shape->topology) ||
      ((shape->topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL ||
           shape->topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM) &&
          (shape->side_count < LDK_GRASS_MIN_SIDE_COUNT ||
              shape->side_count > LDK_GRASS_MAX_SIDE_COUNT)))
  {
    return false;
  }

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    const LDKGrassProfilePoint *point = &shape->profile[i];

    if (!isfinite(point->height) || !isfinite(point->width) ||
        point->height < 0.0f || point->height > 1.0f ||
        point->width < 0.0f ||
        (i + 1u < LDK_GRASS_PROFILE_POINT_COUNT && point->width <= 0.0f))
    {
      return false;
    }

    if (i > 0u && point->height <= shape->profile[i - 1u].height)
    {
      return false;
    }
  }

  return shape->profile[0].height == 0.0f &&
         shape->profile[0].width > 0.0f &&
         shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].height == 1.0f;
}

static bool s_desc_is_valid(
    const LDKGrassPatchDesc *desc, float *out_area, Vec3 *out_normal)
{
  Vec3 cross;
  float area;
  double expected_count;

  if (!desc || !s_vec3_is_finite(desc->plane.origin) ||
      !s_vec3_is_finite(desc->plane.axis_u) ||
      !s_vec3_is_finite(desc->plane.axis_v) ||
      !isfinite(desc->density) || desc->density < 0.0f ||
      !isfinite(desc->min_height) || desc->min_height <= 0.0f ||
      !isfinite(desc->max_height) ||
      desc->max_height < desc->min_height ||
      !isfinite(desc->min_tilt) || desc->min_tilt < 0.0f ||
      !isfinite(desc->max_tilt) || desc->max_tilt < desc->min_tilt ||
      desc->max_tilt >= LDK_GRASS_PI * 0.5f ||
      !isfinite(desc->tilt_direction) ||
      !isfinite(desc->specular) || desc->specular < 0.0f ||
      !isfinite(desc->shininess) || desc->shininess < 0.0f ||
      !isfinite(desc->emission) || desc->emission < 0.0f ||
      !isfinite(desc->curvature) || desc->curvature < 0.0f ||
      !isfinite(desc->wind_strength) || desc->wind_strength < 0.0f ||
      !isfinite(desc->wind_speed) || desc->wind_speed < 0.0f ||
      !s_shape_is_valid(&desc->shape))
  {
    return false;
  }

  cross = vec3_cross(desc->plane.axis_u, desc->plane.axis_v);
  area = vec3_len(cross);
  expected_count = (double)area * (double)desc->density;
  if (!isfinite(area) || area <= STDXM_EPS ||
      expected_count > (double)UINT32_MAX)
  {
    return false;
  }

  if (out_area)
  {
    *out_area = area;
  }
  if (out_normal)
  {
    *out_normal = vec3_div(cross, area);
  }
  return true;
}

static void s_bounds_add_point(Vec3 *bounds_min, Vec3 *bounds_max, Vec3 point)
{
  bounds_min->x = float_min(bounds_min->x, point.x);
  bounds_min->y = float_min(bounds_min->y, point.y);
  bounds_min->z = float_min(bounds_min->z, point.z);
  bounds_max->x = float_max(bounds_max->x, point.x);
  bounds_max->y = float_max(bounds_max->y, point.y);
  bounds_max->z = float_max(bounds_max->z, point.z);
}

static void s_patch_bounds_calculate(const LDKGrassPatchDesc *desc,
    const Mat4 *instances, u32 instance_count,
    Vec3 *out_bounds_min, Vec3 *out_bounds_max)
{
  Vec3 bounds_min = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
  Vec3 bounds_max = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  float half_width = 0.0f;
  float bend_extent;

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    half_width = float_max(
        half_width, desc->shape.profile[i].width * 0.5f);
  }
  if (desc->shape.topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM)
  {
    half_width += desc->shape.profile[0].width *
        (LDK_GRASS_THORNY_BEND_MAX_SCALE +
            LDK_GRASS_THORN_MAX_LENGTH_SCALE);
  }

  for (u32 i = 0u; i < instance_count; ++i)
  {
    for (u32 corner = 0u; corner < 8u; ++corner)
    {
      Vec3 local = vec3_make((corner & 1u) ? half_width : -half_width,
          (corner & 2u) ? 1.0f : 0.0f,
          (corner & 4u) ? half_width : -half_width);
      s_bounds_add_point(&bounds_min, &bounds_max,
          mat4_mul_point(instances[i], local));
    }
  }

  if (instance_count == 0u)
  {
    Vec3 corners[4];

    corners[0] = desc->plane.origin;
    corners[1] = vec3_add(desc->plane.origin, desc->plane.axis_u);
    corners[2] = vec3_add(desc->plane.origin, desc->plane.axis_v);
    corners[3] = vec3_add(corners[1], desc->plane.axis_v);
    for (u32 i = 0u; i < 4u; ++i)
    {
      s_bounds_add_point(&bounds_min, &bounds_max, corners[i]);
    }
  }

  bend_extent = desc->max_height *
      (desc->curvature + desc->wind_strength);
  bounds_min = vec3_sub(bounds_min,
      vec3_make(bend_extent, bend_extent, bend_extent));
  bounds_max = vec3_add(bounds_max,
      vec3_make(bend_extent, bend_extent, bend_extent));
  *out_bounds_min = bounds_min;
  *out_bounds_max = bounds_max;
}

static bool s_shape_equal(
    const LDKGrassBladeShape *a, const LDKGrassBladeShape *b)
{
  if (!a || !b || a->topology != b->topology ||
      (a->topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL &&
          a->side_count != b->side_count))
  {
    return false;
  }

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    if (a->profile[i].height != b->profile[i].height ||
        a->profile[i].width != b->profile[i].width)
    {
      return false;
    }
  }
  return true;
}

static bool s_batch_compatible(
    const LDKGrassBatch *batch, const LDKGrassPatchDesc *desc)
{
  return batch && desc &&
         s_shape_equal(&batch->shape, &desc->shape) &&
         batch->bottom_color == desc->bottom_color &&
         batch->top_color == desc->top_color &&
         batch->specular == desc->specular &&
         batch->shininess == s_shininess_resolve(desc->shininess) &&
         batch->emission == desc->emission &&
         batch->curvature == desc->curvature &&
         batch->wind_strength == desc->wind_strength &&
         batch->wind_speed == desc->wind_speed &&
         batch->casts_shadows == desc->casts_shadows;
}

static bool s_patches_reserve(u32 required)
{
  LDKGrassPatchEntry *patches;
  u32 capacity;

  if (required <= s_grass.patch_capacity)
  {
    return true;
  }

  capacity = s_grass.patch_capacity ? s_grass.patch_capacity : 64u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*patches)))
  {
    return false;
  }

  patches = (LDKGrassPatchEntry *)realloc(
      s_grass.patches, (size_t)capacity * sizeof(*patches));
  if (!patches)
  {
    return false;
  }

  memset(patches + s_grass.patch_capacity, 0,
      (size_t)(capacity - s_grass.patch_capacity) * sizeof(*patches));
  s_grass.patches = patches;
  s_grass.patch_capacity = capacity;
  return true;
}

static bool s_batches_reserve(u32 required)
{
  LDKGrassBatch *batches;
  u32 capacity;

  if (required <= s_grass.batch_capacity)
  {
    return true;
  }

  capacity = s_grass.batch_capacity ? s_grass.batch_capacity : 8u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*batches)))
  {
    return false;
  }

  batches = (LDKGrassBatch *)realloc(
      s_grass.batches, (size_t)capacity * sizeof(*batches));
  if (!batches)
  {
    return false;
  }

  memset(batches + s_grass.batch_capacity, 0,
      (size_t)(capacity - s_grass.batch_capacity) * sizeof(*batches));
  s_grass.batches = batches;
  s_grass.batch_capacity = capacity;
  return true;
}

static u32 s_batch_find_or_add(const LDKGrassPatchDesc *desc)
{
  LDKGrassBatch *batch;

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    if (s_batch_compatible(&s_grass.batches[i], desc))
    {
      return i;
    }
  }

  if (!s_batches_reserve(s_grass.batch_count + 1u))
  {
    return LDK_GRASS_INVALID_BATCH;
  }

  batch = &s_grass.batches[s_grass.batch_count];
  memset(batch, 0, sizeof(*batch));
  batch->shape = desc->shape;
  batch->bottom_color = desc->bottom_color;
  batch->top_color = desc->top_color;
  batch->specular = desc->specular;
  batch->shininess = s_shininess_resolve(desc->shininess);
  batch->emission = desc->emission;
  batch->curvature = desc->curvature;
  batch->wind_strength = desc->wind_strength;
  batch->wind_speed = desc->wind_speed;
  batch->casts_shadows = desc->casts_shadows;
  batch->mesh = ldk_renderer_mesh_null();
  batch->material = ldk_renderer_material_null();
  batch->instance_set = ldk_renderer_instance_set_null();
  batch->dirty = true;
  return s_grass.batch_count++;
}

static bool s_patch_instances_generate(const LDKGrassPatchDesc *desc,
    Mat4 **out_instances, u32 *out_count)
{
  Mat4 *instances = NULL;
  Vec3 normal;
  float area;
  double expected_count;
  u32 count;
  u32 random;
  Quat align;

  if (!out_instances || !out_count ||
      !s_desc_is_valid(desc, &area, &normal))
  {
    return false;
  }

  *out_instances = NULL;
  *out_count = 0u;
  random = s_random_state(desc->seed);
  expected_count = (double)area * (double)desc->density;
  count = (u32)floor(expected_count);
  if ((double)s_random_unit(&random) < expected_count - (double)count)
  {
    count += 1u;
  }

  if (count == 0u)
  {
    return true;
  }
  if (!s_allocation_size_is_valid(count, sizeof(*instances)))
  {
    return false;
  }

  instances = (Mat4 *)malloc((size_t)count * sizeof(*instances));
  if (!instances)
  {
    return false;
  }

  align = quat_from_to(vec3_make(0.0f, 1.0f, 0.0f), normal);
  for (u32 i = 0u; i < count; ++i)
  {
    float u = s_random_unit(&random);
    float v = s_random_unit(&random);
    float height = desc->min_height +
        (desc->max_height - desc->min_height) * s_random_unit(&random);
    float angle = s_random_unit(&random) * 2.0f * LDK_GRASS_PI;
    float tilt_angle = desc->min_tilt +
        (desc->max_tilt - desc->min_tilt) * s_random_unit(&random);
    Vec3 position = vec3_add(desc->plane.origin,
        vec3_add(vec3_mul(desc->plane.axis_u, u),
            vec3_mul(desc->plane.axis_v, v)));
    Quat yaw =
        quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), angle);
    Quat tilt = quat_axis_angle(
        vec3_make(sinf(desc->tilt_direction), 0.0f,
            -cosf(desc->tilt_direction)),
        tilt_angle);
    Quat rotation = quat_mul(align, quat_mul(tilt, yaw));

    instances[i] = mat4_compose(
        position, rotation, vec3_make(1.0f, height, 1.0f));
  }

  *out_instances = instances;
  *out_count = count;
  return true;
}

static LDKGrassPatchEntry *s_patch_get(LDKGrassPatch patch)
{
  LDKGrassPatchEntry *entry;

  if (!s_grass.initialized || patch.version == 0u ||
      patch.index >= s_grass.patch_count)
  {
    return NULL;
  }

  entry = &s_grass.patches[patch.index];
  if (!entry->alive || entry->version != patch.version)
  {
    return NULL;
  }
  return entry;
}

static bool s_batch_instances_reserve(
    LDKGrassBatch *batch, u32 required)
{
  Mat4 *instances;
  u32 capacity;

  if (required <= batch->instance_capacity)
  {
    return true;
  }

  capacity = batch->instance_capacity ? batch->instance_capacity : 256u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*instances)))
  {
    return false;
  }

  instances = (Mat4 *)realloc(
      batch->instances, (size_t)capacity * sizeof(*instances));
  if (!instances)
  {
    return false;
  }

  batch->instances = instances;
  batch->instance_capacity = capacity;
  return true;
}

static bool s_batch_rebuild(u32 batch_index)
{
  LDKGrassBatch *batch;
  u64 required = 0u;
  u32 write_index = 0u;

  if (batch_index >= s_grass.batch_count)
  {
    return false;
  }

  batch = &s_grass.batches[batch_index];
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    const LDKGrassPatchEntry *entry = &s_grass.patches[i];

    if (entry->alive && entry->batch_index == batch_index)
    {
      required += entry->instance_count;
      if (required > UINT32_MAX)
      {
        return false;
      }
    }
  }

  if (!s_batch_instances_reserve(batch, (u32)required))
  {
    return false;
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];

    if (!entry->alive || entry->batch_index != batch_index)
    {
      continue;
    }

    entry->instance_offset = write_index;
    if (entry->instance_count == 0u)
    {
      continue;
    }
    memcpy(batch->instances + write_index, entry->instances,
        (size_t)entry->instance_count * sizeof(*entry->instances));
    write_index += entry->instance_count;
  }

  batch->instance_count = write_index;
  if (write_index == 0u)
  {
    if (ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->instance_set);
    }
    batch->instance_set = ldk_renderer_instance_set_null();
  }
  else if (ldk_renderer_instance_set_is_valid(
               s_grass.renderer, batch->instance_set))
  {
    if (!ldk_renderer_instance_set_update(s_grass.renderer,
            batch->instance_set, batch->instances, write_index))
    {
      return false;
    }
  }
  else
  {
    batch->instance_set = ldk_renderer_instance_set_create(
        s_grass.renderer, batch->instances, write_index);
    if (!ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      return false;
    }
  }
  batch->dirty = false;
  return true;
}

static u8 s_color_channel_lerp(u8 a, u8 b, float t)
{
  float value = (float)a + ((float)b - (float)a) * t;

  value = fminf(fmaxf(value, 0.0f), 255.0f);
  return (u8)(value + 0.5f);
}

static rgba32 s_color_lerp(rgba32 a, rgba32 b, float t)
{
  u8 ar = (u8)((a >> 24u) & 0xffu);
  u8 ag = (u8)((a >> 16u) & 0xffu);
  u8 ab = (u8)((a >> 8u) & 0xffu);
  u8 aa = (u8)(a & 0xffu);
  u8 br = (u8)((b >> 24u) & 0xffu);
  u8 bg = (u8)((b >> 16u) & 0xffu);
  u8 bb = (u8)((b >> 8u) & 0xffu);
  u8 ba = (u8)(b & 0xffu);

  return ((u32)s_color_channel_lerp(ar, br, t) << 24u) |
         ((u32)s_color_channel_lerp(ag, bg, t) << 16u) |
         ((u32)s_color_channel_lerp(ab, bb, t) << 8u) |
         (u32)s_color_channel_lerp(aa, ba, t);
}

static float s_profile_slope(
    const LDKGrassBladeShape *shape, u32 point_index)
{
  u32 first = point_index > 0u ? point_index - 1u : point_index;
  u32 last = point_index + 1u < LDK_GRASS_PROFILE_POINT_COUNT
      ? point_index + 1u
      : point_index;
  float delta_height =
      shape->profile[last].height - shape->profile[first].height;
  float delta_radius =
      (shape->profile[last].width - shape->profile[first].width) * 0.5f;

  return delta_height > 0.0f ? delta_radius / delta_height : 0.0f;
}

static void s_ribbon_mesh_write(const LDKGrassBatch *batch,
    float angle, LDKMeshVertex *vertices, u32 *vertex_count,
    u32 *indices, u32 *index_count)
{
  Vec3 right = vec3_make(cosf(angle), 0.0f, sinf(angle));
  Vec3 front_normal = vec3_make(-sinf(angle), 0.0f, cosf(angle));
  u32 first_vertex = *vertex_count;

  for (u32 level = 0u; level < LDK_GRASS_PROFILE_POINT_COUNT; ++level)
  {
    float height = batch->shape.profile[level].height;
    float half_width = batch->shape.profile[level].width * 0.5f;
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));
    LDKMeshVertex *left = &vertices[(*vertex_count)++];
    LDKMeshVertex *right_vertex = &vertices[(*vertex_count)++];

    left->position = vec3_add(
        vec3_make(0.0f, height, 0.0f), vec3_mul(right, -half_width));
    left->normal = front_normal;
    left->uv = vec2_make(0.0f, height);
    left->color = color;
    left->tangent = vec4_make(
        front_normal.x, front_normal.y, front_normal.z, 0.0f);

    right_vertex->position = vec3_add(
        vec3_make(0.0f, height, 0.0f), vec3_mul(right, half_width));
    right_vertex->normal = front_normal;
    right_vertex->uv = vec2_make(1.0f, height);
    right_vertex->color = color;
    right_vertex->tangent = vec4_make(
        front_normal.x, front_normal.y, front_normal.z, 0.0f);
  }

  for (u32 level = 0u; level + 1u < LDK_GRASS_PROFILE_POINT_COUNT;
       ++level)
  {
    u32 lower_left = first_vertex + level * 2u;
    u32 lower_right = lower_left + 1u;
    u32 upper_left = lower_left + 2u;
    u32 upper_right = lower_left + 3u;

    indices[(*index_count)++] = lower_left;
    indices[(*index_count)++] = lower_right;
    indices[(*index_count)++] = upper_right;
    indices[(*index_count)++] = lower_left;
    indices[(*index_count)++] = upper_right;
    indices[(*index_count)++] = upper_left;
  }
}

static void s_radial_mesh_write(const LDKGrassBatch *batch,
    LDKMeshVertex *vertices, u32 *vertex_count, u32 *indices,
    u32 *index_count)
{
  u32 sides = batch->shape.side_count;
  bool pointed =
      batch->shape.profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width <=
      STDXM_EPS;
  u32 ring_count = pointed ? LDK_GRASS_PROFILE_POINT_COUNT - 1u
                           : LDK_GRASS_PROFILE_POINT_COUNT;

  for (u32 level = 0u; level < ring_count; ++level)
  {
    float height = batch->shape.profile[level].height;
    float radius = batch->shape.profile[level].width * 0.5f;
    float slope = s_profile_slope(&batch->shape, level);
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));

    for (u32 side = 0u; side < sides; ++side)
    {
      float angle = (float)side * 2.0f * LDK_GRASS_PI / (float)sides;
      float x = cosf(angle);
      float z = sinf(angle);
      LDKMeshVertex *vertex = &vertices[(*vertex_count)++];

      vertex->position = vec3_make(x * radius, height, z * radius);
      vertex->normal = vec3_norm(vec3_make(x, -slope, z));
      vertex->uv = vec2_make((float)side / (float)sides, height);
      vertex->color = color;
      vertex->tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
    }
  }

  for (u32 level = 0u; level + 1u < ring_count; ++level)
  {
    u32 lower = level * sides;
    u32 upper = (level + 1u) * sides;

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + next;
      indices[(*index_count)++] = lower + next;
      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + side;
      indices[(*index_count)++] = upper + next;
    }
  }

  if (pointed)
  {
    const LDKGrassProfilePoint *tip =
        &batch->shape.profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u];
    u32 apex = (*vertex_count)++;
    u32 last_ring = (ring_count - 1u) * sides;

    vertices[apex].position = vec3_make(0.0f, tip->height, 0.0f);
    vertices[apex].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[apex].uv = vec2_make(0.5f, 1.0f);
    vertices[apex].color = LDK_RGBA32(batch->top_color);
    vertices[apex].tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = last_ring + side;
      indices[(*index_count)++] = apex;
      indices[(*index_count)++] = last_ring + next;
    }
  }
  else
  {
    u32 cap_ring = *vertex_count;
    u32 source_ring = (ring_count - 1u) * sides;
    u32 center;

    for (u32 side = 0u; side < sides; ++side)
    {
      vertices[*vertex_count] = vertices[source_ring + side];
      vertices[*vertex_count].normal = vec3_make(0.0f, 1.0f, 0.0f);
      *vertex_count += 1u;
    }

    center = (*vertex_count)++;
    vertices[center].position = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[center].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[center].uv = vec2_make(0.5f, 0.5f);
    vertices[center].color = LDK_RGBA32(batch->top_color);
    vertices[center].tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = center;
      indices[(*index_count)++] = cap_ring + next;
      indices[(*index_count)++] = cap_ring + side;
    }
  }
}

static float s_profile_width_at_height(
    const LDKGrassBladeShape *shape, float height)
{
  for (u32 level = 0u;
       level + 1u < LDK_GRASS_PROFILE_POINT_COUNT; ++level)
  {
    const LDKGrassProfilePoint *lower = &shape->profile[level];
    const LDKGrassProfilePoint *upper = &shape->profile[level + 1u];

    if (height <= upper->height)
    {
      float t = (height - lower->height) /
          (upper->height - lower->height);
      return lower->width + (upper->width - lower->width) * t;
    }
  }
  return shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width;
}

static Vec3 s_thorny_stem_center(float height, float base_width)
{
  float x = (sinf(height * 5.1f) * 0.72f + height * 0.38f) *
      base_width;
  float z = (sinf(height * 7.3f + 0.65f) - sinf(0.65f)) * 0.48f *
      base_width;

  return vec3_make(x, height, z);
}

static void s_thorny_stem_mesh_write(const LDKGrassBatch *batch,
    LDKMeshVertex *vertices, u32 *vertex_count,
    u32 *indices, u32 *index_count)
{
  const LDKGrassBladeShape *shape = &batch->shape;
  float base_width = shape->profile[0].width;
  u32 sides = shape->side_count;
  bool pointed =
      shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width <=
      STDXM_EPS;
  u32 ring_count = pointed ? LDK_GRASS_PROFILE_POINT_COUNT - 1u
                           : LDK_GRASS_PROFILE_POINT_COUNT;
  u32 stem_first = *vertex_count;

  for (u32 level = 0u; level < ring_count; ++level)
  {
    float height = shape->profile[level].height;
    float radius = shape->profile[level].width * 0.5f;
    Vec3 center = s_thorny_stem_center(height, base_width);
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));

    for (u32 side = 0u; side < sides; ++side)
    {
      float angle = (float)side * 2.0f * LDK_GRASS_PI / (float)sides;
      Vec3 normal = vec3_make(cosf(angle), 0.0f, sinf(angle));
      LDKMeshVertex *vertex = &vertices[(*vertex_count)++];

      vertex->position = vec3_add(center, vec3_mul(normal, radius));
      vertex->normal = normal;
      vertex->uv = vec2_make((float)side / (float)sides, height);
      vertex->color = color;
      vertex->tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
    }
  }

  for (u32 level = 0u; level + 1u < ring_count; ++level)
  {
    u32 lower = stem_first + level * sides;
    u32 upper = lower + sides;

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + next;
      indices[(*index_count)++] = lower + next;
      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + side;
      indices[(*index_count)++] = upper + next;
    }
  }

  if (pointed)
  {
    u32 apex = (*vertex_count)++;
    u32 last_ring = stem_first + (ring_count - 1u) * sides;
    Vec3 tip = s_thorny_stem_center(1.0f, base_width);

    vertices[apex].position = tip;
    vertices[apex].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[apex].uv = vec2_make(0.5f, 1.0f);
    vertices[apex].color = LDK_RGBA32(batch->top_color);
    vertices[apex].tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = last_ring + side;
      indices[(*index_count)++] = apex;
      indices[(*index_count)++] = last_ring + next;
    }
  }
  else
  {
    u32 cap_ring = *vertex_count;
    u32 source_ring = stem_first + (ring_count - 1u) * sides;
    u32 center;

    for (u32 side = 0u; side < sides; ++side)
    {
      vertices[*vertex_count] = vertices[source_ring + side];
      vertices[*vertex_count].normal = vec3_make(0.0f, 1.0f, 0.0f);
      *vertex_count += 1u;
    }
    center = (*vertex_count)++;
    vertices[center] = vertices[cap_ring];
    vertices[center].position = s_thorny_stem_center(1.0f, base_width);
    vertices[center].uv = vec2_make(0.5f, 1.0f);
    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = center;
      indices[(*index_count)++] = cap_ring + next;
      indices[(*index_count)++] = cap_ring + side;
    }
  }

  for (u32 thorn = 0u; thorn < LDK_GRASS_THORN_COUNT; ++thorn)
  {
    float thorn_t = (float)thorn / (float)(LDK_GRASS_THORN_COUNT - 1u);
    float height = 0.16f + thorn_t * 0.68f;
    float angle = (float)thorn * 2.39996323f + 0.35f;
    Vec3 outward = vec3_make(cosf(angle), 0.0f, sinf(angle));
    Vec3 around = vec3_make(-outward.z, 0.0f, outward.x);
    float stem_radius = s_profile_width_at_height(shape, height) * 0.5f;
    float thorn_length = base_width *
        (LDK_GRASS_THORN_MAX_LENGTH_SCALE - thorn_t * 0.22f);
    float base_radius = base_width * (0.105f - thorn_t * 0.025f);
    Vec3 attachment = vec3_add(
        s_thorny_stem_center(height, base_width),
        vec3_mul(outward, stem_radius));
    Vec3 tip = vec3_add(
        vec3_add(attachment, vec3_mul(outward, thorn_length)),
        vec3_make(0.0f, thorn_length * 0.12f, 0.0f));
    u32 first = *vertex_count;
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));
    Vec3 base[3];

    base[0] = vec3_add(attachment, vec3_make(0.0f, base_radius, 0.0f));
    base[1] = vec3_add(vec3_add(attachment,
                               vec3_make(0.0f, -base_radius * 0.65f, 0.0f)),
        vec3_mul(around, base_radius * 0.9f));
    base[2] = vec3_add(vec3_add(attachment,
                               vec3_make(0.0f, -base_radius * 0.65f, 0.0f)),
        vec3_mul(around, -base_radius * 0.9f));
    for (u32 corner = 0u; corner < 3u; ++corner)
    {
      vertices[*vertex_count].position = base[corner];
      vertices[*vertex_count].normal = outward;
      vertices[*vertex_count].uv = vec2_make((float)corner * 0.5f, height);
      vertices[*vertex_count].color = color;
      vertices[*vertex_count].tangent =
          vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
      *vertex_count += 1u;
    }
    vertices[*vertex_count].position = tip;
    vertices[*vertex_count].normal = outward;
    vertices[*vertex_count].uv = vec2_make(0.5f, height);
    vertices[*vertex_count].color = color;
    vertices[*vertex_count].tangent =
        vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
    *vertex_count += 1u;

    indices[(*index_count)++] = first;
    indices[(*index_count)++] = first + 1u;
    indices[(*index_count)++] = first + 3u;
    indices[(*index_count)++] = first + 1u;
    indices[(*index_count)++] = first + 2u;
    indices[(*index_count)++] = first + 3u;
    indices[(*index_count)++] = first + 2u;
    indices[(*index_count)++] = first;
    indices[(*index_count)++] = first + 3u;
  }
}

static bool s_batch_mesh_create(LDKGrassBatch *batch)
{
  LDKMeshVertex vertices[LDK_GRASS_MAX_MESH_VERTEX_COUNT];
  u32 indices[LDK_GRASS_MAX_MESH_INDEX_COUNT];
  LDKRendererMeshDesc desc = {0};
  u32 vertex_count = 0u;
  u32 index_count = 0u;

  if (!batch || !s_shape_is_valid(&batch->shape) || !s_grass.renderer)
  {
    return false;
  }

  memset(vertices, 0, sizeof(vertices));
  switch (batch->shape.topology)
  {
  case LDK_GRASS_BLADE_TOPOLOGY_RIBBON:
    s_ribbon_mesh_write(batch, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS:
    s_ribbon_mesh_write(batch, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    s_ribbon_mesh_write(batch, LDK_GRASS_PI * 0.5f, vertices,
        &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS:
    s_ribbon_mesh_write(batch, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    s_ribbon_mesh_write(batch, LDK_GRASS_PI / 3.0f, vertices,
        &vertex_count, indices, &index_count);
    s_ribbon_mesh_write(batch, LDK_GRASS_PI * 2.0f / 3.0f, vertices,
        &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_RADIAL:
    s_radial_mesh_write(
        batch, vertices, &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM:
    s_thorny_stem_mesh_write(
        batch, vertices, &vertex_count, indices, &index_count);
    break;
  default:
    return false;
  }

  desc.vertices = vertices;
  desc.vertex_count = vertex_count;
  desc.indices = indices;
  desc.index_count = index_count;
  desc.has_tangents = true;
  batch->mesh = ldk_renderer_mesh_create(s_grass.renderer, &desc);
  if (!ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh))
  {
    batch->mesh = ldk_renderer_mesh_null();
    return false;
  }

  batch->index_count = index_count;
  return true;
}

static bool s_batch_material_create(LDKGrassBatch *batch)
{
  LDKRendererMaterialDesc desc = {0};

  if (!batch || !s_grass.renderer)
  {
    return false;
  }

  desc.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  desc.texture = ldk_renderer_texture_null();
  desc.normal_map = ldk_renderer_texture_null();
  desc.specular_map = ldk_renderer_texture_null();
  desc.color = 0xffffffffu;
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  desc.alpha_cutoff = 0.5f;
  desc.specular = batch->specular;
  desc.shininess = batch->shininess;
  desc.emission = batch->emission;
  desc.vegetation_curvature = batch->curvature;
  desc.vegetation_wind_strength = batch->wind_strength;
  desc.vegetation_wind_speed = batch->wind_speed;
  desc.vegetation = true;
  batch->material = ldk_renderer_material_create(s_grass.renderer, &desc);
  return ldk_renderer_material_is_valid(
      s_grass.renderer, batch->material);
}

void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc)
{
  if (!out_desc)
  {
    return;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->plane.axis_u = vec3_make(1.0f, 0.0f, 0.0f);
  out_desc->plane.axis_v = vec3_make(0.0f, 0.0f, -1.0f);
  out_desc->shape.profile[0] = (LDKGrassProfilePoint){0.0f, 0.10f};
  out_desc->shape.profile[1] = (LDKGrassProfilePoint){0.55f, 0.09f};
  out_desc->shape.profile[2] = (LDKGrassProfilePoint){0.85f, 0.06f};
  out_desc->shape.profile[3] = (LDKGrassProfilePoint){1.0f, 0.0f};
  out_desc->shape.topology = LDK_GRASS_BLADE_TOPOLOGY_RIBBON;
  out_desc->shape.side_count = 4u;
  out_desc->density = 1.0f;
  out_desc->min_height = 0.8f;
  out_desc->max_height = 1.0f;
  out_desc->min_tilt = 0.0f;
  out_desc->max_tilt = 0.0f;
  out_desc->tilt_direction = 0.0f;
  out_desc->bottom_color = 0x35502affu;
  out_desc->top_color = 0x89a95cffu;
  out_desc->specular = 0.0f;
  out_desc->shininess = 32.0f;
  out_desc->emission = 0.0f;
  out_desc->curvature = 0.08f;
  out_desc->wind_strength = 0.08f;
  out_desc->wind_speed = 1.4f;
  out_desc->casts_shadows = false;
}

LDKGrassPatch ldk_grass_patch_null(void)
{
  return (LDKGrassPatch){0u, 0u};
}

bool ldk_grass_patch_is_valid(LDKGrassPatch patch)
{
  return s_patch_get(patch) != NULL;
}

LDKGrassPatch ldk_grass_add(const LDKGrassPatchDesc *desc)
{
  LDKGrassPatchEntry *entry;
  Mat4 *instances;
  u32 instance_count;
  u32 batch_index;
  u32 patch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!s_grass.initialized ||
      !s_patch_instances_generate(desc, &instances, &instance_count))
  {
    return ldk_grass_patch_null();
  }

  batch_index = s_batch_find_or_add(desc);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    return ldk_grass_patch_null();
  }
  s_patch_bounds_calculate(
      desc, instances, instance_count, &bounds_min, &bounds_max);

  for (patch_index = 0u; patch_index < s_grass.patch_count; ++patch_index)
  {
    if (!s_grass.patches[patch_index].alive)
    {
      break;
    }
  }

  if (patch_index == s_grass.patch_count)
  {
    if (!s_patches_reserve(s_grass.patch_count + 1u))
    {
      free(instances);
      return ldk_grass_patch_null();
    }
    s_grass.patch_count += 1u;
  }

  entry = &s_grass.patches[patch_index];
  if (entry->version == 0u)
  {
    entry->version = 1u;
  }
  entry->desc = *desc;
  entry->instances = instances;
  entry->instance_count = instance_count;
  entry->instance_offset = 0u;
  entry->batch_index = batch_index;
  entry->bounds_min = bounds_min;
  entry->bounds_max = bounds_max;
  entry->alive = true;
  s_grass.batches[batch_index].dirty = true;
  return (LDKGrassPatch){patch_index, entry->version};
}

bool ldk_grass_update(
    LDKGrassPatch patch, const LDKGrassPatchDesc *desc)
{
  LDKGrassPatchEntry *entry = s_patch_get(patch);
  Mat4 *instances;
  u32 instance_count;
  u32 batch_index;
  u32 old_batch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!entry ||
      !s_patch_instances_generate(desc, &instances, &instance_count))
  {
    return false;
  }

  batch_index = s_batch_find_or_add(desc);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    return false;
  }
  s_patch_bounds_calculate(
      desc, instances, instance_count, &bounds_min, &bounds_max);

  old_batch_index = entry->batch_index;
  free(entry->instances);
  entry->desc = *desc;
  entry->instances = instances;
  entry->instance_count = instance_count;
  entry->instance_offset = 0u;
  entry->batch_index = batch_index;
  entry->bounds_min = bounds_min;
  entry->bounds_max = bounds_max;
  s_grass.batches[old_batch_index].dirty = true;
  s_grass.batches[batch_index].dirty = true;
  return true;
}

bool ldk_grass_remove(LDKGrassPatch patch)
{
  LDKGrassPatchEntry *entry = s_patch_get(patch);
  u32 batch_index;

  if (!entry)
  {
    return false;
  }

  batch_index = entry->batch_index;
  free(entry->instances);
  entry->instances = NULL;
  entry->instance_count = 0u;
  entry->instance_offset = 0u;
  entry->batch_index = LDK_GRASS_INVALID_BATCH;
  entry->alive = false;
  entry->version += 1u;
  if (entry->version == 0u)
  {
    entry->version = 1u;
  }
  s_grass.batches[batch_index].dirty = true;
  return true;
}

void ldk_grass_clear(void)
{
  if (!s_grass.initialized)
  {
    return;
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];

    free(entry->instances);
    entry->instances = NULL;
    entry->instance_count = 0u;
    entry->instance_offset = 0u;
    entry->batch_index = LDK_GRASS_INVALID_BATCH;
    entry->alive = false;
    entry->version += 1u;
    if (entry->version == 0u)
    {
      entry->version = 1u;
    }
  }

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];

    if (ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->instance_set);
    }
    batch->instance_set = ldk_renderer_instance_set_null();
    batch->instance_count = 0u;
    batch->dirty = false;
  }
}

bool ldk_grass_system_initialize(LDKRenderer *renderer)
{
  if (!renderer || s_grass.initialized)
  {
    return false;
  }

  memset(&s_grass, 0, sizeof(s_grass));
  s_grass.renderer = renderer;
  s_grass.initialized = true;
  return true;
}

static void s_batch_submit_pending(LDKGrassBatch *batch)
{
  u32 flags;

  if (!batch || batch->submit_count == 0u)
  {
    return;
  }

  flags = batch->casts_shadows
      ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
      : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE;
  (void)ldk_renderer_submit_mesh_instance_set_range(s_grass.renderer,
      LDK_RENDERER_VIEW_ALL, batch->mesh, batch->material, 0u,
      batch->index_count, batch->instance_set, batch->submit_offset,
      batch->submit_count, flags);
  batch->submit_count = 0u;
}

void ldk_grass_system_submit(void)
{
  if (!s_grass.initialized || !s_grass.renderer)
  {
    return;
  }

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];

    batch->submit_count = 0u;
    batch->submit_ready = false;
    if (batch->dirty && !s_batch_rebuild(i))
    {
      continue;
    }
    if (batch->instance_count == 0u)
    {
      continue;
    }
    if (!ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh) &&
        !s_batch_mesh_create(batch))
    {
      continue;
    }
    if (!ldk_renderer_material_is_valid(
            s_grass.renderer, batch->material) &&
        !s_batch_material_create(batch))
    {
      continue;
    }
    batch->submit_ready = ldk_renderer_instance_set_is_valid(
        s_grass.renderer, batch->instance_set);
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    const LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassBatch *batch;

    if (!entry->alive || entry->instance_count == 0u ||
        entry->batch_index >= s_grass.batch_count)
    {
      continue;
    }

    batch = &s_grass.batches[entry->batch_index];
    if (!batch->submit_ready)
    {
      continue;
    }

    if (!ldk_renderer_view_bounds_visible(s_grass.renderer,
            s_grass.renderer->game_view,
            entry->bounds_min, entry->bounds_max))
    {
      s_batch_submit_pending(batch);
      continue;
    }

    if (batch->submit_count != 0u &&
        entry->instance_offset !=
            batch->submit_offset + batch->submit_count)
    {
      s_batch_submit_pending(batch);
    }
    if (batch->submit_count == 0u)
    {
      batch->submit_offset = entry->instance_offset;
    }
    batch->submit_count += entry->instance_count;
  }

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    s_batch_submit_pending(&s_grass.batches[i]);
  }
}

void ldk_grass_system_terminate(void)
{
  if (!s_grass.initialized)
  {
    return;
  }

  ldk_grass_clear();
  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];

    if (s_grass.renderer &&
        ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh))
    {
      ldk_renderer_mesh_destroy(s_grass.renderer, batch->mesh);
    }
    if (s_grass.renderer &&
        ldk_renderer_material_is_valid(s_grass.renderer, batch->material))
    {
      ldk_renderer_material_destroy(s_grass.renderer, batch->material);
    }
    if (s_grass.renderer && ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->instance_set);
    }
    free(batch->instances);
  }

  free(s_grass.patches);
  free(s_grass.batches);
  memset(&s_grass, 0, sizeof(s_grass));
}
