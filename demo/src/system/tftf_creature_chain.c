#include "tftf_creature_chain.h"

#include <math.h>
#include <string.h>

#define TFTF_CHAIN_PI 3.14159265358979323846f

static float s_clamp(float v, float lo, float hi)
{
  return fminf(hi, fmaxf(lo, v));
}

static Vec3 s_normal(Vec3 v, Vec3 fallback)
{
  if (vec3_len2(v) > 1e-12f)
  {
    return vec3_norm(v);
  }
  return vec3_len2(fallback) > 1e-12f ? vec3_norm(fallback) :
      vec3_make(0.0f, 1.0f, 0.0f);
}

/* Keep the current folding plane when possible. This is not a limb pole:
 * it only disambiguates a perfectly straight or reversed chain. */
static Vec3 s_constrain_bend(Vec3 before, Vec3 pivot, Vec3 next,
    Vec3 reference, float min_degrees, float max_degrees, float length)
{
  const Vec3 prior = s_normal(vec3_sub(pivot, before), reference);
  const Vec3 wanted = s_normal(vec3_sub(next, pivot), prior);
  const float angle = acosf(s_clamp(vec3_dot(prior, wanted), -1.0f, 1.0f));
  const float minimum = s_clamp(min_degrees, 0.0f, 175.0f) *
      (TFTF_CHAIN_PI / 180.0f);
  const float maximum = s_clamp(max_degrees, min_degrees, 175.0f) *
      (TFTF_CHAIN_PI / 180.0f);
  const float limited = s_clamp(angle, minimum, maximum);
  if (fabsf(limited - angle) < 1e-6f)
  {
    return vec3_add(pivot, vec3_mul(wanted, length));
  }
  Vec3 perpendicular = vec3_sub(wanted,
      vec3_mul(prior, vec3_dot(wanted, prior)));
  if (vec3_len2(perpendicular) < 1e-10f)
  {
    perpendicular = vec3_sub(reference,
        vec3_mul(prior, vec3_dot(reference, prior)));
  }
  if (vec3_len2(perpendicular) < 1e-10f)
  {
    const Vec3 guide = fabsf(prior.y) < 0.85f ?
        vec3_make(0, 1, 0) : vec3_make(0, 0, 1);
    perpendicular = vec3_sub(guide,
        vec3_mul(prior, vec3_dot(guide, prior)));
  }
  perpendicular = s_normal(perpendicular, vec3_make(1, 0, 0));
  const Vec3 projected = vec3_add(vec3_mul(prior, cosf(limited)),
      vec3_mul(perpendicular, sinf(limited)));
  return vec3_add(pivot, vec3_mul(s_normal(projected, prior), length));
}

void tftf_creature_chain_build(TFTFCreatureChain *chain,
    const TFTFCreatureNode *nodes, u32 count)
{
  memset(chain, 0, sizeof(*chain));
  if (count < 2u || count > TFTF_CREATURE_MAX_NODES)
  {
    return;
  }
  chain->count = count;
  for (u32 i = 0u; i < count; ++i)
  {
    chain->rest[i] = nodes[i].position;
    chain->bend_min[i] = s_clamp(nodes[i].min_bend_degrees, 0, 175);
    chain->bend_max[i] = s_clamp(nodes[i].max_bend_degrees,
        chain->bend_min[i], 175);
    if (i > 0u)
    {
      chain->length[i - 1u] = fmaxf(0.0001f,
          vec3_len(vec3_sub(nodes[i].position, nodes[i - 1u].position)));
    }
  }
}

void tftf_creature_chain_rest_pose(const TFTFCreatureChain *chain,
    TFTFCreaturePose *pose)
{
  memcpy(pose->nodes, chain->rest,
      (size_t)chain->count * sizeof(*pose->nodes));
}

float tftf_creature_chain_solve(const TFTFCreatureChain *chain,
    TFTFCreaturePose *pose, Vec3 target, u32 moving_endpoint)
{
  const u32 count = chain->count;
  if (count < 2u || count > TFTF_CREATURE_MAX_NODES ||
      (moving_endpoint != 0u && moving_endpoint != count - 1u))
  {
    return INFINITY;
  }
  const bool reverse = moving_endpoint == 0u;
  Vec3 p[TFTF_CREATURE_MAX_NODES];
  Vec3 rest[TFTF_CREATURE_MAX_NODES];
  float lengths[TFTF_CREATURE_MAX_NODES - 1u];
  float min_bend[TFTF_CREATURE_MAX_NODES];
  float max_bend[TFTF_CREATURE_MAX_NODES];
  for (u32 i = 0u; i < count; ++i)
  {
    const u32 src = reverse ? count - 1u - i : i;
    p[i] = pose->nodes[src];
    rest[i] = chain->rest[src];
    min_bend[i] = chain->bend_min[src];
    max_bend[i] = chain->bend_max[src];
    if (i + 1u < count)
    {
      const u32 seg = reverse ? count - 2u - i : i;
      lengths[i] = chain->length[seg];
    }
  }
  const Vec3 anchor = p[0];
  const u32 last = count - 1u;
  /* Fall back to rest-pose direction when a numerical degeneracy occurs. */
  for (u32 iteration = 0u; iteration < 90u; ++iteration)
  {
    p[last] = target;
    for (u32 i = last; i > 0u; --i)
    {
      const Vec3 fallback = vec3_sub(rest[i - 1u], rest[i]);
      const Vec3 dir = s_normal(vec3_sub(p[i - 1u], p[i]), fallback);
      Vec3 next = vec3_add(p[i], vec3_mul(dir, lengths[i - 1u]));
      if (i < last)
      {
        const Vec3 reference = vec3_sub(rest[i - 1u], rest[i]);
        next = s_constrain_bend(p[i + 1u], p[i], next,
            reference, min_bend[i], max_bend[i], lengths[i - 1u]);
      }
      p[i - 1u] = next;
    }
    p[0] = anchor;
    for (u32 i = 1u; i <= last; ++i)
    {
      const Vec3 fallback = vec3_sub(rest[i], rest[i - 1u]);
      const Vec3 dir = s_normal(vec3_sub(p[i], p[i - 1u]), fallback);
      Vec3 next = vec3_add(p[i - 1u], vec3_mul(dir, lengths[i - 1u]));
      if (i > 1u)
      {
        const Vec3 reference = vec3_sub(rest[i], rest[i - 1u]);
        next = s_constrain_bend(p[i - 2u], p[i - 1u], next,
            reference, min_bend[i - 1u], max_bend[i - 1u],
            lengths[i - 1u]);
      }
      p[i] = next;
    }
    if (vec3_len2(vec3_sub(p[last], target)) < 9e-8f)
    {
      break;
    }
  }
  for (u32 i = 0u; i < count; ++i)
  {
    pose->nodes[reverse ? count - 1u - i : i] = p[i];
  }
  return vec3_len(vec3_sub(p[last], target));
}
