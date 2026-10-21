#include "tftf_creature_system.h"
#include "tftf_creature_chain.h"

#include "../component/tftf_creature.h"
#include "../component/tftf_creature_node.h"
#include "../component/tftf_player_character.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <ldk_material.h>
#include <ldk_material_asset.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>
#include <module/ldk_renderer.h>
#include <module/ldk_scenegraph.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TFTF_CREATURE_PART_COUNT 2u
#define TFTF_CREATURE_MAX_PATH 112u
#define TFTF_CREATURE_PI 3.14159265358979323846f

typedef enum TFTFCreaturePart
{
  TFTF_CREATURE_PART_BODY = 0,
  TFTF_CREATURE_PART_APPENDAGE
} TFTFCreaturePart;

typedef struct TFTFCreatureGeometry
{
  LDKMeshVertex *vertices;
  u32 *indices;
  u32 vertex_count;
  u32 index_count;
  u32 vertex_capacity;
  u32 index_capacity;
  LDKResourceMesh mesh;
  LDKResourceMaterial material;
  LDKResourceTexture texture;
  LDKResourceTexture normal_map;
  LDKResourceTexture specular_map;
  LDKAssetMaterial asset;
  u32 asset_revision;
} TFTFCreatureGeometry;

typedef enum TFTFCreatureFootPhase
{
  TFTF_CREATURE_FOOT_PLANTED = 0,
  TFTF_CREATURE_FOOT_SWING,
  TFTF_CREATURE_FOOT_AIRBORNE
} TFTFCreatureFootPhase;

typedef struct TFTFCreatureFootState
{
  bool initialized;
  TFTFCreatureFootPhase phase;
  Vec3 planted_world;
  Vec3 current_world;
  Vec3 swing_start_world;
  Vec3 swing_target_world;
  float swing_elapsed;
  float swing_duration;
} TFTFCreatureFootState;

typedef enum TFTFCreatureStingPhase
{
  TFTF_CREATURE_STING_FORWARD = 0,
  TFTF_CREATURE_STING_HOLD,
  TFTF_CREATURE_STING_RETURN,
  TFTF_CREATURE_STING_WAIT
} TFTFCreatureStingPhase;

typedef struct TFTFCreatureInstance
{
  LDKEntity entity;
  bool seen;
  float time;
  float sting_phase_time;
  TFTFCreatureStingPhase sting_phase;
  /* Cadence is a request, not the timeline of an unfinished step. */
  float step_clock;
  u32 next_step_serial;
  bool sting_target_valid;
  Vec3 sting_locked_target;
  bool has_world;
  Mat4 last_world;
  TFTFCreatureFootState feet[TFTF_CREATURE_MAX_APPENDAGES];
  bool has_config;
  TFTFCreatureComponent last_config;
  /* Runtime node transforms are driven as bones; keep their authored rest
   * state separately so the next frame never consumes the animated pose. */
  u32 rest_count;
  LDKEntity rest_entities[TFTF_CREATURE_MAX_NODES];
  Vec3 rest_positions[TFTF_CREATURE_MAX_NODES];
  Quat rest_rotations[TFTF_CREATURE_MAX_NODES];
  u64 last_chain_signature;
  bool has_chain_signature;
  TFTFCreatureGeometry parts[TFTF_CREATURE_PART_COUNT];
} TFTFCreatureInstance;

/* A child entity with TFTFCreatureNodeComponent is an editable body node.
 * The order field, not physical Y or hierarchy insertion order, defines the
 * chain. This allows heads to descend below earlier points. */
static void s_resolve_body_nodes(LDKEntity owner, TFTFCreatureComponent *c,
    LDKEntity *out_entities)
{
  LDKTransform *parent = ldk_ecs_component_get(
      owner, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!parent)
  {
    return;
  }
  TFTFCreatureNode ordered_nodes[TFTF_CREATURE_MAX_NODES];
  LDKEntity ordered_entities[TFTF_CREATURE_MAX_NODES];
  u32 order_keys[TFTF_CREATURE_MAX_NODES];
  u32 entity_keys[TFTF_CREATURE_MAX_NODES];
  u32 count = 0;
  LDKEntity child = parent->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!transform)
    {
      break;
    }
    const LDKEntity next = transform->next_sibling;
    const TFTFCreatureNodeComponent *node = ldk_ecs_component_get_const(
        child, ldk_component_type(TFTFCreatureNodeComponent));
    if (node && count < TFTF_CREATURE_MAX_NODES)
    {
      TFTFCreatureNode value = {0};
      value.position = transform->local_position;
      /* Scale is authored in Transform, not duplicated on the component.
       * The body skin uses the three semiaxes, not just a scalar radius. */
      value.scale = vec3_make(
          fmaxf(0.01f, fabsf(transform->local_scale.x)),
          fmaxf(0.01f, fabsf(transform->local_scale.y)),
          fmaxf(0.01f, fabsf(transform->local_scale.z)));
      value.min_bend_degrees = fminf(175, fmaxf(0, node->min_bend_degrees));
      value.max_bend_degrees = fminf(175, fmaxf(
          value.min_bend_degrees, node->max_bend_degrees));
      u32 insert = count;
      while (insert > 0u &&
          (order_keys[insert-1u] > node->order ||
           (order_keys[insert-1u] == node->order &&
            entity_keys[insert-1u] > child.index)))
      {
        ordered_nodes[insert] = ordered_nodes[insert-1u];
        ordered_entities[insert] = ordered_entities[insert-1u];
        order_keys[insert] = order_keys[insert-1u];
        entity_keys[insert] = entity_keys[insert-1u];
        --insert;
      }
      ordered_nodes[insert] = value;
      ordered_entities[insert] = child;
      order_keys[insert] = node->order;
      entity_keys[insert] = child.index;
      ++count;
    }
    child = next;
  }
  if (count)
  {
    c->node_count = count;
    for (u32 i = 0; i < count; ++i)
    {
      c->nodes[i] = ordered_nodes[i];
      if (out_entities)
      {
        out_entities[i] = ordered_entities[i];
      }
    }
  }
}

typedef struct TFTFCreatureInstancePool
{
  TFTFCreatureInstance *instances;
  u32 count;
  u32 capacity;
} TFTFCreatureInstancePool;

static TFTFCreatureInstancePool s_runtime_pool;
static TFTFCreatureInstancePool s_editor_pool;

static float s_clamp(float v, float a, float b)
{
  return fminf(b, fmaxf(a, v));
}

