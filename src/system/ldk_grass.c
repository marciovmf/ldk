#include <system/ldk_grass.h>

#include <ldk.h>
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

typedef struct LDKGrassResolvedDesc
{
  LDKGrassPlane plane;
  LDKGrassBladeShape shape;
  float density;
  float min_height;
  float max_height;
  float min_tilt;
  float max_tilt;
  float tilt_direction;
  rgba32 bottom_color;
  rgba32 top_color;
  float specular;
  float shininess;
  float emission;
  float curvature;
  float wind_strength;
  float wind_speed;
  float wind_direction;
  u32 seed;
  bool casts_shadows;
} LDKGrassResolvedDesc;

typedef struct LDKGrassPatchEntry
{
  LDKGrassPatchDesc desc;
  LDKGrassTypeId type_id;
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
  float wind_direction;
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
  LDKGrassSystem *owner;
  LDKRenderer *renderer;
  LDKGrassPatchEntry *patches;
  u32 patch_count;
  u32 patch_capacity;
  LDKGrassBatch *batches;
  u32 batch_count;
  u32 batch_capacity;
  u64 config_hash;
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

static bool s_blade_preset_is_valid(LDKGrassBladePreset preset)
{
  return preset >= LDK_GRASS_BLADE_PRESET_THIN &&
         preset <= LDK_GRASS_BLADE_PRESET_THORNY;
}

#define LDK_GRASS_TYPE_READ_CASE(index)                                      \
  case index:                                                                \
    out_desc->name = system->type_##index##_name;                            \
    out_desc->blade_preset = system->type_##index##_blade_preset;            \
    out_desc->density = system->type_##index##_density;                      \
    out_desc->min_height = system->type_##index##_min_height;                \
    out_desc->max_height = system->type_##index##_max_height;                \
    out_desc->blade_width = system->type_##index##_blade_width;              \
    out_desc->curvature = system->type_##index##_curvature;                  \
    out_desc->wind_strength = system->type_##index##_wind_strength;          \
    out_desc->bottom_color = system->type_##index##_bottom_color;            \
    out_desc->top_color = system->type_##index##_top_color;                  \
    return true

static bool s_type_slot_read(const LDKGrassSystem *system,
    LDKGrassTypeId type_id, LDKGrassTypeDesc *out_desc)
{
  if (!system || !out_desc || type_id >= LDK_GRASS_TYPE_COUNT)
  {
    return false;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  switch (type_id)
  {
    LDK_GRASS_TYPE_READ_CASE(0);
    LDK_GRASS_TYPE_READ_CASE(1);
    LDK_GRASS_TYPE_READ_CASE(2);
    LDK_GRASS_TYPE_READ_CASE(3);
    LDK_GRASS_TYPE_READ_CASE(4);
    LDK_GRASS_TYPE_READ_CASE(5);
    LDK_GRASS_TYPE_READ_CASE(6);
    LDK_GRASS_TYPE_READ_CASE(7);
    LDK_GRASS_TYPE_READ_CASE(8);
    LDK_GRASS_TYPE_READ_CASE(9);
    LDK_GRASS_TYPE_READ_CASE(10);
    LDK_GRASS_TYPE_READ_CASE(11);
    LDK_GRASS_TYPE_READ_CASE(12);
    LDK_GRASS_TYPE_READ_CASE(13);
    LDK_GRASS_TYPE_READ_CASE(14);
    LDK_GRASS_TYPE_READ_CASE(15);
  default:
    break;
  }
  return false;
}

#undef LDK_GRASS_TYPE_READ_CASE

#define LDK_GRASS_TYPE_WRITE_CASE(index)                                     \
  case index:                                                                \
    system->type_##index##_name = desc->name;                                \
    system->type_##index##_blade_preset = desc->blade_preset;                \
    system->type_##index##_density = desc->density;                          \
    system->type_##index##_min_height = desc->min_height;                    \
    system->type_##index##_max_height = desc->max_height;                    \
    system->type_##index##_blade_width = desc->blade_width;                  \
    system->type_##index##_curvature = desc->curvature;                      \
    system->type_##index##_wind_strength = desc->wind_strength;              \
    system->type_##index##_bottom_color = desc->bottom_color;                \
    system->type_##index##_top_color = desc->top_color;                      \
    return true

static bool s_type_slot_write(LDKGrassSystem *system,
    LDKGrassTypeId type_id, const LDKGrassTypeDesc *desc)
{
  if (!system || !desc || type_id >= LDK_GRASS_TYPE_COUNT)
  {
    return false;
  }

  switch (type_id)
  {
    LDK_GRASS_TYPE_WRITE_CASE(0);
    LDK_GRASS_TYPE_WRITE_CASE(1);
    LDK_GRASS_TYPE_WRITE_CASE(2);
    LDK_GRASS_TYPE_WRITE_CASE(3);
    LDK_GRASS_TYPE_WRITE_CASE(4);
    LDK_GRASS_TYPE_WRITE_CASE(5);
    LDK_GRASS_TYPE_WRITE_CASE(6);
    LDK_GRASS_TYPE_WRITE_CASE(7);
    LDK_GRASS_TYPE_WRITE_CASE(8);
    LDK_GRASS_TYPE_WRITE_CASE(9);
    LDK_GRASS_TYPE_WRITE_CASE(10);
    LDK_GRASS_TYPE_WRITE_CASE(11);
    LDK_GRASS_TYPE_WRITE_CASE(12);
    LDK_GRASS_TYPE_WRITE_CASE(13);
    LDK_GRASS_TYPE_WRITE_CASE(14);
    LDK_GRASS_TYPE_WRITE_CASE(15);
  default:
    break;
  }
  return false;
}

#undef LDK_GRASS_TYPE_WRITE_CASE

static bool s_type_desc_is_valid(const LDKGrassTypeDesc *desc)
{
  return desc && x_smallstr_length(&desc->name) > 0u &&
         s_blade_preset_is_valid(desc->blade_preset) &&
         isfinite(desc->density) && desc->density >= 0.0f &&
         isfinite(desc->min_height) && desc->min_height > 0.0f &&
         isfinite(desc->max_height) && desc->max_height >= desc->min_height &&
         isfinite(desc->blade_width) && desc->blade_width > 0.0f &&
         isfinite(desc->curvature) && desc->curvature >= 0.0f &&
         isfinite(desc->wind_strength) && desc->wind_strength >= 0.0f;
}

static bool s_system_config_is_valid(const LDKGrassSystem *system)
{
  LDKGrassTypeDesc desc;
  LDKGrassTypeDesc other;

  if (!system || !isfinite(system->min_tilt_degrees) ||
      system->min_tilt_degrees < 0.0f ||
      !isfinite(system->max_tilt_degrees) ||
      system->max_tilt_degrees < system->min_tilt_degrees ||
      system->max_tilt_degrees >= 90.0f ||
      !isfinite(system->tilt_direction_degrees) ||
      !isfinite(system->wind_direction_degrees) ||
      !isfinite(system->wind_force) || system->wind_force < 0.0f ||
      !isfinite(system->wind_speed) || system->wind_speed < 0.0f ||
      !isfinite(system->specular) || system->specular < 0.0f ||
      !isfinite(system->shininess) || system->shininess < 0.0f ||
      !isfinite(system->emission) || system->emission < 0.0f)
  {
    return false;
  }

  for (LDKGrassTypeId i = 0u; i < LDK_GRASS_TYPE_COUNT; ++i)
  {
    if (!s_type_slot_read(system, i, &desc))
    {
      return false;
    }
    if (x_smallstr_length(&desc.name) == 0u)
    {
      continue;
    }
    if (!s_type_desc_is_valid(&desc))
    {
      return false;
    }

    for (LDKGrassTypeId j = i + 1u; j < LDK_GRASS_TYPE_COUNT; ++j)
    {
      if (!s_type_slot_read(system, j, &other))
      {
        return false;
      }
      if (x_smallstr_length(&other.name) > 0u &&
          x_smallstr_cmp(&desc.name, &other.name) == 0)
      {
        return false;
      }
    }
  }

  return true;
}

static bool s_shape_from_type(
    const LDKGrassTypeDesc *type, LDKGrassBladeShape *out_shape)
{
  float width;

  if (!s_type_desc_is_valid(type) || !out_shape)
  {
    return false;
  }

  memset(out_shape, 0, sizeof(*out_shape));
  width = type->blade_width;
  out_shape->side_count = 4u;

  switch (type->blade_preset)
  {
  case LDK_GRASS_BLADE_PRESET_THIN:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_RIBBON;
    break;
  case LDK_GRASS_BLADE_PRESET_CROSSED:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS;
    break;
  case LDK_GRASS_BLADE_PRESET_TUFT:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS;
    break;
  case LDK_GRASS_BLADE_PRESET_FLAT_TOP:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_RIBBON;
    break;
  case LDK_GRASS_BLADE_PRESET_THORNY:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM;
    break;
  default:
    return false;
  }

  out_shape->profile[0] = (LDKGrassProfilePoint){0.0f, width};
  if (type->blade_preset == LDK_GRASS_BLADE_PRESET_FLAT_TOP)
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.55f, width * 0.95f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.85f, width * 0.88f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, width * 0.82f};
  }
  else if (type->blade_preset == LDK_GRASS_BLADE_PRESET_THORNY)
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.42f, width * 0.92f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.76f, width * 0.62f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, 0.0f};
  }
  else
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.55f, width * 0.85f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.85f, width * 0.50f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, 0.0f};
  }
  return true;
}

static bool s_resolved_desc_build(const LDKGrassSystem *system,
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *patch,
    LDKGrassResolvedDesc *out_desc)
{
  LDKGrassTypeDesc type;

  if (!system || !patch || !out_desc ||
      !s_type_slot_read(system, type_id, &type) ||
      !s_type_desc_is_valid(&type) ||
      !s_vec3_is_finite(patch->plane.origin) ||
      !s_vec3_is_finite(patch->plane.axis_u) ||
      !s_vec3_is_finite(patch->plane.axis_v) ||
      !isfinite(patch->density_scale) || patch->density_scale < 0.0f)
  {
    return false;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->plane = patch->plane;
  if (!s_shape_from_type(&type, &out_desc->shape))
  {
    return false;
  }
  out_desc->density = type.density * patch->density_scale;
  out_desc->min_height = type.min_height;
  out_desc->max_height = type.max_height;
  out_desc->min_tilt = deg_to_rad(system->min_tilt_degrees);
  out_desc->max_tilt = deg_to_rad(system->max_tilt_degrees);
  out_desc->tilt_direction = deg_to_rad(system->tilt_direction_degrees);
  out_desc->bottom_color = type.bottom_color;
  out_desc->top_color = type.top_color;
  out_desc->specular = system->specular;
  out_desc->shininess = system->shininess;
  out_desc->emission = system->emission;
  out_desc->curvature = type.curvature;
  out_desc->wind_strength = type.wind_strength * system->wind_force;
  out_desc->wind_speed = system->wind_speed;
  out_desc->wind_direction = deg_to_rad(system->wind_direction_degrees);
  out_desc->seed = patch->seed;
  out_desc->casts_shadows = system->casts_shadows;
  return isfinite(out_desc->density) && isfinite(out_desc->wind_strength);
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
    const LDKGrassResolvedDesc *desc, float *out_area, Vec3 *out_normal)
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
      !isfinite(desc->wind_direction) ||
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

static void s_patch_bounds_calculate(const LDKGrassResolvedDesc *desc,
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
    const LDKGrassBatch *batch, const LDKGrassResolvedDesc *desc)
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
         batch->wind_direction == desc->wind_direction &&
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

static u32 s_batch_find_or_add(const LDKGrassResolvedDesc *desc)
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
  batch->wind_direction = desc->wind_direction;
  batch->casts_shadows = desc->casts_shadows;
  batch->mesh = ldk_renderer_mesh_null();
  batch->material = ldk_renderer_material_null();
  batch->instance_set = ldk_renderer_instance_set_null();
  batch->dirty = true;
  return s_grass.batch_count++;
}