static float s_smooth(float x)
{
  x = s_clamp(x, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

static Vec3 s_safe_norm(Vec3 v, Vec3 fallback)
{
  return vec3_len2(v) > 1e-9f ? vec3_norm(v) : fallback;
}

/* Intersection radius of a node's axis-aligned ellipsoid along a direction.
 * Both directions and sizes are in the creature's local reference frame. */
static float s_ellipsoid_radius(Vec3 size, Vec3 direction)
{
  direction = s_safe_norm(direction, vec3_make(1, 0, 0));
  const float x = direction.x / fmaxf(0.01f, fabsf(size.x));
  const float y = direction.y / fmaxf(0.01f, fabsf(size.y));
  const float z = direction.z / fmaxf(0.01f, fabsf(size.z));
  return 1.0f / sqrtf(fmaxf(1e-12f, x * x + y * y + z * z));
}

static bool s_geometry_grow(TFTFCreatureGeometry *g, u32 v, u32 i)
{
  if (v > g->vertex_capacity)
  {
    u32 new_capacity = g->vertex_capacity ? g->vertex_capacity : 64u;
    while (new_capacity < v)
    {
      if (new_capacity > UINT32_MAX / 2u)
      {
        return false;
      }
      new_capacity *= 2u;
    }
    LDKMeshVertex *p = realloc(g->vertices,
        (size_t)new_capacity * sizeof(*p));
    if (!p)
    {
      return false;
    }
    g->vertices = p;
    g->vertex_capacity = new_capacity;
  }
  if (i > g->index_capacity)
  {
    u32 new_capacity = g->index_capacity ? g->index_capacity : 128u;
    while (new_capacity < i)
    {
      if (new_capacity > UINT32_MAX / 2u)
      {
        return false;
      }
      new_capacity *= 2u;
    }
    u32 *p = realloc(g->indices, (size_t)new_capacity * sizeof(*p));
    if (!p)
    {
      return false;
    }
    g->indices = p;
    g->index_capacity = new_capacity;
  }
  return true;
}

static bool s_geometry_vertex(TFTFCreatureGeometry *g, Vec3 pos,
    Vec2 uv, u32 color)
{
  if (!s_geometry_grow(g, g->vertex_count + 1u, g->index_count))
  {
    return false;
  }
  LDKMeshVertex *v = &g->vertices[g->vertex_count++];
  memset(v, 0, sizeof(*v));
  v->position = pos;
  v->uv = uv;
  v->color = color;
  return true;
}

static bool s_geometry_triangle(TFTFCreatureGeometry *g,
    u32 a, u32 b, u32 c)
{
  if (!s_geometry_grow(g, g->vertex_count, g->index_count + 3u))
  {
    return false;
  }
  g->indices[g->index_count++] = a;
  g->indices[g->index_count++] = b;
  g->indices[g->index_count++] = c;
  return true;
}

/* Build a single continuous skin around a sampled centerline. Same topology
 * throughout animation; smooth normals are rebuilt from the current pose. */
static bool s_skin(TFTFCreatureGeometry *g, const Vec3 *points,
    const float *radii, const float *flatten, const Vec3 *sizes,
    u32 count, u32 sides, u32 color)
{
  Vec3 previous_side = vec3_make(1, 0, 0);
  u32 base = g->vertex_count;
  u32 stride = sides + 1u;
  if (count < 2u || sides < 3u)
  {
    return false;
  }
  for (u32 i = 0; i < count; ++i)
  {
    Vec3 prev = points[i ? i - 1u : i];
    Vec3 next = points[i + 1u < count ? i + 1u : i];
    Vec3 axis = s_safe_norm(vec3_sub(next, prev), vec3_make(0, 1, 0));
    Vec3 side = vec3_sub(previous_side,
        vec3_mul(axis, vec3_dot(previous_side, axis)));
    if (vec3_len2(side) < 1e-8f)
    {
      side = vec3_cross(axis,
          fabsf(axis.z) < .9f ? vec3_make(0, 0, 1) : vec3_make(0, 1, 0));
    }
    side = s_safe_norm(side, vec3_make(1, 0, 0));
    Vec3 other = s_safe_norm(vec3_cross(axis, side), vec3_make(0, 0, 1));
    previous_side = side;
    for (u32 j = 0; j <= sides; ++j)
    {
      float a = (2.0f * TFTF_CREATURE_PI * (float)j) / (float)sides;
      Vec3 direction = vec3_add(
          vec3_mul(side, cosf(a)), vec3_mul(other, sinf(a)));
      Vec3 offset = sizes != NULL
          ? vec3_mul(direction, s_ellipsoid_radius(sizes[i], direction))
          : vec3_add(
              vec3_mul(side, cosf(a) * radii[i] * flatten[i]),
              vec3_mul(other, sinf(a) * radii[i]));
      Vec2 uv = vec2_make((float)j / (float)sides,
          (float)i / (float)(count - 1u));
      if (!s_geometry_vertex(g, vec3_add(points[i], offset), uv, color))
      {
        return false;
      }
    }
  }
  for (u32 i = 0; i + 1u < count; ++i)
  {
    for (u32 j = 0; j < sides; ++j)
    {
      u32 a = base + i * stride + j;
      u32 b = a + 1u;
      u32 c = a + stride;
      u32 d = b + stride;
      if (!s_geometry_triangle(g, a, d, c) ||
          !s_geometry_triangle(g, a, b, d))
      {
        return false;
      }
    }
  }
  return true;
}

/* Smoothed centerline and ellipsoid-size profile, independent of intersections
 * between control volumes. This avoids the bead-like silhouette. */
static bool s_chain_path(const Vec3 *nodes, const Vec3 *node_sizes,
    u32 n, u32 divisions, bool pointed_start, bool pointed_end,
    Vec3 *points, Vec3 *sizes, u32 *out_count)
{
  u32 count = 0u;
  if (n < 2u || n > TFTF_CREATURE_MAX_NODES || divisions < 1u ||
      divisions > 10u)
  {
    return false;
  }
  for (u32 i = 0; i + 1u < n; ++i)
  {
    Vec3 a = nodes[i], b = nodes[i+1u];
    Vec3 before = nodes[i ? i-1u : i], after = nodes[i+2u < n ? i+2u : i+1u];
    for (u32 k = 0; k < divisions; ++k)
    {
      float t = (float)k / (float)divisions;
      float t2 = t*t, t3 = t2*t;
      float h0 = 2*t3-3*t2+1, h1=t3-2*t2+t;
      float h2=-2*t3+3*t2,h3=t3-t2;
      Vec3 p = vec3_add(vec3_add(vec3_mul(a,h0),vec3_mul(b,h2)),
          vec3_add(vec3_mul(vec3_sub(b,before),h1*.5f),
                   vec3_mul(vec3_sub(after,a),h3*.5f)));
      points[count] = p;
      sizes[count] = vec3_lerp(
          node_sizes[i], node_sizes[i+1u], s_smooth(t));
      count++;
    }
  }
  points[count] = nodes[n-1u];
  sizes[count] = node_sizes[n - 1u];
  count++;
  /* Rounded ends: a few hemispherical profiles, no separate sphere meshes. */
  const u32 caps = 3u;
  if (count + caps*2u > TFTF_CREATURE_MAX_PATH)
  {
    return false;
  }
  Vec3 forward = s_safe_norm(vec3_sub(points[1],points[0]),vec3_make(0,1,0));
  Vec3 backward = s_safe_norm(vec3_sub(points[count-1u],points[count-2u]),forward);
  Vec3 first = points[0], last=points[count-1u];
  Vec3 first_size = sizes[0], last_size = sizes[count - 1u];
  float first_r = s_ellipsoid_radius(first_size, forward);
  float last_r = s_ellipsoid_radius(last_size, backward);
  memmove(points+caps,points,(size_t)count*sizeof(*points));
  memmove(sizes+caps,sizes,(size_t)count*sizeof(*sizes));
  for (u32 j=0;j<caps;++j)
  {
    float q=(float)(caps-j)/(float)caps;
    points[j]=vec3_sub(first,vec3_mul(forward,first_r*q));
    sizes[j] = vec3_mul(first_size, pointed_start ? (1.0f - q) :
        sqrtf(fmaxf(0, 1-q*q)));
    points[count+caps+j]=vec3_add(last,
        vec3_mul(backward,last_r*((float)(j+1u)/(float)caps)));
    q=(float)(j+1u)/(float)caps;
    sizes[count+caps+j] = vec3_mul(last_size, pointed_end ?
        (1.0f - q) : sqrtf(fmaxf(0, 1-q*q)));
  }
  *out_count=count+caps*2u;
  return true;
}

static bool s_body_path(const TFTFCreatureComponent *c, const Vec3 *nodes,
    Vec3 *points, Vec3 *sizes, u32 *out_count)
{
  Vec3 node_sizes[TFTF_CREATURE_MAX_NODES];
  for (u32 i = 0u; i < c->node_count && i < TFTF_CREATURE_MAX_NODES; ++i)
  {
    node_sizes[i] = c->nodes[i].scale;
  }
  return s_chain_path(nodes, node_sizes, c->node_count,
      c->segments_per_node,
      c->start_cap == TFTF_CREATURE_CAP_POINTED,
      c->end_cap == TFTF_CREATURE_CAP_POINTED,
      points, sizes, out_count);
}

/* Each attached chain is made from the SAME node entities as the body.
 * Parent node transforms are supplied from the resolved pose, not by
 * reading potentially stale runtime scenegraph matrices. */
typedef struct TFTFCreatureResolvedChain
{
  u32 count;
  TFTFCreatureEndCap start_cap;
  TFTFCreatureEndCap end_cap;
  Vec3 centers[TFTF_CREATURE_MAX_NODES];
  Vec3 scales[TFTF_CREATURE_MAX_NODES];
} TFTFCreatureResolvedChain;

static u64 s_chain_hash(u64 hash, const void *source, size_t bytes)
{
  const u8 *data = source;
  for (size_t i = 0u; i < bytes; ++i)
    hash = (hash ^ (u64)data[i]) * 1099511628211ull;
  return hash;
}

static Vec3 s_chain_extents(Mat4 transform, Vec3 scale)
{
  const Vec3 x = mat4_mul_dir(transform, vec3_make(scale.x, 0, 0));
  const Vec3 y = mat4_mul_dir(transform, vec3_make(0, scale.y, 0));
  const Vec3 z = mat4_mul_dir(transform, vec3_make(0, 0, scale.z));
  return vec3_make(
      fmaxf(.001f, sqrtf(x.x*x.x + y.x*y.x + z.x*z.x)),
      fmaxf(.001f, sqrtf(x.y*x.y + y.y*y.y + z.y*z.y)),
      fmaxf(.001f, sqrtf(x.z*x.z + y.z*y.y + z.z*z.z)));
}

static void s_collect_chains_from_node(LDKEntity parent_node,
    Mat4 parent_matrix, TFTFCreatureResolvedChain *result,
    u32 *count, u32 depth, u64 *signature)
{
  if (depth >= TFTF_CREATURE_MAX_CHAIN_DEPTH ||
      *count >= TFTF_CREATURE_MAX_CHAINS) return;
  const LDKTransform *parent = ldk_ecs_component_get_const(
      parent_node, LDK_COMPONENT_TYPE_TRANSFORM);
  LDKEntity child = parent ? parent->first_child : x_handle_null();
  while (!x_handle_is_null(child) && *count < TFTF_CREATURE_MAX_CHAINS)
  {
    const LDKTransform *chain_tf = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!chain_tf) break;
    const LDKEntity next_chain = chain_tf->next_sibling;
    const TFTFCreatureChainComponent *chain = ldk_ecs_component_get_const(
        child, ldk_component_type(TFTFCreatureChainComponent));
    if (chain && chain->enabled)
    {
      const Mat4 chain_matrix = mat4_mul(parent_matrix, mat4_compose(
          chain_tf->local_position, chain_tf->local_rotation,
          chain_tf->local_scale));
      LDKEntity ordered[TFTF_CREATURE_MAX_NODES] = {0};
      const LDKTransform *ordered_tf[TFTF_CREATURE_MAX_NODES] = {0};
      u32 keys[TFTF_CREATURE_MAX_NODES] = {0};
      u32 n = 0u;
      LDKEntity joint = chain_tf->first_child;
      while (!x_handle_is_null(joint))
      {
        const LDKTransform *jt = ldk_ecs_component_get_const(
            joint, LDK_COMPONENT_TYPE_TRANSFORM);
        if (!jt) break;
        const LDKEntity next_joint = jt->next_sibling;
        const TFTFCreatureNodeComponent *node = ldk_ecs_component_get_const(
            joint, ldk_component_type(TFTFCreatureNodeComponent));
        if (node && n < TFTF_CREATURE_MAX_NODES)
        {
          u32 at = n;
          while (at > 0u && (keys[at - 1u] > node->order ||
              (keys[at - 1u] == node->order &&
               ordered[at - 1u].index > joint.index)))
          {
            ordered[at] = ordered[at - 1u];
            ordered_tf[at] = ordered_tf[at - 1u];
            keys[at] = keys[at - 1u];
            --at;
          }
          ordered[at] = joint;
          ordered_tf[at] = jt;
          keys[at] = node->order;
          ++n;
        }
        joint = next_joint;
      }
      if (n >= 2u)
      {
        TFTFCreatureResolvedChain *dst = &result[(*count)++];
        *dst = (TFTFCreatureResolvedChain){0};
        dst->count = n;
        dst->start_cap = chain->start_cap;
        dst->end_cap = chain->end_cap;
        for (u32 i = 0u; i < n; ++i)
        {
          const LDKTransform *tf = ordered_tf[i];
          const Mat4 node_matrix = mat4_mul(chain_matrix, mat4_compose(
              tf->local_position, tf->local_rotation, tf->local_scale));
          dst->centers[i] = mat4_mul_point(
              chain_matrix, tf->local_position);
          dst->scales[i] = s_chain_extents(mat4_mul(chain_matrix,
              mat4_from_quat(tf->local_rotation)),
              vec3_make(fabsf(tf->local_scale.x),
                        fabsf(tf->local_scale.y),
                        fabsf(tf->local_scale.z)));
          (void)node_matrix;
          s_collect_chains_from_node(ordered[i], node_matrix, result,
              count, depth + 1u, signature);
        }
        *signature = s_chain_hash(*signature, dst, sizeof(*dst));
      }
    }
    child = next_chain;
  }
}

static bool s_render_chains(const TFTFCreatureComponent *c,
    TFTFCreatureInstance *instance,
    const TFTFCreatureResolvedChain *chains, u32 count)
{
  for (u32 i = 0u; i < count; ++i)
  {
    const TFTFCreatureResolvedChain *chain = &chains[i];
    Vec3 points[TFTF_CREATURE_MAX_PATH];
    Vec3 sizes[TFTF_CREATURE_MAX_PATH];
    u32 path_count = 0u;
    if (!s_chain_path(chain->centers, chain->scales, chain->count,
            c->segments_per_node,
            chain->start_cap == TFTF_CREATURE_CAP_POINTED,
            chain->end_cap == TFTF_CREATURE_CAP_POINTED,
            points, sizes, &path_count) ||
        !s_skin(&instance->parts[TFTF_CREATURE_PART_APPENDAGE],
            points, NULL, NULL, sizes, path_count,
            c->radial_segments, c->appendage_color))
      return false;
  }
  return true;
}

static void s_normals(TFTFCreatureGeometry *g)
{
  for (u32 i = 0; i < g->vertex_count; ++i)
  {
    g->vertices[i].normal = vec3_make(0, 0, 0);
    g->vertices[i].tangent = vec4_make(0, 0, 0, 0);
  }
  for (u32 i = 0; i + 2u < g->index_count; i += 3u)
  {
    LDKMeshVertex *a = &g->vertices[g->indices[i]];
    LDKMeshVertex *b = &g->vertices[g->indices[i + 1u]];
    LDKMeshVertex *c = &g->vertices[g->indices[i + 2u]];
    Vec3 e1 = vec3_sub(b->position, a->position);
    Vec3 e2 = vec3_sub(c->position, a->position);
    Vec3 normal = vec3_cross(e1, e2);
    a->normal = vec3_add(a->normal, normal);
    b->normal = vec3_add(b->normal, normal);
    c->normal = vec3_add(c->normal, normal);
    float du1 = b->uv.x - a->uv.x;
    float dv1 = b->uv.y - a->uv.y;
    float du2 = c->uv.x - a->uv.x;
    float dv2 = c->uv.y - a->uv.y;
    float determinant = du1 * dv2 - du2 * dv1;
    if (fabsf(determinant) > 1e-8f)
    {
      Vec3 tangent = vec3_mul(vec3_sub(vec3_mul(e1, dv2),
          vec3_mul(e2, dv1)), 1.0f / determinant);
      u32 ia = g->indices[i], ib = g->indices[i+1u], ic = g->indices[i+2u];
      u32 ids[3] = {ia, ib, ic};
      for (u32 v = 0; v < 3u; ++v)
      {
        Vec4 *stored = &g->vertices[ids[v]].tangent;
        stored->x += tangent.x;
        stored->y += tangent.y;
        stored->z += tangent.z;
      }
    }
  }
  for (u32 i = 0; i < g->vertex_count; ++i)
  {
    Vec3 normal = s_safe_norm(g->vertices[i].normal, vec3_make(0, 1, 0));
    Vec3 tangent = vec3_make(g->vertices[i].tangent.x,
        g->vertices[i].tangent.y, g->vertices[i].tangent.z);
    tangent = vec3_sub(tangent, vec3_mul(normal, vec3_dot(normal, tangent)));
    tangent = s_safe_norm(tangent, vec3_make(1, 0, 0));
    g->vertices[i].normal = normal;
    g->vertices[i].tangent = vec4_make(tangent.x, tangent.y, tangent.z, 1.0f);
  }
}

/* Capture the player at the beginning of each strike. Holding the
 * world-space target for a full cycle lets the player dodge the attack.
 * Editor preview uses the manually authored sting_target instead. */
typedef struct TFTFCreatureTargetSearch
{
  LDKEntity entity;
  bool found;
} TFTFCreatureTargetSearch;

static bool s_find_player(LDKEntity candidate, void *user)
{
  TFTFCreatureTargetSearch *search = user;
  if (!ldk_ecs_component_get_const(candidate,
          ldk_component_type(TFTFPlayerCharacterComponent)))
  {
    return true;
  }
  search->entity = candidate;
  search->found = true;
  return false;
}

static Vec3 s_capture_sting_target(const TFTFCreatureComponent *c,
    Mat4 creature_world, bool preview)
{
  /* Manual target remains useful for authoring and isolated testing. */
  Vec3 target = c->sting_target;
  if (preview)
  {
    return target;
  }
  TFTFCreatureTargetSearch search = {0};
  (void)ldk_ecs_entity_foreach(s_find_player, &search);
  if (search.found)
  {
    (void)ldk_scenegraph_update_entity(search.entity);
    Mat4 player_world;
    if (ldk_transform_get_world_matrix(search.entity, &player_world))
    {
      const Vec3 world_pos = mat4_mul_point(player_world,
          vec3_make(0, 0, 0));
      target = mat4_mul_point(mat4_inverse_affine(creature_world), world_pos);
    }
  }
  return target;
}

/* Durations at multiplier 1, matching the original 4.4-second attack.
 * Every stage has its own clock, so a faster return does not shorten the
 * target hold or the interval between strikes. animation_dt already includes
 * animation_speed, which continues to scale all four stages. */
static void s_advance_sting(const TFTFCreatureComponent *c,
    TFTFCreatureInstance *instance, float animation_dt,
    Mat4 creature_world, bool preview, bool config_changed)
{
  const float forward_duration = 4.4f * .45f;
  const float return_duration = 4.4f * .55f;
  const float forward_speed = fmaxf(0.0f, c->sting_speed);
  const float return_speed = fmaxf(0.0f, c->sting_return_speed);
  const float hold = fmaxf(0.0f, c->sting_hold_time);
  const float interval = fmaxf(0.0f, c->sting_interval);
  float remaining = animation_dt;

  /* Preserve the leftover time on transitions, including large frame deltas.
   * The loop is bounded to avoid unbounded work with tiny cycle durations. */
  for (u32 i = 0u; i < 32u; ++i)
  {
    if (instance->sting_phase == TFTF_CREATURE_STING_WAIT)
    {
      const float until_next = fmaxf(0.0f,
          interval - instance->sting_phase_time);
      if (remaining < until_next)
      {
        instance->sting_phase_time += remaining;
        break;
      }
      remaining -= until_next;
      instance->sting_phase = TFTF_CREATURE_STING_FORWARD;
      instance->sting_phase_time = 0.0f;
      instance->sting_target_valid = false;
    }

    if (instance->sting_phase == TFTF_CREATURE_STING_FORWARD &&
        !instance->sting_target_valid)
    {
      instance->sting_locked_target = s_capture_sting_target(
          c, creature_world, preview);
      instance->sting_target_valid = true;
    }

    float duration;
    float speed;
    TFTFCreatureStingPhase next_phase;
    switch (instance->sting_phase)
    {
      case TFTF_CREATURE_STING_FORWARD:
        duration = forward_duration;
        speed = forward_speed;
        next_phase = TFTF_CREATURE_STING_HOLD;
        break;
      case TFTF_CREATURE_STING_HOLD:
        duration = hold;
        speed = 1.0f;
        next_phase = TFTF_CREATURE_STING_RETURN;
        break;
      case TFTF_CREATURE_STING_RETURN:
        duration = return_duration;
        speed = return_speed;
        next_phase = TFTF_CREATURE_STING_WAIT;
        break;
      default:
        /* WAIT transitions at the beginning of the loop. */
        return;
    }

    if (speed <= 0.0f)
    {
      break;
    }
    const float until_end = fmaxf(0.0f,
        (duration - instance->sting_phase_time) / speed);
    if (remaining < until_end)
    {
      instance->sting_phase_time += remaining * speed;
      break;
    }
    remaining -= until_end;
    instance->sting_phase = next_phase;
    instance->sting_phase_time = 0.0f;
    if (next_phase == TFTF_CREATURE_STING_WAIT)
    {
      instance->sting_target_valid = false;
    }
  }

  /* Updating the preview's target must not reset the current animation. */
  if (preview && config_changed &&
      instance->sting_phase != TFTF_CREATURE_STING_WAIT)
  {
    instance->sting_locked_target = s_capture_sting_target(
        c, creature_world, preview);
    instance->sting_target_valid = true;
  }
}

static void s_animate_body(const TFTFCreatureComponent *c,
    TFTFCreatureInstance *instance, Vec3 *out)
{
  u32 n=c->node_count;
  /* Body bob follows actual feet in Swing. Parallel steps contribute a
   * single bounded bob, not one oscillation per requested gait beat. */
  float bob = 0.0f;
  if (c->motion == TFTF_CREATURE_MOTION_WALK)
  {
    float swing_sum = 0.0f;
    u32 swing_count = 0u;
    const u32 foot_count = c->appendage_count > TFTF_CREATURE_MAX_APPENDAGES
        ? TFTF_CREATURE_MAX_APPENDAGES : c->appendage_count;
    for (u32 i = 0u; i < foot_count; ++i)
    {
      const TFTFCreatureFootState *foot = &instance->feet[i];
      if (foot->phase != TFTF_CREATURE_FOOT_SWING ||
          foot->swing_duration <= 0.0f)
      {
        continue;
      }
      const float phase = s_clamp(
          foot->swing_elapsed / foot->swing_duration, 0.0f, 1.0f);
      swing_sum += sinf(TFTF_CREATURE_PI * phase);
      ++swing_count;
    }
    if (swing_count)
    {
      bob = .055f * fminf(1.0f, c->movement_speed) *
          swing_sum / (float)swing_count;
    }
  }
  for (u32 i = 0; i < n; ++i)
  {
    float weight = n > 1u ? (float)i / (float)(n - 1u) : 0.0f;
    out[i] = c->nodes[i].position;
    out[i].y += c->body_height + bob;
    if (c->motion != TFTF_CREATURE_MOTION_STING)
    {
      out[i].z += c->body_sway * sinf(instance->time * c->body_sway_speed -
          weight * 2.0f * TFTF_CREATURE_PI);
    }
  }
  if (c->motion==TFTF_CREATURE_MOTION_CRAWL)
  {
    for(u32 i=1;i+1u<n;++i)out[i].y+=
        .08f*sinf(instance->time*3.0f-(float)i*1.4f);
  }
  else if (c->motion == TFTF_CREATURE_MOTION_STING && n >= 2u &&
      c->head != TFTF_CREATURE_HEAD_NONE)
  {
    /* The target can be at any distance. Only the goal moves; FABRIK owns
     * every joint position and keeps the authored link lengths intact. */
    float strength = 0.0f;
    switch (instance->sting_phase)
    {
      case TFTF_CREATURE_STING_FORWARD:
        strength = s_smooth(s_clamp(
            instance->sting_phase_time / (4.4f * .45f), 0.0f, 1.0f));
        break;
      case TFTF_CREATURE_STING_HOLD:
        strength = 1.0f;
        break;
      case TFTF_CREATURE_STING_RETURN:
        strength = 1.0f - s_smooth(s_clamp(
            instance->sting_phase_time / (4.4f * .55f), 0.0f, 1.0f));
        break;
      case TFTF_CREATURE_STING_WAIT:
        break;
    }
    TFTFCreatureChain chain;
    TFTFCreaturePose pose;
    tftf_creature_chain_build(&chain, c->nodes, n);
    /* Body height is a rigid translation, not a change in link lengths. */
    for (u32 i = 0u; i < n; ++i)
    {
      chain.rest[i] = out[i];
    }
    tftf_creature_chain_rest_pose(&chain, &pose);
    const u32 head = c->head == TFTF_CREATURE_HEAD_FIRST ? 0u : n - 1u;
    if (strength > 0.0f)
    {
      const Vec3 goal = vec3_lerp(pose.nodes[head],
          instance->sting_locked_target, strength);
      (void)tftf_creature_chain_solve(&chain, &pose, goal, head);
    }
    memcpy(out, pose.nodes, (size_t)n * sizeof(*out));
  }

}

/* The Creature Lab's interleaved ring order (offset gait):
 * 8 legs => E1 D3 E2 D4 E3 D1 E4 D2. E is the even slot. */
static u32 s_gait_order_index(u32 count, u32 serial)
{
  if (count == 0u)
  {
    return 0u;
  }
  const u32 pairs = (count + 1u) / 2u;
  const u32 order = serial % count;
  if ((order & 1u) == 0u)
  {
    const u32 left = (order / 2u) * 2u;
    return left < count ? left : count - 1u;
  }
  const u32 ring = ((pairs + (order / 2u) + pairs - 2u) % pairs);
  const u32 right = ring * 2u + 1u;
  return right < count ? right : count - 1u;
}

/* Constrain a detached foot to its maximum geometric reach. */
static Vec3 s_foot_reach_limit(Vec3 root, Vec3 tip, float max_distance)
{
  Vec3 delta = vec3_sub(tip, root);
  const float length = vec3_len(delta);
  if (length > max_distance && length > 1e-6f)
  {
    tip = vec3_add(root, vec3_mul(delta, max_distance / length));
  }
  return tip;
}

static Vec3 s_foot_advance(Vec3 from, Vec3 to, float distance)
{
  Vec3 delta = vec3_sub(to, from);
  const float length = vec3_len(delta);
  if (length <= distance || length <= 1e-6f)
  {
    return to;
  }
  return vec3_add(from, vec3_mul(delta, distance / length));
}

/* A stride is initiated by support error, not by an animation phase.
 * Several feet may swing concurrently. The gait cursor only chooses which
 * eligible foot starts next; the swing lifetime belongs to that foot. */
static bool s_appendages(const TFTFCreatureComponent *c,
    TFTFCreatureInstance *ins, const Vec3 *nodes, Mat4 world, float dt)
{
  u32 count = c->appendage_count;
  if (c->appendage_type != TFTF_CREATURE_APPENDAGE_LEG || count == 0u)
  {
    return true;
  }
  count = count > TFTF_CREATURE_MAX_APPENDAGES
      ? TFTF_CREATURE_MAX_APPENDAGES : count;
  const u32 pairs = (count + 1u) / 2u;
  const bool walking = c->motion == TFTF_CREATURE_MOTION_WALK;
  const float swing_height = c->step_height;
  const Vec3 up = vec3_make(0, 1, 0);
  const Vec3 forward = vec3_make(0, 0, -1);
  const Vec3 lateral = vec3_make(1, 0, 0);
  const Mat4 inverse_world = mat4_inverse_affine(world);
  const bool vertical = c->body_axis == TFTF_CREATURE_BODY_VERTICAL;
  float body_length = 0.0f;
  for (u32 j = 1u; j < c->node_count; ++j)
  {
    body_length += vec3_len(vec3_sub(nodes[j], nodes[j - 1u]));
  }
  Vec3 roots[TFTF_CREATURE_MAX_APPENDAGES];
  Vec3 outwards[TFTF_CREATURE_MAX_APPENDAGES];
  Vec3 roots_world[TFTF_CREATURE_MAX_APPENDAGES];
  u32 radial_node = 0u;
  if (!vertical)
  {
    radial_node = c->node_count / 2u;
  }
  else
  {
    /* Select the attachment node from the authored rest pose, not from the
     * animated pose. During Sting the head can descend below the base; using
     * animated heights would move every radial appendage to the head. The
     * root position itself still follows this node's animated position. */
    float lowest = INFINITY;
    for (u32 j = 0u; j < c->node_count; ++j)
    {
      const float y = c->nodes[j].position.y - c->nodes[j].scale.y;
      if (y < lowest)
      {
        lowest = y;
        radial_node = j;
      }
    }
  }
  Vec3 ideals[TFTF_CREATURE_MAX_APPENDAGES];
  Vec3 contacts[TFTF_CREATURE_MAX_APPENDAGES];
  const float ground_height = world.m[13];
  const float reach = fmaxf(.1f, c->appendage_length * 2.0f * .98f);
  const float requested_speed = fmaxf(0.0f, c->movement_speed);
  /* The only authored locomotion speed controls foot travel too. Use the
   * measured world velocity to anticipate motion imposed by a parent. */
  Vec3 world_velocity = vec3_make(0, 0, 0);
  if (ins->has_world && dt > 1e-5f)
  {
    world_velocity = vec3_make(
        (world.m[12] - ins->last_world.m[12]) / dt, 0.0f,
        (world.m[14] - ins->last_world.m[14]) / dt);
  }
  const float measured_speed = vec3_len(world_velocity);
  const float max_predict_speed = fmaxf(.5f, requested_speed * 2.0f);
  const float locomotion_speed = fmaxf(requested_speed,
      fminf(measured_speed, max_predict_speed));
  const float foot_speed = fmaxf(.85f, locomotion_speed * 4.25f);
  const float stride = fmaxf(.12f, c->step_length);
  const float trigger_distance = fmaxf(.08f, stride * .32f);
  const float cadence_interval = fmaxf(.035f, fminf(.40f,
      stride * .8f / fmaxf(.01f, locomotion_speed * (float)count)));
  /* A sudden teleport must not predict several metres of foot lead. */
  if (measured_speed > max_predict_speed && measured_speed > 1e-5f)
  {
    world_velocity = vec3_mul(world_velocity,
        max_predict_speed / measured_speed);
  }

  for (u32 i = 0; i < count; ++i)
  {
    const float side = (i % 2u) ? 1.0f : -1.0f;
    const u32 pair = i / 2u;
    Vec3 outward;
    if (c->appendage_distribution == TFTF_CREATURE_APPENDAGE_RADIAL)
    {
      /* Consecutive indexes are left/right legs of the SAME pair. A
       * half-circle per side keeps the pairs mirrored and their gait labels
       * meaningful; a full circle over the raw index staggered each pair. */
      const float angle = TFTF_CREATURE_PI *
          ((float)pair + .5f) / (float)pairs;
      outward = s_safe_norm(vec3_add(
          vec3_mul(lateral, side * sinf(angle)),
          vec3_mul(forward, cosf(angle))), lateral);
      /* Roots emerge from the lower lateral skin of an actual body node. */
      const Vec3 surface_dir = s_safe_norm(vec3_add(outward,
          vec3_mul(up, -.20f)), outward);
      const float radius = s_ellipsoid_radius(
          c->nodes[radial_node].scale, surface_dir);
      roots[i] = vec3_add(nodes[radial_node], vec3_mul(surface_dir,
          fmaxf(0.0f, radius + c->appendage_root_offset)));
    }
    else if (vertical)
    {
      const float longitudinal =
          ((float)pair - .5f * (float)(pairs - 1u)) *
          c->appendage_spacing;
      outward = vec3_mul(lateral, side);
      const float base_width = s_ellipsoid_radius(
          c->nodes[0].scale, outward);
      roots[i] = vec3_add(nodes[0],
          vec3_add(vec3_mul(forward, longitudinal),
              vec3_mul(outward, fmaxf(0.0f,
                  base_width + c->appendage_root_offset))));
    }
    else
    {
      /* Curved horizontal bodies distribute pairs along the authored spine. */
      const float t = s_clamp(.5f + (((float)pair -
          .5f * (float)(pairs - 1u)) * c->appendage_spacing /
          fmaxf(.25f, body_length)), .07f, .93f);
      float remaining = t * body_length;
      u32 segment = 0u;
      for (; segment + 1u < c->node_count - 1u; ++segment)
      {
        const float distance = vec3_len(
            vec3_sub(nodes[segment + 1u], nodes[segment]));
        if (remaining <= distance)
        {
          break;
        }
        remaining -= distance;
      }
      const float segment_length = fmaxf(.001f, vec3_len(
          vec3_sub(nodes[segment + 1u], nodes[segment])));
      outward = vec3_mul(lateral, side);
      roots[i] = vec3_add(vec3_lerp(
          nodes[segment], nodes[segment + 1u],
          s_clamp(remaining / segment_length, 0.0f, 1.0f)),
          vec3_mul(outward, fmaxf(0.0f,
              s_ellipsoid_radius(c->nodes[segment].scale, outward) +
              c->appendage_root_offset)));
    }
    outwards[i] = outward;
    roots_world[i] = mat4_mul_point(world, roots[i]);
    Vec3 ideal_local = vec3_add(roots[i], vec3_mul(outward,
        c->appendage_reach * c->appendage_length));
    ideal_local.y = 0.0f;
    ideals[i] = mat4_mul_point(world, ideal_local);
    ideals[i].y = ground_height;
    TFTFCreatureFootState *foot = &ins->feet[i];
    if (!foot->initialized)
    {
      foot->planted_world = ideals[i];
      foot->current_world = ideals[i];
      foot->phase = TFTF_CREATURE_FOOT_PLANTED;
      if (vec3_len2(vec3_sub(ideals[i], roots_world[i])) > reach * reach)
      {
        foot->phase = TFTF_CREATURE_FOOT_AIRBORNE;
        foot->current_world = s_foot_reach_limit(
            roots_world[i], ideals[i], reach);
      }
      foot->initialized = true;
    }
  }

  /* Support is lost only at the geometric limit, never because a gait
   * interval elapsed. Detached feet chase a reachable point at foot speed. */
  for (u32 i = 0; i < count; ++i)
  {
    TFTFCreatureFootState *foot = &ins->feet[i];
    const Vec3 root_world = roots_world[i];
    if (foot->phase == TFTF_CREATURE_FOOT_PLANTED &&
        vec3_len2(vec3_sub(foot->planted_world, root_world)) > reach * reach)
    {
      foot->phase = TFTF_CREATURE_FOOT_AIRBORNE;
      foot->current_world = s_foot_reach_limit(
          root_world, foot->planted_world, reach);
    }
    if (foot->phase == TFTF_CREATURE_FOOT_AIRBORNE)
    {
      /* An airborne appendage must look for a point ON the support plane.
       * Clamping a 3D target to a sphere can leave it floating above the
       * ground; requiring the unclamped ideal to lie within 85% of reach
       * could then keep every appendage airborne and stall the entire gait.
       * Project onto the reachable ground disc before trying to replant. */
      const float safe_reach = reach * .95f;
      const float vertical = ground_height - root_world.y;
      if (fabsf(vertical) < safe_reach)
      {
        const float horizontal_reach = sqrtf(fmaxf(0.0f,
            safe_reach * safe_reach - vertical * vertical));
        Vec3 horizontal = vec3_sub(ideals[i], root_world);
        horizontal.y = 0.0f;
        const float distance = vec3_len(horizontal);
        if (distance > horizontal_reach && distance > 1e-6f)
        {
          horizontal = vec3_mul(horizontal,
              horizontal_reach / distance);
        }
        Vec3 desired = vec3_add(root_world, horizontal);
        desired.y = ground_height;
        if (dt > 0.0f)
        {
          foot->current_world = s_foot_advance(
              foot->current_world, desired, foot_speed * dt);
        }
        foot->current_world = s_foot_reach_limit(
            root_world, foot->current_world, reach);
        if (vec3_len2(vec3_sub(foot->current_world, desired)) < .0025f)
        {
          foot->planted_world = desired;
          foot->current_world = desired;
          foot->phase = TFTF_CREATURE_FOOT_PLANTED;
        }
      }
      else
      {
        /* No point on this ground plane lies within reach. Keep the limb
         * airborne rather than inventing a support or stretching it. */
        foot->current_world = s_foot_reach_limit(
            root_world, foot->current_world, reach);
      }
    }
  }

  /* Every foot owns its swing clock, allowing overlapping but complete
   * steps. Restrict simultaneous unsupported feet to retain body support. */
  u32 unsupported = 0u;
  for (u32 i = 0; i < count; ++i)
  {
    TFTFCreatureFootState *foot = &ins->feet[i];
    if (foot->phase == TFTF_CREATURE_FOOT_SWING)
    {
      if (dt > 0.0f)
      {
        foot->swing_elapsed = fminf(foot->swing_duration,
            foot->swing_elapsed + dt);
      }
      const float phase = s_clamp(foot->swing_elapsed /
          fmaxf(1e-4f, foot->swing_duration), 0.0f, 1.0f);
      const float move_phase = phase;
      Vec3 tip = vec3_lerp(foot->swing_start_world,
          foot->swing_target_world, move_phase);
      tip.y += swing_height * sinf(TFTF_CREATURE_PI * move_phase);
      if (vec3_len2(vec3_sub(tip, roots_world[i])) > reach * reach)
      {
        foot->phase = TFTF_CREATURE_FOOT_AIRBORNE;
        foot->current_world = s_foot_reach_limit(
            roots_world[i], tip, reach);
      }
      else if (phase >= 1.0f)
      {
        foot->planted_world = foot->swing_target_world;
        foot->current_world = foot->planted_world;
        foot->phase = TFTF_CREATURE_FOOT_PLANTED;
      }
      else
      {
        foot->current_world = tip;
      }
    }
    if (foot->phase != TFTF_CREATURE_FOOT_PLANTED)
    {
      ++unsupported;
    }
  }

  /* Start steps when supports fall behind their ideal location. The ring
   * order is preserved among eligible feet, but no single Swing stalls the
   * rest. Airborne feet count against the unsupported-foot budget. */
  if (walking && dt > 0.0f && locomotion_speed > 0.0f)
  {
    ins->step_clock = fminf(cadence_interval, ins->step_clock + dt);
    const u32 max_unsupported = count > 1u ? count / 2u : 1u;
    if (unsupported < max_unsupported &&
        ins->step_clock >= cadence_interval)
    {
      u32 candidate = UINT32_MAX;
      u32 next_cursor = ins->next_step_serial;
      for (u32 attempt = 0u; attempt < count; ++attempt)
      {
        const u32 cursor = ins->next_step_serial + attempt;
        const u32 index = s_gait_order_index(count, cursor);
        TFTFCreatureFootState *foot = &ins->feet[index];
        if (foot->phase != TFTF_CREATURE_FOOT_PLANTED)
        {
          continue;
        }
        Vec3 support_error = vec3_sub(ideals[index], foot->planted_world);
        support_error.y = 0.0f;
        if (vec3_len2(support_error) >
            trigger_distance * trigger_distance)
        {
          candidate = index;
          next_cursor = cursor + 1u;
          break;
        }
      }
      if (candidate != UINT32_MAX)
      {
        TFTFCreatureFootState *foot = &ins->feet[candidate];
        /* Estimate the landing time, then project the support ahead of the
         * moving body. This is a distance in world units, not one frame. */
        Vec3 displacement = vec3_sub(ideals[candidate],
            foot->planted_world);
        displacement.y = 0.0f;
        const float rough_distance = vec3_len(displacement);
        const float arc = TFTF_CREATURE_PI * swing_height;
        const float rough_duration = fmaxf(0.0f,
            sqrtf(rough_distance * rough_distance + arc * arc) / foot_speed);
        Vec3 lead = vec3_mul(world_velocity, rough_duration * .85f +
            cadence_interval * .25f);
        const float lead_distance = vec3_len(lead);
        if (lead_distance > stride * .75f && lead_distance > 1e-6f)
        {
          lead = vec3_mul(lead, stride * .75f / lead_distance);
        }
        Vec3 goal = vec3_add(ideals[candidate], lead);
        goal.y = ground_height;
        const float safe_reach = reach * .95f;
        const float height = ground_height - roots_world[candidate].y;
        if (fabsf(height) >= safe_reach)
        {
          foot->phase = TFTF_CREATURE_FOOT_AIRBORNE;
          foot->current_world = s_foot_reach_limit(
              roots_world[candidate], foot->planted_world, reach);
        }
        else
        {
          Vec3 stride_delta = vec3_sub(goal, foot->planted_world);
          stride_delta.y = 0.0f;
          const float stride_distance = vec3_len(stride_delta);
          const float max_stride = stride * 1.7f;
          if (stride_distance > max_stride && stride_distance > 1e-6f)
          {
            goal = vec3_add(foot->planted_world,
                vec3_mul(stride_delta, max_stride / stride_distance));
            goal.y = ground_height;
          }
          /* Reach is the final constraint: limiting the stride afterward
           * could put the landing point outside the leg's physical reach. */
          const float horizontal_reach = sqrtf(fmaxf(0.0f,
              safe_reach * safe_reach - height * height));
          Vec3 horizontal = vec3_sub(goal, roots_world[candidate]);
          horizontal.y = 0.0f;
          const float horizontal_distance = vec3_len(horizontal);
          if (horizontal_distance > horizontal_reach &&
              horizontal_distance > 1e-6f)
          {
            goal = vec3_add(roots_world[candidate], vec3_mul(horizontal,
                horizontal_reach / horizontal_distance));
            goal.y = ground_height;
          }
          /* Never start a visually empty step. */
          if (vec3_len2(vec3_sub(goal, foot->planted_world)) > .0001f)
          {
            const float length = vec3_len(vec3_sub(
                goal, foot->planted_world));
            foot->swing_duration = fmaxf(.08f,
                sqrtf(length * length + arc * arc) / foot_speed);
            foot->swing_start_world = foot->planted_world;
            foot->swing_target_world = goal;
            foot->swing_elapsed = 0.0f;
            foot->current_world = foot->planted_world;
            foot->phase = TFTF_CREATURE_FOOT_SWING;
          }
        }
        ins->next_step_serial = next_cursor;
        ins->step_clock = 0.0f;
      }
    }
  }

  for (u32 i = 0; i < count; ++i)
  {
    TFTFCreatureFootState *foot = &ins->feet[i];
    Vec3 tip_world = foot->phase == TFTF_CREATURE_FOOT_PLANTED
        ? foot->planted_world : foot->current_world;
    contacts[i] = mat4_mul_point(inverse_world, tip_world);
  }

  for (u32 i = 0; i < count; ++i)
  {
    Vec3 root = roots[i];
    Vec3 tip = contacts[i];
    Vec3 path[16];
    float radii[16], flats[16];
    if (c->appendage_type == TFTF_CREATURE_APPENDAGE_LEG)
    {
      const Vec3 displacement = vec3_sub(tip, root);
      const float distance = vec3_len(displacement);
      const Vec3 bone = s_safe_norm(displacement, vec3_make(0, -1, 0));
      /* A radial knee bends along its own spoke, not along +/-X for every
       * leg. The old fixed pole made the front/back legs cross diagonally. */
      Vec3 pole = vec3_add(outwards[i],
          vec3_mul(up, c->appendage_knee));
      pole = vec3_sub(pole, vec3_mul(bone, vec3_dot(pole, bone)));
      pole = s_safe_norm(pole, vec3_make(0, 1, 0));
      const float segment = fmaxf(.1f, c->appendage_length);
      const float extension = sqrtf(fmaxf(0.0f,
          segment * segment - .25f * distance * distance));
      const Vec3 knee = vec3_add(vec3_lerp(root, tip, .5f),
          vec3_mul(pole, extension));
      path[0] = root;
      path[1] = vec3_lerp(root, knee, .5f);
      path[2] = knee;
      path[3] = vec3_lerp(knee, tip, .5f);
      path[4] = tip;
      for (u32 j = 0; j < 5u; ++j)
      {
        radii[j] = fmaxf(.004f, c->appendage_thickness *
            (1.0f - (1.0f - c->appendage_tip_taper) * (float)j / 4.0f));
        flats[j] = 1.0f;
      }
      if (!s_skin(&ins->parts[TFTF_CREATURE_PART_APPENDAGE], path,
          radii, flats, NULL, 5u, c->radial_segments, c->appendage_color))
      {
        return false;
      }
    }
  }
  return true;
}

static void s_part_release(LDKRenderer *renderer,TFTFCreatureGeometry *g)
{
  if(renderer)
  {
    if(ldk_renderer_mesh_is_valid(renderer,g->mesh))
      ldk_renderer_mesh_destroy(renderer,g->mesh);
    ldk_renderer_material_destroy(renderer,g->material);
    ldk_renderer_image_release(renderer,g->texture);
    ldk_renderer_image_release(renderer,g->normal_map);
    ldk_renderer_image_release(renderer,g->specular_map);
  }
  free(g->vertices);
  free(g->indices);
  memset(g,0,sizeof(*g));
}

static void s_restore_node_transforms(TFTFCreatureInstance *ins)
{
  for (u32 i = 0u; i < ins->rest_count; ++i)
  {
    if (!ldk_ecs_component_get_const(ins->rest_entities[i],
            ldk_component_type(TFTFCreatureNodeComponent))) continue;
    (void)ldk_transform_set_local_position(
        ins->rest_entities[i], ins->rest_positions[i]);
    (void)ldk_transform_set_local_rotation(
        ins->rest_entities[i], ins->rest_rotations[i]);
  }
  ins->rest_count = 0u;
}

static void s_instance_release(LDKRenderer *renderer,TFTFCreatureInstance *ins)
{
  s_restore_node_transforms(ins);
  for(u32 j=0;j<TFTF_CREATURE_PART_COUNT;++j)s_part_release(renderer,&ins->parts[j]);
}

static bool s_part_material(TFTFCreatureGeometry *g,LDKRenderer *renderer,
    LDKAssetManager *assets, LDKAssetMaterial asset)
{
  const LDKAssetMaterialData *data=assets?
      ldk_asset_manager_material_get_const(assets,asset):NULL;
  u32 revision=data?data->revision:0u;
  bool same=g->asset.h.index==asset.h.index&&
      g->asset.h.version==asset.h.version;
  if(same&&g->asset_revision==revision&&
      ldk_renderer_material_is_valid(renderer,g->material))return true;
  LDKMaterialDesc fallback;
  if(!ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR,&fallback))return false;
  fallback.args.vertex_color.color=0xffffffffu;
  fallback.surface.specular=.55f;
  fallback.surface.shininess=60.0f;
  if(!ldk_renderer_material_resolve(renderer,assets,
      data?&data->descriptor:&fallback,&g->material,&g->texture,
      &g->normal_map,&g->specular_map))return false;
  g->asset=asset;
  g->asset_revision=revision;
  return true;
}