static bool s_patch_instances_generate(const LDKGrassResolvedDesc *desc,
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
  desc.vegetation_wind_direction = batch->wind_direction;
  desc.vegetation = true;
  batch->material = ldk_renderer_material_create(s_grass.renderer, &desc);
  return ldk_renderer_material_is_valid(
      s_grass.renderer, batch->material);
}


static void s_hash_bytes(u64 *hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;

  for (size_t i = 0u; i < size; ++i)
  {
    *hash ^= (u64)bytes[i];
    *hash *= UINT64_C(1099511628211);
  }
}

static u64 s_system_config_hash(const LDKGrassSystem *system)
{
  u64 hash = UINT64_C(14695981039346656037);
  LDKGrassTypeDesc desc;

#define LDK_GRASS_HASH_FIELD(field)                                           \
  s_hash_bytes(&hash, &system->field, sizeof(system->field))

  LDK_GRASS_HASH_FIELD(min_tilt_degrees);
  LDK_GRASS_HASH_FIELD(max_tilt_degrees);
  LDK_GRASS_HASH_FIELD(tilt_direction_degrees);
  LDK_GRASS_HASH_FIELD(wind_direction_degrees);
  LDK_GRASS_HASH_FIELD(wind_force);
  LDK_GRASS_HASH_FIELD(wind_speed);
  LDK_GRASS_HASH_FIELD(specular);
  LDK_GRASS_HASH_FIELD(shininess);
  LDK_GRASS_HASH_FIELD(emission);
  LDK_GRASS_HASH_FIELD(casts_shadows);

#undef LDK_GRASS_HASH_FIELD

  for (LDKGrassTypeId i = 0u; i < LDK_GRASS_TYPE_COUNT; ++i)
  {
    size_t name_length;

    if (!s_type_slot_read(system, i, &desc))
    {
      continue;
    }

    name_length = x_smallstr_length(&desc.name);
    s_hash_bytes(&hash, &name_length, sizeof(name_length));
    if (name_length == 0u)
    {
      continue;
    }

    s_hash_bytes(&hash, x_smallstr_cstr(&desc.name), name_length);
    s_hash_bytes(&hash, &desc.blade_preset, sizeof(desc.blade_preset));
    s_hash_bytes(&hash, &desc.density, sizeof(desc.density));
    s_hash_bytes(&hash, &desc.min_height, sizeof(desc.min_height));
    s_hash_bytes(&hash, &desc.max_height, sizeof(desc.max_height));
    s_hash_bytes(&hash, &desc.blade_width, sizeof(desc.blade_width));
    s_hash_bytes(&hash, &desc.curvature, sizeof(desc.curvature));
    s_hash_bytes(&hash, &desc.wind_strength, sizeof(desc.wind_strength));
    s_hash_bytes(&hash, &desc.bottom_color, sizeof(desc.bottom_color));
    s_hash_bytes(&hash, &desc.top_color, sizeof(desc.top_color));
  }

  return hash;
}

static void s_batch_release(LDKGrassBatch *batch)
{
  if (!batch)
  {
    return;
  }

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
  if (s_grass.renderer &&
      ldk_renderer_instance_set_is_valid(s_grass.renderer, batch->instance_set))
  {
    ldk_renderer_instance_set_destroy(s_grass.renderer, batch->instance_set);
  }

  free(batch->instances);
  memset(batch, 0, sizeof(*batch));
}

static void s_batches_release_all(void)
{
  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    s_batch_release(&s_grass.batches[i]);
  }
  s_grass.batch_count = 0u;
}

typedef struct LDKGrassPatchRebuild
{
  LDKGrassResolvedDesc resolved;
  Mat4 *instances;
  u32 instance_count;
  Vec3 bounds_min;
  Vec3 bounds_max;
  bool has_type;
} LDKGrassPatchRebuild;

static bool s_config_rebuild(LDKGrassSystem *system)
{
  LDKGrassPatchRebuild *rebuilt = NULL;

  if (!system || !s_system_config_is_valid(system) ||
      !s_batches_reserve(LDK_GRASS_TYPE_COUNT))
  {
    return false;
  }

  if (s_grass.patch_count > 0u)
  {
    if (!s_allocation_size_is_valid(
            s_grass.patch_count, sizeof(*rebuilt)))
    {
      return false;
    }
    rebuilt = (LDKGrassPatchRebuild *)calloc(
        s_grass.patch_count, sizeof(*rebuilt));
    if (!rebuilt)
    {
      return false;
    }
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassPatchRebuild *item = &rebuilt[i];

    if (!entry->alive)
    {
      continue;
    }

    if (!s_resolved_desc_build(
            system, entry->type_id, &entry->desc, &item->resolved))
    {
      /* Removing a configured type leaves existing patch handles alive but
       * with no rendered blades until the patch is updated to a valid type. */
      continue;
    }

    if (!s_patch_instances_generate(
            &item->resolved, &item->instances, &item->instance_count))
    {
      for (u32 j = 0u; j <= i; ++j)
      {
        free(rebuilt[j].instances);
      }
      free(rebuilt);
      return false;
    }
    s_patch_bounds_calculate(&item->resolved, item->instances,
        item->instance_count, &item->bounds_min, &item->bounds_max);
    item->has_type = true;
  }

  s_batches_release_all();
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassPatchRebuild *item = &rebuilt[i];
    u32 batch_index;

    if (!entry->alive)
    {
      continue;
    }

    free(entry->instances);
    entry->instances = NULL;
    entry->instance_count = 0u;
    entry->instance_offset = 0u;
    entry->batch_index = LDK_GRASS_INVALID_BATCH;

    if (!item->has_type)
    {
      continue;
    }

    batch_index = s_batch_find_or_add(&item->resolved);
    LDK_ASSERT(batch_index != LDK_GRASS_INVALID_BATCH);
    if (batch_index == LDK_GRASS_INVALID_BATCH)
    {
      free(item->instances);
      continue;
    }

    entry->instances = item->instances;
    item->instances = NULL;
    entry->instance_count = item->instance_count;
    entry->batch_index = batch_index;
    entry->bounds_min = item->bounds_min;
    entry->bounds_max = item->bounds_max;
    s_grass.batches[batch_index].dirty = true;
  }

  free(rebuilt);
  s_grass.config_hash = s_system_config_hash(system);
  return true;
}