static bool s_part_upload(LDKRenderer *renderer,TFTFCreatureGeometry *g)
{
  if(!g->vertex_count||!g->index_count)return true;
  s_normals(g);
  LDKRendererMeshDesc desc={0};
  desc.vertices=g->vertices;
  desc.vertex_count=g->vertex_count;
  desc.indices=g->indices;
  desc.index_count=g->index_count;
  desc.has_tangents=true;
  if(ldk_renderer_mesh_is_valid(renderer,g->mesh))
    return ldk_renderer_mesh_update(renderer,g->mesh,&desc);
  g->mesh=ldk_renderer_mesh_create(renderer,&desc);
  return ldk_renderer_mesh_is_valid(renderer,g->mesh);
}

static TFTFCreatureInstance *s_instance_get(
    TFTFCreatureInstancePool *pool, LDKEntity entity)
{
  for (u32 i = 0; i < pool->count; ++i)
  {
    if (pool->instances[i].entity.index == entity.index &&
        pool->instances[i].entity.version == entity.version)
    {
      return &pool->instances[i];
    }
  }
  if (pool->count == pool->capacity)
  {
    u32 next = pool->capacity ? pool->capacity * 2u : 16u;
    TFTFCreatureInstance *items = realloc(pool->instances,
        (size_t)next * sizeof(*items));
    if (!items)
    {
      return NULL;
    }
    pool->instances = items;
    pool->capacity = next;
  }
  TFTFCreatureInstance *ins = &pool->instances[pool->count++];
  memset(ins, 0, sizeof(*ins));
  ins->entity = entity;
  return ins;
}