void ldk_grass_type_desc_defaults(LDKGrassTypeDesc *out_desc)
{
  if (!out_desc)
  {
    return;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->blade_preset = LDK_GRASS_BLADE_PRESET_THIN;
  out_desc->density = 1.0f;
  out_desc->min_height = 0.8f;
  out_desc->max_height = 1.0f;
  out_desc->blade_width = 0.10f;
  out_desc->curvature = 0.08f;
  out_desc->wind_strength = 0.08f;
  out_desc->bottom_color = 0x35502affu;
  out_desc->top_color = 0x89a95cffu;
}

LDKGrassTypeId ldk_grass_get_id_by_name(const char *name)
{
  LDKGrassTypeDesc desc;

  if (!s_grass.initialized || !s_grass.owner || !name || name[0] == 0)
  {
    return LDK_GRASS_TYPE_ID_INVALID;
  }

  for (LDKGrassTypeId i = 0u; i < LDK_GRASS_TYPE_COUNT; ++i)
  {
    if (!s_type_slot_read(s_grass.owner, i, &desc) ||
        x_smallstr_length(&desc.name) == 0u)
    {
      continue;
    }
    if (x_smallstr_cmp_cstr(&desc.name, name) == 0)
    {
      return i;
    }
  }

  return LDK_GRASS_TYPE_ID_INVALID;
}

bool ldk_grass_type_is_valid(LDKGrassTypeId type_id)
{
  LDKGrassTypeDesc desc;

  return s_grass.initialized && s_grass.owner &&
         s_type_slot_read(s_grass.owner, type_id, &desc) &&
         s_type_desc_is_valid(&desc);
}

bool ldk_grass_type_get(
    LDKGrassTypeId type_id, LDKGrassTypeDesc *out_desc)
{
  if (!out_desc || !s_grass.initialized || !s_grass.owner ||
      !s_type_slot_read(s_grass.owner, type_id, out_desc) ||
      !s_type_desc_is_valid(out_desc))
  {
    return false;
  }
  return true;
}

LDKGrassTypeId ldk_grass_type_register(const LDKGrassTypeDesc *desc)
{
  LDKGrassTypeDesc current;

  if (!s_grass.initialized || !s_grass.owner ||
      !s_type_desc_is_valid(desc) ||
      ldk_grass_get_id_by_name(x_smallstr_cstr(&desc->name)) !=
          LDK_GRASS_TYPE_ID_INVALID)
  {
    return LDK_GRASS_TYPE_ID_INVALID;
  }

  for (LDKGrassTypeId i = 0u; i < LDK_GRASS_TYPE_COUNT; ++i)
  {
    if (!s_type_slot_read(s_grass.owner, i, &current))
    {
      return LDK_GRASS_TYPE_ID_INVALID;
    }
    if (x_smallstr_length(&current.name) == 0u)
    {
      if (!s_type_slot_write(s_grass.owner, i, desc))
      {
        return LDK_GRASS_TYPE_ID_INVALID;
      }
      return i;
    }
  }

  return LDK_GRASS_TYPE_ID_INVALID;
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
  out_desc->density_scale = 1.0f;
}

LDKGrassPatch ldk_grass_patch_null(void)
{
  return (LDKGrassPatch){0u, 0u};
}

bool ldk_grass_patch_is_valid(LDKGrassPatch patch)
{
  return s_patch_get(patch) != NULL;
}

LDKGrassPatch ldk_grass_add(
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc)
{
  LDKGrassResolvedDesc resolved;
  LDKGrassPatchEntry *entry;
  Mat4 *instances;
  u32 instance_count;
  u32 batch_index;
  u32 patch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!s_grass.initialized || !s_grass.owner ||
      !s_resolved_desc_build(s_grass.owner, type_id, desc, &resolved) ||
      !s_patch_instances_generate(&resolved, &instances, &instance_count))
  {
    return ldk_grass_patch_null();
  }

  batch_index = s_batch_find_or_add(&resolved);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    return ldk_grass_patch_null();
  }
  s_patch_bounds_calculate(
      &resolved, instances, instance_count, &bounds_min, &bounds_max);

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
  entry->type_id = type_id;
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