static void s_instance_pool_clear(
    TFTFCreatureInstancePool *pool, LDKRenderer *renderer)
{
  for (u32 i = 0u; i < pool->count; ++i)
  {
    s_instance_release(renderer, &pool->instances[i]);
  }
  free(pool->instances);
  *pool = (TFTFCreatureInstancePool){0};
}

int tftf_creature_system_initialize(void *data)
{
  (void)data;
  /* Preview objects must not survive the transition into Play. */
  s_instance_pool_clear(&s_editor_pool,
      ldk_module_get(LDK_MODULE_RENDERER));
  return 0;
}

static void s_render_creature_group(const LDKEntityGroup *group,
    float dt, bool preview, TFTFCreatureInstancePool *pool)
{
  if(!group)return;
  LDKRenderer *renderer=ldk_module_get(LDK_MODULE_RENDERER);
  LDKAssetManager *assets=ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if(!renderer)return;
  for (u32 i = 0; i < pool->count; ++i) pool->instances[i].seen = false;
  for(u32 k=0;k<group->count;++k)
  {
    LDKEntity entity=group->entities[k];
    const TFTFCreatureComponent *source = ldk_ecs_component_get_const(
        entity, ldk_component_type(TFTFCreatureComponent));
    if (!source)
    {
      continue;
    }
    TFTFCreatureInstance *ins = s_instance_get(pool, entity);
    if (!ins) continue;
    TFTFCreatureComponent resolved = *source;
    LDKEntity body_entities[TFTF_CREATURE_MAX_NODES] = {0};
    s_resolve_body_nodes(entity, &resolved, body_entities);
    const TFTFCreatureComponent *c = &resolved;
    if(!c->enabled||c->node_count<2u||
       c->node_count>TFTF_CREATURE_MAX_NODES||c->radial_segments<4u||
       c->radial_segments>16u||c->segments_per_node<1u||
       c->segments_per_node>10u)continue;

    ins->seen = true;
    if (!preview)
    {
      bool topology_changed = ins->rest_count != c->node_count;
      for (u32 j = 0u; j < c->node_count && !topology_changed; ++j)
      {
        topology_changed = ins->rest_entities[j].index !=
                body_entities[j].index ||
            ins->rest_entities[j].version != body_entities[j].version;
      }
      if (topology_changed)
      {
        /* New play session or new anatomical topology. */
        ins->rest_count = c->node_count;
        for (u32 j = 0u; j < c->node_count; ++j)
        {
          ins->rest_entities[j] = body_entities[j];
          ins->rest_positions[j] = c->nodes[j].position;
          const LDKTransform *tf = ldk_ecs_component_get_const(
              body_entities[j], LDK_COMPONENT_TYPE_TRANSFORM);
          ins->rest_rotations[j] = tf ? tf->local_rotation :
              (Quat){0, 0, 0, 1};
        }
      }
      else
      {
        for (u32 j = 0u; j < c->node_count; ++j)
          resolved.nodes[j].position = ins->rest_positions[j];
      }
    }
    else
    {
      for (u32 j = 0u; j < c->node_count; ++j)
      {
        const LDKTransform *tf = ldk_ecs_component_get_const(
            body_entities[j], LDK_COMPONENT_TYPE_TRANSFORM);
        ins->rest_rotations[j] = tf ? tf->local_rotation :
            (Quat){0, 0, 0, 1};
      }
    }
    /* Edit preview never mutates transforms or advances the game simulation. */
    const float update_dt = (preview || ldk_game_instance_is_paused()) ? 0.0f :
        (isfinite(dt) && dt > 0.0f && dt < .25f ? dt : 0.0f);
    if (c->motion == TFTF_CREATURE_MOTION_WALK && update_dt > 0.0f &&
        c->movement_speed > 0.0f)
    {
      Vec3 local_position;
      if (ldk_transform_get_local_position(entity, &local_position))
      {
        (void)ldk_scenegraph_update_entity(entity);
        Mat4 current_world = mat4_identity();
        if (ldk_transform_get_world_matrix(entity, &current_world))
        {
          Vec3 world_forward = s_safe_norm(mat4_mul_dir(
              current_world, vec3_make(0, 0, -1)),
              vec3_make(0, 0, -1));
          Vec3 world_step = vec3_mul(world_forward,
              update_dt * c->movement_speed);
          Vec3 parent_step = world_step;
          const LDKEntity parent = ldk_transform_get_parent(entity);
          if (!x_handle_is_null(parent))
          {
            Mat4 parent_world;
            if (ldk_transform_get_world_matrix(parent, &parent_world))
            {
              parent_step = mat4_mul_dir(
                  mat4_inverse_affine(parent_world), world_step);
            }
          }
          (void)ldk_transform_set_local_position(entity,
              vec3_add(local_position, parent_step));
        }
      }
    }
    (void)ldk_scenegraph_update_entity(entity);
    Mat4 world=mat4_identity();
    (void)ldk_transform_get_world_matrix(entity,&world);
    bool config_changed = !ins->has_config ||
        memcmp(&ins->last_config, c, sizeof(*c)) != 0;
    bool world_changed = !ins->has_world ||
        memcmp(&ins->last_world, &world, sizeof(world)) != 0;
    bool is_animating = update_dt > 0.0f &&
        (c->motion != TFTF_CREATURE_MOTION_IDLE || c->body_pulse != 0.0f ||
         c->body_sway != 0.0f);
    /* Detect edits to any nested chain even while the creature is idle.
     * Hash a rest-space representation; runtime movement is tracked by
     * is_animating and does not change the authored chain signature. */
    TFTFCreatureResolvedChain rest_chains[TFTF_CREATURE_MAX_CHAINS] = {0};
    u32 rest_chain_count = 0u;
    u64 chain_signature = 14695981039346656037ull;
    for (u32 j = 0u; j < c->node_count; ++j)
    {
      if (x_handle_is_null(body_entities[j])) continue;
      const LDKTransform *tf = ldk_ecs_component_get_const(
          body_entities[j], LDK_COMPONENT_TYPE_TRANSFORM);
      if (!tf) continue;
      const Mat4 rest_matrix = mat4_compose(c->nodes[j].position,
          ins->rest_rotations[j], tf->local_scale);
      s_collect_chains_from_node(body_entities[j], rest_matrix,
          rest_chains, &rest_chain_count, 0u, &chain_signature);
    }
    chain_signature = s_chain_hash(chain_signature,
        &rest_chain_count, sizeof(rest_chain_count));
    const bool chains_changed = !ins->has_chain_signature ||
        ins->last_chain_signature != chain_signature;
    bool geometry_updated = config_changed || is_animating || chains_changed ||
        (world_changed && c->appendage_count != 0u &&
            c->appendage_type != TFTF_CREATURE_APPENDAGE_NONE);
    Vec3 nodes[TFTF_CREATURE_MAX_NODES] = {0};
    if (geometry_updated)
    {
      if (!ins->has_config ||
          ins->last_config.appendage_count != c->appendage_count ||
          ins->last_config.appendage_type != c->appendage_type ||
          ins->last_config.appendage_distribution !=
              c->appendage_distribution ||
          ins->last_config.appendage_spacing != c->appendage_spacing ||
          ins->last_config.appendage_root_offset !=
              c->appendage_root_offset ||
          ins->last_config.appendage_reach != c->appendage_reach ||
          ins->last_config.appendage_length != c->appendage_length ||
          ins->last_config.body_axis != c->body_axis ||
          ins->last_config.node_count != c->node_count ||
          memcmp(ins->last_config.nodes, c->nodes,
              sizeof(c->nodes)) != 0)
      {
        memset(ins->feet, 0, sizeof(ins->feet));
        ins->step_clock = 0.0f;
        ins->next_step_serial = 0u;
      }
      const float animation_dt =
          update_dt * fmaxf(0.0f, c->animation_speed);
      ins->time += animation_dt;
      if (c->motion == TFTF_CREATURE_MOTION_STING)
      {
        if (ins->has_config &&
            ins->last_config.motion != TFTF_CREATURE_MOTION_STING)
        {
          ins->sting_phase_time = 0.0f;
          ins->sting_phase = TFTF_CREATURE_STING_FORWARD;
          ins->sting_target_valid = false;
        }
        s_advance_sting(c, ins, animation_dt, world, preview,
            config_changed);
      }
      else
      {
        ins->sting_phase_time = 0.0f;
        ins->sting_phase = TFTF_CREATURE_STING_FORWARD;
        ins->sting_target_valid = false;
      }
      for(u32 j=0;j<TFTF_CREATURE_PART_COUNT;++j)
      {
        ins->parts[j].vertex_count=0;
        ins->parts[j].index_count=0;
      }
      s_animate_body(c,ins,nodes);
      Vec3 path[TFTF_CREATURE_MAX_PATH];
      Vec3 sizes[TFTF_CREATURE_MAX_PATH];
      u32 path_count = 0u;
      bool ok = s_body_path(c, nodes, path, sizes, &path_count);
      if (ok)
      {
        for (u32 j = 0u; j < path_count; ++j)
        {
          const float pulse = fmaxf(.05f, 1.0f + c->body_pulse *
              sinf(ins->time * c->body_pulse_speed -
                  (float)j * c->body_pulse_waves));
          sizes[j] = vec3_mul(sizes[j], pulse);
        }
        ok = s_skin(&ins->parts[TFTF_CREATURE_PART_BODY], path,
            NULL, NULL, sizes, path_count, c->radial_segments,
            c->body_color);
      }
      /* Leg physics uses real frame time; only actual movement_speed
       * determines the locomotion rate. */
      if (ok) ok = s_appendages(c, ins, nodes, world, update_dt);
      if (ok)
      {
        TFTFCreatureResolvedChain chains[TFTF_CREATURE_MAX_CHAINS] = {0};
        u32 chain_count = 0u;
        u64 signature = 14695981039346656037ull;
        for (u32 j = 0u; j < c->node_count; ++j)
        {
          const LDKTransform *tf = ldk_ecs_component_get_const(
              body_entities[j], LDK_COMPONENT_TYPE_TRANSFORM);
          if (!tf) continue;
          const Vec3 rest_direction = s_safe_norm(vec3_sub(
              c->nodes[j + 1u < c->node_count ? j + 1u : j].position,
              c->nodes[j > 0u ? j - 1u : j].position),
              vec3_make(0, 1, 0));
          const Vec3 moved_direction = s_safe_norm(vec3_sub(
              nodes[j + 1u < c->node_count ? j + 1u : j],
              nodes[j > 0u ? j - 1u : j]), rest_direction);
          const Quat rot = quat_mul(quat_from_to(rest_direction,
              moved_direction), ins->rest_rotations[j]);
          const Mat4 node_matrix = mat4_compose(nodes[j], rot,
              tf->local_scale);
          s_collect_chains_from_node(body_entities[j], node_matrix,
              chains, &chain_count, 0u, &signature);
        }
        ok = s_render_chains(c, ins, chains, chain_count);
      }
      if(!ok)continue;
    }
    bool upload_ok = true;
    const LDKAssetMaterial materials[TFTF_CREATURE_PART_COUNT]={
        c->body_material,c->appendage_material};
    for(u32 j=0;j<TFTF_CREATURE_PART_COUNT;++j)
    {
      TFTFCreatureGeometry *g=&ins->parts[j];
      if(!g->vertex_count||!g->index_count)continue;
      if (geometry_updated && !s_part_upload(renderer, g))
      {
        upload_ok = false; /* Retry on the next frame. */
        continue;
      }
      if (!s_part_material(g, renderer, assets, materials[j]))
      {
        continue;
      }
      (void)ldk_renderer_submit_mesh(renderer, g->mesh, g->material, world);
    }
    if (!preview && geometry_updated)
    {
      for (u32 j = 0u; j < c->node_count; ++j)
      {
        if (x_handle_is_null(body_entities[j])) continue;
        const Vec3 rest_dir = s_safe_norm(vec3_sub(
            c->nodes[j + 1u < c->node_count ? j + 1u : j].position,
            c->nodes[j > 0u ? j - 1u : j].position),
            vec3_make(0, 1, 0));
        const Vec3 pose_dir = s_safe_norm(vec3_sub(
            nodes[j + 1u < c->node_count ? j + 1u : j],
            nodes[j > 0u ? j - 1u : j]), rest_dir);
        const Quat rotated = quat_mul(quat_from_to(rest_dir, pose_dir),
            ins->rest_rotations[j]);
        (void)ldk_transform_set_local_position(body_entities[j], nodes[j]);
        (void)ldk_transform_set_local_rotation(body_entities[j], rotated);
      }
      (void)ldk_scenegraph_update_entity(entity);
    }
    if (upload_ok && geometry_updated)
    {
      ins->last_config = *c;
      ins->has_config = true;
      ins->last_world = world;
      ins->has_world = true;
      ins->last_chain_signature = chain_signature;
      ins->has_chain_signature = true;
    }
  }
  /* Destroy resources when an entity disappears from the queried group. */
  for (u32 i = 0u; i < pool->count;)
  {
    if (pool->instances[i].seen)
    {
      ++i;
      continue;
    }
    s_instance_release(renderer, &pool->instances[i]);
    pool->instances[i] = pool->instances[--pool->count];
  }
}