bool ldk_grass_update(LDKGrassPatch patch,
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc)
{
  LDKGrassPatchEntry *entry = s_patch_get(patch);
  LDKGrassResolvedDesc resolved;
  Mat4 *instances;
  u32 instance_count;
  u32 batch_index;
  u32 old_batch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!entry || !s_grass.owner ||
      !s_resolved_desc_build(s_grass.owner, type_id, desc, &resolved) ||
      !s_patch_instances_generate(&resolved, &instances, &instance_count))
  {
    return false;
  }

  batch_index = s_batch_find_or_add(&resolved);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    return false;
  }
  s_patch_bounds_calculate(
      &resolved, instances, instance_count, &bounds_min, &bounds_max);

  old_batch_index = entry->batch_index;
  free(entry->instances);
  entry->desc = *desc;
  entry->type_id = type_id;
  entry->instances = instances;
  entry->instance_count = instance_count;
  entry->instance_offset = 0u;
  entry->batch_index = batch_index;
  entry->bounds_min = bounds_min;
  entry->bounds_max = bounds_max;
  if (old_batch_index < s_grass.batch_count)
  {
    s_grass.batches[old_batch_index].dirty = true;
  }
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
  entry->type_id = LDK_GRASS_TYPE_ID_INVALID;
  entry->alive = false;
  entry->version += 1u;
  if (entry->version == 0u)
  {
    entry->version = 1u;
  }
  if (batch_index < s_grass.batch_count)
  {
    s_grass.batches[batch_index].dirty = true;
  }
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
    entry->type_id = LDK_GRASS_TYPE_ID_INVALID;
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

static void s_grass_submit(void)
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

int ldk_grass_system_initialize(void *data)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;
  LDKRenderer *renderer = (LDKRenderer *)ldk_module_get(LDK_MODULE_RENDERER);

  if (!system || !renderer || s_grass.initialized ||
      !s_system_config_is_valid(system))
  {
    return -1;
  }

  memset(&s_grass, 0, sizeof(s_grass));
  s_grass.owner = system;
  s_grass.renderer = renderer;
  s_grass.config_hash = s_system_config_hash(system);
  s_grass.initialized = true;
  return 0;
}

void ldk_grass_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;
  u64 config_hash;

  (void)group;
  (void)dt;

  if (!system || !s_grass.initialized || s_grass.owner != system)
  {
    return;
  }

  config_hash = s_system_config_hash(system);
  if (config_hash != s_grass.config_hash)
  {
    if (!s_config_rebuild(system))
    {
      return;
    }
  }

  s_grass_submit();
}

void ldk_grass_system_terminate(void *data)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;

  if (!s_grass.initialized || s_grass.owner != system)
  {
    return;
  }

  ldk_grass_clear();
  s_batches_release_all();
  free(s_grass.patches);
  free(s_grass.batches);
  memset(&s_grass, 0, sizeof(s_grass));
}