void tftf_creature_system_update(void *data,
    const LDKEntityGroup *group, float dt)
{
  (void)data;
  s_render_creature_group(group, dt, false, &s_runtime_pool);
}

#define TFTF_EDITOR_PREVIEW_CAPACITY 1024u

typedef struct TFTFCreaturePreviewGroup
{
  LDKEntity entities[TFTF_EDITOR_PREVIEW_CAPACITY];
  u32 count;
  bool overflow;
} TFTFCreaturePreviewGroup;

static bool s_editor_preview_collect(LDKEntity entity, void *user)
{
  TFTFCreaturePreviewGroup *group = user;
  if (!ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureComponent)))
  {
    return true;
  }
  if (group->count == TFTF_EDITOR_PREVIEW_CAPACITY)
  {
    group->overflow = true;
    return false;
  }
  group->entities[group->count++] = entity;
  return true;
}

/* Independent editor cache and renderer pass. Never calls the runtime
 * CreatureSystem entry, and never advances feet or creature transforms. */
void tftf_creature_editor_preview_update(void)
{
  TFTFCreaturePreviewGroup collection = {0};
  (void)ldk_ecs_entity_foreach(s_editor_preview_collect, &collection);
  if (collection.overflow)
  {
    return;
  }
  LDKEntityGroup group = {0};
  group.entities = collection.entities;
  group.count = collection.count;
  s_render_creature_group(&group, 0.0f, true, &s_editor_pool);
}

void tftf_creature_system_terminate(void *data)
{
  (void)data;
  LDKRenderer *renderer = ldk_module_get(LDK_MODULE_RENDERER);
  s_instance_pool_clear(&s_runtime_pool, renderer);
  s_instance_pool_clear(&s_editor_pool, renderer);
}
