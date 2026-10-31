/*
 * TFTF island generator v1.3 core (independent C11 module).
 * The approved v1.0 baseline is stored independently and is unchanged.
 *
 * No LDK/STDX dependency. Geographical generation, ecology, relief and
 * access validation are shared by the game and the standalone test client.
 *
 * Public entrypoint: tftf_island_generator_generate().
 * The area is measured in world coordinates on a separate sampling domain.
 * Each image is fitted to the complete generated silhouette, with ocean
 * margins, so long islands are not cropped by the PPM canvas.
 * Height samples use sea level = 0. Geography coordinates are normalized.
 * Tile-to-geography calibration is explicit, not inferred from PPM pixels.
 */

#include "tftf_island_generator.h"

#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISLAND_MAP_SIZE 600u
#define ISLAND_AREA_SAMPLE_SIZE 768u
#define ISLAND_AREA_SAMPLE_HALF_EXTENT 2.5f
#define ISLAND_VIEW_MARGIN_RATIO 0.14f
#define ISLAND_LAND_SCALE 0.0097f
#define ISLAND_LAND_OCTAVES 5u
#define ISLAND_LAND_NOISE_STRENGTH 0.75f
#define ISLAND_BRANCHING 0.0f
#define ISLAND_LOOPING 0.0f
#define ISLAND_MAX_NODES 32u
#define ISLAND_PI 3.14159265358979323846f
#define ISLAND_DEFAULT_AREA_PERCENT 33.0f
/* Exploratory world-space distances/heights: sea level is 0. */
#define ISLAND_BEACH_WIDTH 0.036f
#define ISLAND_COAST_PROFILE_WIDTH 0.32f
#define ISLAND_SHALLOW_DEPTH_LIMIT 0.055f
#define ISLAND_SEA_LEVEL_GRAY 128.0f
#define ISLAND_HEIGHT_GRAY_SCALE 600.0f
/* Provisional world-space obstacle parameters; not engine movement units. */
#define ISLAND_WALKABLE_SLOPE 0.95f
/* Natural inland escarpments: elongated, noise-warped steep terrain
 * with a gradual backside, not necessarily an elevated plateau.
 * Height units remain the standalone tester's provisional world units. */
#define ISLAND_FORMATION_MAX_COUNT 3u
#define ISLAND_FORMATION_MAX_TRIALS 44u
#define ISLAND_FORMATION_MIN_INTERIOR_DISTANCE 0.056f
#define ISLAND_FORMATION_COAST_GUARD_START 0.024f
#define ISLAND_FORMATION_COAST_GUARD_END 0.078f
#define ISLAND_FORMATION_MIN_UPLIFT_CELLS 160u
#define ISLAND_FORMATION_MIN_STEEP_EDGES 35u
#define ISLAND_FORMATION_MIN_MAX_UPLIFT 0.065f
#define ISLAND_FORMATION_MIN_FACE_LENGTH 0.075f
#define ISLAND_FORMATION_RAMP_MIN_WIDTH 0.19f
#define ISLAND_FORMATION_CLIFF_WIDTH 0.0055f
/* A short, walkable corridor is carved only when a ridge isolates land.
 * These are RASTER sample widths; they are not LDK terrain tile meters. */
#define ISLAND_FORMATION_CORRIDOR_MAX_COUNT 4u
#define ISLAND_FORMATION_CORRIDOR_REPAIR_TRIALS 24u
#define ISLAND_FORMATION_CORRIDOR_MAX_TRIES 18u
#define ISLAND_FORMATION_CORRIDOR_RUNUP 22u
#define ISLAND_FORMATION_CORRIDOR_RUNOUT 44u
#define ISLAND_FORMATION_CORRIDOR_CORE 1.5f
#define ISLAND_FORMATION_CORRIDOR_FEATHER 3.5f
/* Safety bound for the optional, local post-process access carving. */
#define ISLAND_ACCESS_REPAIR_MAX_STEPS 128u
/* Optional satellite islands. The image raster is for visualization only:
 * pixels must not be mistaken for 1m terrain tiles. This standalone tester
 * needs an explicit geography-to-meters scale until it is integrated. */
#define ISLAND_MAX_SATELLITES 2u
#define ISLAND_DEFAULT_MIN_GAP_TILES 3.0f
#define ISLAND_DEFAULT_MAX_GAP_TILES 5.0f
#define ISLAND_DEFAULT_TILE_SIZE_METERS 1.0f
#define ISLAND_DEFAULT_METERS_PER_WORLD_UNIT 200.0f
#define ISLAND_SATELLITE_PLACEMENT_MARGIN 0.90f
#define ISLAND_SATELLITE_PLACEMENT_TRIES 180u

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;

typedef struct IslandGeographyNode
{
  float x;
  float y;
  float direction;
  float radius;
  i32 parent;
  u32 children;
  u32 depth;
} IslandGeographyNode;

typedef struct IslandGeography
{
  IslandGeographyNode nodes[ISLAND_MAX_NODES];
  u32 count;
  u32 branch_count;
  u32 max_depth;
  float tendency_chain;
  float tendency_turn;
} IslandGeography;

static u32 s_hash_u32(u32 x)
{
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static u32 s_hash_2d_i32(int x, int y, u32 seed)
{
  u32 h = seed;
  h ^= s_hash_u32((u32)x + 0x9e3779b9u);
  h ^= s_hash_u32((u32)y + 0x85ebca6bu);
  return s_hash_u32(h);
}

static float s_hash_2d_rand01(int x, int y, u32 seed)
{
  u32 h = s_hash_2d_i32(x, y, seed);
  return (float)(h >> 8) * (1.0f / 16777215.0f);
}

static float s_lerp_f32(float a, float b, float t)
{
  return a + (b - a) * t;
}

static float s_smoothstep_f32(float t)
{
  return t * t * (3.0f - 2.0f * t);
}

static float s_value_noise_2d(float x, float y, u32 seed)
{
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  int x1 = x0 + 1;
  int y1 = y0 + 1;
  float tx = x - (float)x0;
  float ty = y - (float)y0;
  float a;
  float b;
  float c;
  float d;
  float top;
  float bottom;

  tx = s_smoothstep_f32(tx);
  ty = s_smoothstep_f32(ty);
  a = s_hash_2d_rand01(x0, y0, seed);
  b = s_hash_2d_rand01(x1, y0, seed);
  c = s_hash_2d_rand01(x0, y1, seed);
  d = s_hash_2d_rand01(x1, y1, seed);
  top = s_lerp_f32(a, b, tx);
  bottom = s_lerp_f32(c, d, tx);
  return s_lerp_f32(top, bottom, ty);
}

static float s_fbm_compose_2d(float x, float y, u32 seed, u32 octaves)
{
  float value = 0.0f;
  float amplitude = 1.0f;
  float frequency = 1.0f;
  float amplitude_sum = 0.0f;

  for (u32 i = 0u; i < octaves; ++i)
  {
    u32 octave_seed = seed + i * 1013u;
    value += s_value_noise_2d(
        x * frequency, y * frequency, octave_seed) * amplitude;
    amplitude_sum += amplitude;
    frequency *= 2.0f;
    amplitude *= 0.5f;
  }
  return amplitude_sum > 0.0f ? value / amplitude_sum : 0.0f;
}

static float s_island_geography_random(u32 seed, u32 index, u32 salt)
{
  return (float)(s_hash_u32(seed ^ s_hash_u32(index + salt)) >> 8) *
      (1.0f / 16777215.0f);
}

/*
 * The graph is grown recursively, without a fixed number of arms or a fixed
 * hub. Every room may extend the graph. Parent selection mixes recent tips
 * and older branches; each world seed chooses different global tendencies.
 * Candidate positions reject severe overlaps while allowing crooked arms.
 */
static u32 s_island_geography_parent_select(
    const IslandGeography *graph, u32 seed, u32 step, u32 trial,
    float chain, float branchiness)
{
  float total = 0.0f;
  float weights[ISLAND_MAX_NODES];
  float pick;

  if (s_island_geography_random(seed, step * 83u + trial, 211u) <
      chain * 0.38f && graph->nodes[graph->count - 1u].children < 3u)
    return graph->count - 1u;

  for (u32 i = 0u; i < graph->count; ++i)
  {
    const IslandGeographyNode *node = &graph->nodes[i];
    float age = (float)(graph->count - i - 1u);
    float recent = 1.0f / (1.0f + age * (0.02f + chain * 0.17f));
    float weight = (0.25f + recent * 2.0f) *
        (node->children == 0u ? (0.5f + chain * 2.3f)
                            : (0.5f + branchiness * 2.4f));
    if (node->children >= 3u)
      weight = 0.0f;
    else if (node->children == 2u)
      weight *= 0.18f;
    if (i == 0u)
      weight *= 0.55f;
    weights[i] = weight;
    total += weight;
  }

  pick = s_island_geography_random(seed, step * 71u + trial, 233u) * total;
  for (u32 i = 0u; i < graph->count; ++i)
  {
    pick -= weights[i];
    if (pick <= 0.0f && weights[i] > 0.0f)
      return i;
  }
  return graph->count - 1u;
}

static bool s_island_geography_node_clear(
    const IslandGeography *graph, u32 parent, float x, float y,
    float distance)
{
  float min_spacing = distance * 0.73f;
  float min_spacing2 = min_spacing * min_spacing;

  for (u32 i = 0u; i < graph->count; ++i)
  {
    if (i == parent)
      continue;
    float dx = graph->nodes[i].x - x;
    float dy = graph->nodes[i].y - y;
    if (dx * dx + dy * dy < min_spacing2)
      return false;
  }
  return true;
}

static void s_island_geography_build(
    u32 world_seed, float branching, float looping, IslandGeography *graph)
{
  const u32 seed = world_seed ^ 0x31415927u;
  float minimum_x = 0.0f, maximum_x = 0.0f;
  float minimum_y = 0.0f, maximum_y = 0.0f;
  float chain = 0.12f + 0.73f *
      s_island_geography_random(seed, 0u, 17u) - 0.12f * branching;
  float branchiness = 0.12f + 0.76f *
      s_island_geography_random(seed, 0u, 29u) + 0.12f * branching;
  float turning = 0.10f + 0.80f *
      s_island_geography_random(seed, 0u, 39u) + 0.10f * looping;
  float handedness = s_island_geography_random(seed, 0u, 41u) < 0.5f
      ? -1.0f : 1.0f;
  float orientation = 2.0f * ISLAND_PI *
      s_island_geography_random(seed, 0u, 59u);
  u32 desired_count = 19u +
      (s_hash_u32(seed ^ 0xCC556677u) % 11u);

  memset(graph, 0, sizeof(*graph));
  chain = fminf(0.97f, fmaxf(0.05f, chain));
  turning = fminf(1.0f, turning);
  graph->tendency_chain = chain;
  graph->tendency_turn = turning;
  graph->count = 1u;
  graph->nodes[0] = (IslandGeographyNode){
      0.0f, 0.0f, orientation, 0.19f, -1, 0u, 0u};

  for (u32 step = 1u; step < desired_count; ++step)
  {
    u32 chosen_parent = 0u;
    float chosen_x = 0.0f, chosen_y = 0.0f;
    float chosen_direction = 0.0f;
    bool placed = false;

    for (u32 trial = 0u; trial < 100u; ++trial)
    {
      u32 parent_index = s_island_geography_parent_select(
          graph, seed, step, trial, chain, branchiness);
      const IslandGeographyNode *parent = &graph->nodes[parent_index];
      float r0 = s_island_geography_random(seed, step * 131u + trial, 271u);
      float r1 = s_island_geography_random(seed, step * 139u + trial, 281u);
      float r2 = s_island_geography_random(seed, step * 149u + trial, 293u);
      float distance = 0.85f + 0.52f * r0;
      float angle;
      float x, y;

      if (parent->children == 0u)
      {
        /* Continue along the current branch, but allow persistent curves. */
        angle = parent->direction +
            (r1 - 0.5f) * (0.35f + turning * 1.65f) +
            handedness * turning * 0.20f;
      }
      else
      {
        /* Start a new branch, not necessarily at the root of the tree. */
        float sign = r1 < 0.5f ? -1.0f : 1.0f;
        angle = parent->direction +
            sign * (0.75f + 1.60f * r2) +
            (r0 - 0.5f) * turning * 0.65f;
      }
      x = parent->x + cosf(angle) * distance;
      y = parent->y + sinf(angle) * distance;
      if (!s_island_geography_node_clear(graph, parent_index,
              x, y, distance))
        continue;
      chosen_parent = parent_index;
      chosen_direction = angle;
      chosen_x = x;
      chosen_y = y;
      placed = true;
      break;
    }

    if (!placed)
      break;

    {
      IslandGeographyNode *node = &graph->nodes[graph->count];
      node->x = chosen_x;
      node->y = chosen_y;
      node->direction = chosen_direction;
      node->parent = (i32)chosen_parent;
      node->radius = 0.14f + 0.12f *
          s_island_geography_random(seed, graph->count, 131u);
      node->children = 0u;
      node->depth = graph->nodes[chosen_parent].depth + 1u;
      graph->nodes[chosen_parent].children++;
      graph->max_depth = node->depth > graph->max_depth
          ? node->depth : graph->max_depth;
      graph->count++;
    }
    minimum_x = fminf(minimum_x, chosen_x);
    maximum_x = fmaxf(maximum_x, chosen_x);
    minimum_y = fminf(minimum_y, chosen_y);
    maximum_y = fmaxf(maximum_y, chosen_y);
  }

  for (u32 i = 0u; i < graph->count; ++i)
    if (graph->nodes[i].children >= 2u)
      graph->branch_count++;

  {
    float span = fmaxf(maximum_x - minimum_x, maximum_y - minimum_y);
    float scale = span > 1e-5f ? 1.82f / span : 1.0f;
    float cx = (maximum_x + minimum_x) * 0.5f;
    float cy = (maximum_y + minimum_y) * 0.5f;
    for (u32 i = 0u; i < graph->count; ++i)
    {
      graph->nodes[i].x = (graph->nodes[i].x - cx) * scale;
      graph->nodes[i].y = (graph->nodes[i].y - cy) * scale;
    }
  }
}

static float s_island_geography_field(
    const IslandGeography *graph, float nx, float ny)
{
  float field = -100.0f;

  for (u32 i = 0u; i < graph->count; ++i)
  {
    const IslandGeographyNode *node = &graph->nodes[i];
    float dx;
    float dy;
    float radius = node->radius;
    float distance;
    const IslandGeographyNode *parent;
    float ex;
    float ey;
    float length_squared;
    float t;

    if (node->parent < 0)
      continue;
    parent = &graph->nodes[node->parent];
    ex = node->x - parent->x;
    ey = node->y - parent->y;
    length_squared = ex * ex + ey * ey;
    t = length_squared > 1e-7f
        ? ((nx - parent->x) * ex + (ny - parent->y) * ey) /
              length_squared : 0.0f;
    t = fminf(1.0f, fmaxf(0.0f, t));
    dx = nx - (parent->x + t * ex);
    dy = ny - (parent->y + t * ey);
    radius = s_lerp_f32(parent->radius, node->radius, t);
    distance = sqrtf(dx * dx + dy * dy);
    field = fmaxf(field, (radius - distance) * 2.8f);
  }

  return field;
}

static float s_island_geography_threshold(
    const float *values, u64 count, float target_fraction)
{
  float low = INFINITY;
  float high = -INFINITY;

  for (u64 i = 0u; i < count; ++i)
  {
    low = fminf(low, values[i]);
    high = fmaxf(high, values[i]);
  }

  for (u32 pass = 0u; pass < 24u; ++pass)
  {
    float mid = (low + high) * 0.5f;
    u64 above = 0u;
    for (u64 i = 0u; i < count; ++i)
      if (values[i] > mid)
        ++above;
    if ((double)above / (double)count > (double)target_fraction)
      low = mid;
    else
      high = mid;
  }
  return (low + high) * 0.5f;
}

/* Sample the same world-space Land field regardless of the output viewport. */
static float s_island_geography_value(
    const IslandGeography *graph, u32 seed, float world_x, float world_y)
{
  const float noise_x = (world_x + 1.0f) *
      (float)ISLAND_MAP_SIZE * 0.5f * ISLAND_LAND_SCALE;
  const float noise_y = (world_y + 1.0f) *
      (float)ISLAND_MAP_SIZE * 0.5f * ISLAND_LAND_SCALE;
  const float noise = s_fbm_compose_2d(
      noise_x, noise_y, seed, ISLAND_LAND_OCTAVES);
  return s_island_geography_field(graph, world_x, world_y) +
      (noise - 0.5f) * ISLAND_LAND_NOISE_STRENGTH;
}

typedef struct IslandGeographyView
{
  float threshold;
  float center_x;
  float center_y;
  float span;
  float world_area;
} IslandGeographyView;

/*
 * Estimate the target land area in WORLD units, not as a fraction of the
 * output pixels. The initial sample covers a substantially larger space than
 * the graph; if terrain reaches its boundary, enlarge and retry rather than
 * silently clipping it. The fitted output view is computed only afterwards.
 */
static bool s_island_geography_view_build(
    const IslandGeography *graph, u32 seed, float target_world_area,
    IslandGeographyView *out_view)
{
  const u32 size = ISLAND_AREA_SAMPLE_SIZE;
  const u64 cell_count = (u64)size * size;
  float *values = (float *)malloc((size_t)cell_count * sizeof(*values));
  float half_extent = ISLAND_AREA_SAMPLE_HALF_EXTENT;
  bool result = false;

  if (!values || !out_view)
  {
    free(values);
    return false;
  }

  for (u32 attempt = 0u; attempt < 4u; ++attempt)
  {
    const float span = half_extent * 2.0f;
    const float cell_size = span / (float)size;
    const float fraction = target_world_area / (span * span);
    float threshold;
    u32 min_x = size, min_y = size, max_x = 0u, max_y = 0u;
    u64 land_count = 0u;
    bool touches_edge = false;

    if (fraction <= 0.0f || fraction >= 1.0f)
      break;

    for (u32 y = 0u; y < size; ++y)
    {
      const float world_y = -half_extent + ((float)y + 0.5f) * cell_size;
      for (u32 x = 0u; x < size; ++x)
      {
        const float world_x = -half_extent + ((float)x + 0.5f) * cell_size;
        values[(u64)y * size + x] =
            s_island_geography_value(graph, seed, world_x, world_y);
      }
    }

    threshold = s_island_geography_threshold(values, cell_count, fraction);
    for (u32 y = 0u; y < size; ++y)
    {
      for (u32 x = 0u; x < size; ++x)
      {
        if (values[(u64)y * size + x] <= threshold)
          continue;
        if (x == 0u || y == 0u || x == size - 1u || y == size - 1u)
          touches_edge = true;
        min_x = x < min_x ? x : min_x;
        min_y = y < min_y ? y : min_y;
        max_x = x > max_x ? x : max_x;
        max_y = y > max_y ? y : max_y;
        ++land_count;
      }
    }

    if (touches_edge)
    {
      half_extent *= 1.5f;
      continue;
    }
    if (land_count == 0u)
      break;

    {
      const float width = (float)(max_x - min_x + 1u) * cell_size;
      const float height = (float)(max_y - min_y + 1u) * cell_size;
      const float bounding_span = fmaxf(width, height);
      const float margin = fmaxf(0.10f,
          bounding_span * ISLAND_VIEW_MARGIN_RATIO);
      out_view->threshold = threshold;
      out_view->center_x = -half_extent +
          0.5f * (float)(min_x + max_x + 1u) * cell_size;
      out_view->center_y = -half_extent +
          0.5f * (float)(min_y + max_y + 1u) * cell_size;
      out_view->span = bounding_span + margin * 2.0f;
      out_view->world_area = (float)land_count * cell_size * cell_size;
    }
    result = true;
    break;
  }

  free(values);
  return result;
}

static float s_ecology_random(u32 seed, u32 salt);

/* A satellite is an independent, irregular, low-lying landmass. The
 * approved graph and main Land field are never modified. */
typedef struct IslandSatellite
{
  float x, y, radius_x, radius_y, angle;
  float gap;
  u8 depart_id;
  float depart_x, depart_y, arrive_x, arrive_y;
  u32 seed;
} IslandSatellite;

typedef struct IslandSatellites
{
  IslandSatellite islands[ISLAND_MAX_SATELLITES];
  u32 count;
} IslandSatellites;

/* All gap limits are configured in physical terrain tiles. The world scale
 * makes the conversion independent from the 600x600 debug-image resolution. */
typedef struct IslandSatelliteConfig
{
  float min_gap_tiles;
  float max_gap_tiles;
  float tile_size_meters;
  float meters_per_world_unit;
} IslandSatelliteConfig;

static float s_island_gap_world(float gap_tiles,
    const IslandSatelliteConfig *config)
{
  return gap_tiles * config->tile_size_meters /
      config->meters_per_world_unit;
}

static float s_island_world_x(const IslandGeographyView *v, u32 x)
{
  return v->center_x +
      (((float)x + 0.5f) / (float)ISLAND_MAP_SIZE - 0.5f) * v->span;
}

static float s_island_world_y(const IslandGeographyView *v, u32 y)
{
  return v->center_y +
      (((float)y + 0.5f) / (float)ISLAND_MAP_SIZE - 0.5f) * v->span;
}

static bool s_island_satellite_contains(const IslandSatellite *a,
    float wx, float wy)
{
  float dx = wx - a->x, dy = wy - a->y;
  const float c = cosf(a->angle), sn = sinf(a->angle);
  const float lx = c * dx + sn * dy;
  const float ly = -sn * dx + c * dy;
  const float normalized = sqrtf((lx / a->radius_x) * (lx / a->radius_x) +
      (ly / a->radius_y) * (ly / a->radius_y));
  const float broad = s_fbm_compose_2d(wx * 8.0f + 19.0f,
      wy * 8.0f - 11.0f, a->seed, 3u);
  const float fine = s_fbm_compose_2d(wx * 17.0f,
      wy * 17.0f, a->seed ^ 0x8A936B51u, 2u);
  const float outline = 1.0f + 0.32f * (broad - 0.5f) +
      0.10f * (fine - 0.5f);
  return normalized < outline;
}

/* 0 is sea, 1 is the ORIGINAL main island, 2-3 are satellites. */
static u8 s_island_land_id(const IslandGeography *graph, u32 seed,
    const IslandGeographyView *v, const IslandSatellites *satellites,
    float wx, float wy)
{
  if (s_island_geography_value(graph, seed, wx, wy) > v->threshold)
    return 1u;
  for (u32 i = 0u; i < satellites->count; ++i)
    if (s_island_satellite_contains(&satellites->islands[i], wx, wy))
      return (u8)(i + 2u);
  return 0u;
}

/* The placement raster is NOT the final output. It covers the original
 * silhouette plus a generous water margin, so satellites can originate
 * outside it. Coast pixel lookup and exact nearest-land gap measurement
 * ensure both separation and short visual/raft crossing distances. */
static bool s_island_satellites_build(const IslandGeography *graph,
    u32 seed, u32 variant, const IslandSatelliteConfig *config,
    IslandGeographyView *view, IslandSatellites *out)
{
  const u32 n = ISLAND_MAP_SIZE, count = n * n;
  IslandGeographyView placement = *view;
  IslandSatellites placed = {0};
  u8 *occupied = NULL;
  float *water_distance = NULL;
  u32 *nearest = NULL;
  u32 *shoreline = NULL;
  const float min_gap = s_island_gap_world(config->min_gap_tiles, config);
  const float max_gap = s_island_gap_world(config->max_gap_tiles, config);
  u32 desired = s_hash_u32(seed ^ 0xA24BD839u) % 3u;
  const u32 placement_seed = seed ^ s_hash_u32(variant * 0x4F0D39B1u);
  bool ok = false;
  u32 min_x = n, min_y = n, max_x = 0u, max_y = 0u;
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (!desired) return true;
  placement.span += ISLAND_SATELLITE_PLACEMENT_MARGIN;
  const float pixel = placement.span / (float)n;
  occupied = (u8 *)calloc(count, sizeof(*occupied));
  water_distance = (float *)malloc((size_t)count * sizeof(*water_distance));
  nearest = (u32 *)malloc((size_t)count * sizeof(*nearest));
  shoreline = (u32 *)malloc((size_t)count * sizeof(*shoreline));
  if (!occupied || !water_distance || !nearest || !shoreline) goto done;

  for (u32 y = 0u; y < n; ++y)
    for (u32 x = 0u; x < n; ++x)
      occupied[y * n + x] = (u8)(s_island_geography_value(
          graph, seed, s_island_world_x(&placement, x),
          s_island_world_y(&placement, y)) > view->threshold);

  for (u32 island = 0u; island < desired; ++island)
  {
    u32 coastline = 0u;
    bool accepted = false;
    /* Nearest-land sources use an exact Euclidean distance to the source
     * pixel, propagated by the same two-pass method as bathymetry. */
    for (u32 i = 0u; i < count; ++i)
    {
      nearest[i] = occupied[i] ? i : UINT32_MAX;
      water_distance[i] = occupied[i] ? 0.0f : INFINITY;
    }
    for (u32 y = 0u; y < n; ++y)
      for (u32 x = 0u; x < n; ++x)
      {
        const u32 at = y * n + x;
        if (!occupied[at]) continue;
        if ((x > 0u && !occupied[at - 1u]) ||
            (x + 1u < n && !occupied[at + 1u]) ||
            (y > 0u && !occupied[at - n]) ||
            (y + 1u < n && !occupied[at + n]))
          shoreline[coastline++] = at;
      }
    if (!coastline) goto done;
    /* Propagate existing-land locations so each proposed satellite's
     * nearest approach can be checked without an O(N^2) pairwise scan. */
#define RELAX_PLACEMENT(X,Y,SX,SY) do { \
      const u32 _at = (Y) * n + (X); \
      const u32 _source = nearest[(SY) * n + (SX)]; \
      if (_source != UINT32_MAX) { \
        float _dx = (float)(X) - (float)(_source % n); \
        float _dy = (float)(Y) - (float)(_source / n); \
        float _d2 = _dx * _dx + _dy * _dy; \
        if (_d2 < water_distance[_at]) { \
          water_distance[_at] = _d2; nearest[_at] = _source; \
        } \
      } \
    } while (0)
    for (u32 y = 0u; y < n; ++y)
      for (u32 x = 0u; x < n; ++x)
      {
        if (x) RELAX_PLACEMENT(x,y,x-1u,y);
        if (y)
        {
          RELAX_PLACEMENT(x,y,x,y-1u);
          if (x) RELAX_PLACEMENT(x,y,x-1u,y-1u);
          if (x+1u<n) RELAX_PLACEMENT(x,y,x+1u,y-1u);
        }
      }
    for (u32 y = n; y-- > 0u;)
      for (u32 x = n; x-- > 0u;)
      {
        if (x+1u<n) RELAX_PLACEMENT(x,y,x+1u,y);
        if (y+1u<n)
        {
          RELAX_PLACEMENT(x,y,x,y+1u);
          if (x) RELAX_PLACEMENT(x,y,x-1u,y+1u);
          if (x+1u<n) RELAX_PLACEMENT(x,y,x+1u,y+1u);
        }
      }
#undef RELAX_PLACEMENT
    for (u32 attempt = 0u; attempt < ISLAND_SATELLITE_PLACEMENT_TRIES;
        ++attempt)
    {
      const u32 pick = s_hash_u32(placement_seed ^ (island * 0xF128AD3Bu) ^
          (attempt * 0x94D049BBu)) % coastline;
      const u32 source = shoreline[pick];
      const u32 px = source % n, py = source / n;
      /* Choose a neighboring WATER direction from the actual shoreline. */
      i32 outward_x[4], outward_y[4];
      u32 options = 0u;
      if (px > 0u && !occupied[source - 1u])
        {outward_x[options] = -1; outward_y[options++] = 0;}
      if (px + 1u < n && !occupied[source + 1u])
        {outward_x[options] = 1; outward_y[options++] = 0;}
      if (py > 0u && !occupied[source - n])
        {outward_x[options] = 0; outward_y[options++] = -1;}
      if (py + 1u < n && !occupied[source + n])
        {outward_x[options] = 0; outward_y[options++] = 1;}
      if (!options) continue;
      const u32 pick_dir = s_hash_u32(placement_seed ^ attempt ^
          (island * 0x1926EA39u)) % options;
      float dx = (float)outward_x[pick_dir], dy =
          (float)outward_y[pick_dir];
      const float turn = (s_ecology_random(placement_seed ^ attempt,
          island * 0x51A893E3u) - 0.5f) * 0.65f;
      const float cs = cosf(turn), sn = sinf(turn);
      const float ndx = dx*cs - dy*sn, ndy = dx*sn + dy*cs;
      IslandSatellite sat = {0};
      const float radius_rand = s_ecology_random(placement_seed ^ attempt,
          island * 0x3A196741u ^ 0xF1AB2D65u);
      const float gap_rand = s_ecology_random(placement_seed ^ attempt,
          island * 0xA6D22811u ^ 0xD7B5029Au);
      const float desired_gap = min_gap +
          (max_gap - min_gap) *
          (0.18f + 0.64f * gap_rand);
      sat.radius_x = 0.105f + 0.060f * radius_rand;
      sat.radius_y = sat.radius_x * (0.65f + 0.30f *
          s_ecology_random(placement_seed ^ (attempt*31u), island ^ 0xE291CCB4u));
      sat.angle = ISLAND_PI * 2.0f * s_ecology_random(placement_seed ^ attempt,
          island ^ 0xCB99D421u);
      sat.seed = s_hash_u32(placement_seed ^ (attempt * 0x376AD8B9u) ^
          (island * 0xBF16E313u) ^ 0x3D3BAA41u);
      /* Push outward by its expected radius plus a short crossing. */
      sat.x = s_island_world_x(&placement, px) +
          ndx * (sat.radius_x * 1.05f + desired_gap);
      sat.y = s_island_world_y(&placement, py) +
          ndy * (sat.radius_x * 1.05f + desired_gap);
      const float max_r = sat.radius_x * 1.30f;
      const i32 lo_x = (i32)floorf(((sat.x - max_r - placement.center_x) /
          placement.span + 0.5f) * n);
      const i32 hi_x = (i32)ceilf(((sat.x + max_r - placement.center_x) /
          placement.span + 0.5f) * n);
      const i32 lo_y = (i32)floorf(((sat.y - max_r - placement.center_y) /
          placement.span + 0.5f) * n);
      const i32 hi_y = (i32)ceilf(((sat.y + max_r - placement.center_y) /
          placement.span + 0.5f) * n);
      if (lo_x < 2 || lo_y < 2 || hi_x >= (i32)n - 2 ||
          hi_y >= (i32)n - 2) continue;
      float min_gap2 = INFINITY;
      u32 closest = UINT32_MAX, pixels = 0u;
      for (i32 y = lo_y; y <= hi_y; ++y)
        for (i32 x = lo_x; x <= hi_x; ++x)
        {
          const u32 at = (u32)y * n + (u32)x;
          if (!s_island_satellite_contains(&sat,
              s_island_world_x(&placement, (u32)x),
              s_island_world_y(&placement, (u32)y))) continue;
          ++pixels;
          if (water_distance[at] < min_gap2)
            {min_gap2 = water_distance[at]; closest = at;}
        }
      if (pixels < 190u || closest == UINT32_MAX) continue;
      sat.gap = sqrtf(min_gap2) * pixel;
      if (sat.gap < min_gap || sat.gap > max_gap) continue;
      const u32 coast_ref = nearest[closest];
      if (coast_ref == UINT32_MAX) continue;
      sat.depart_id = 1u;
      for (u32 prior = 0u; prior < placed.count; ++prior)
        if (s_island_satellite_contains(&placed.islands[prior],
            s_island_world_x(&placement, coast_ref % n),
            s_island_world_y(&placement, coast_ref / n)))
          sat.depart_id = (u8)(prior + 2u);
      sat.depart_x = s_island_world_x(&placement, coast_ref % n);
      sat.depart_y = s_island_world_y(&placement, coast_ref / n);
      sat.arrive_x = s_island_world_x(&placement, closest % n);
      sat.arrive_y = s_island_world_y(&placement, closest / n);
      /* Commit only after distance and visibility-in-plan checks. */
      for (i32 y = lo_y; y <= hi_y; ++y)
        for (i32 x = lo_x; x <= hi_x; ++x)
          if (s_island_satellite_contains(&sat,
              s_island_world_x(&placement, (u32)x),
              s_island_world_y(&placement, (u32)y)))
            occupied[(u32)y * n + (u32)x] = 1u;
      placed.islands[placed.count++] = sat;
      accepted = true;
      break;
    }
    if (!accepted) goto done;
  }
  /* Fit the WHOLE archipelago for visualization. Main-world coordinates
   * and original threshold stay untouched. */
  for (u32 y = 0u; y < n; ++y)
    for (u32 x = 0u; x < n; ++x)
      if (occupied[y * n + x])
      {
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
      }
  if (min_x == n) goto done;
  {
    const float bw = ((float)(max_x - min_x + 1u)) * pixel;
    const float bh = ((float)(max_y - min_y + 1u)) * pixel;
    const float bound = fmaxf(bw, bh);
    const float margin = fmaxf(0.10f,
        bound * ISLAND_VIEW_MARGIN_RATIO);
    view->center_x = placement.center_x +
        ((float)(min_x + max_x + 1u) * 0.5f / n - 0.5f) * placement.span;
    view->center_y = placement.center_y +
        ((float)(min_y + max_y + 1u) * 0.5f / n - 0.5f) * placement.span;
    view->span = bound + margin * 2.0f;
  }
  *out = placed;
  ok = true;
done:
  free(shoreline);
  free(nearest);
  free(water_distance);
  free(occupied);
  return ok;
}

/*
 * Ecological test layer. This code ONLY samples the already generated land
 * silhouette. It does not write to the graph, Land field or area threshold.
 * Sea level is zero here; the synthetic heights are for visual tests only.
 */
typedef enum IslandSurface
{
  ISLAND_SURFACE_DEEP_WATER = 0,
  ISLAND_SURFACE_SHALLOW_WATER,
  ISLAND_SURFACE_SAND,
  ISLAND_SURFACE_GRASS,
  ISLAND_SURFACE_DRY,
  ISLAND_SURFACE_FOREST,
  ISLAND_SURFACE_TALL_GRASS,
  ISLAND_SURFACE_THORNS,
  ISLAND_SURFACE_ROCK,
  ISLAND_SURFACE_COUNT
} IslandSurface;

typedef enum IslandCoast
{
  ISLAND_COAST_NONE = 0,
  ISLAND_COAST_BEACH,
  ISLAND_COAST_CLIFF,
  ISLAND_COAST_OVERGROWN,
  ISLAND_COAST_COUNT
} IslandCoast;

typedef struct IslandFormation
{
  float x, y;
  float radius;
  float angle;
  u32 seed;
  float height_offset;
  float radius_y;
  float rise;
  float entry_angle;
} IslandFormation;

typedef struct IslandEcology
{
  u8 *land;
  u8 *land_id;
  u8 *surface;
  u8 *coast;
  u8 *coast_profile;
  float *coast_suitability;
  float *height;
  float *shore_distance;
  float *water_distance;
  u32 *shore_source;
  float *moisture;
  float *thorn_score;
  float *tall_score;
  float *reference_height;
  u8 *reachable;
  u8 *formation_mask;
  u8 *formation_corridor_mask;
  u8 *formation_local_fix_mask;
  u8 *access_repair_mask;
  IslandFormation formations[ISLAND_FORMATION_MAX_COUNT];
  u32 formation_count;
  u32 formation_corridors;
  u32 formation_corridor_cells;
  u32 formation_local_fix_events;
  u32 formation_local_fix_cells;
  u32 attempts;
  u32 unreachable_land;
  u32 access_repair_steps;
  u32 access_repair_cells;
  u32 access_unreachable_before;
  float access_repair_max_change;
  u32 uplift_cells;
  float max_uplift;
  u64 coast_edges_total;
  u64 coast_edges_beach;
  u64 beach_edges_walkable;
  u64 cliff_edges_steep;
  u64 cliff_edges_total;
  float slope_world_span;
  u64 surface_count[ISLAND_SURFACE_COUNT];
  u64 coast_count[ISLAND_COAST_COUNT];
  u64 land_count;
} IslandEcology;

static float s_clamp01(float v)
{
  return fminf(1.0f, fmaxf(0.0f, v));
}

static float s_ecology_random(u32 seed, u32 salt)
{
  return (float)(s_hash_u32(seed ^ salt) >> 8) * (1.0f / 16777215.0f);
}

static bool s_ecology_allocate(IslandEcology *e)
{
  const size_t count = (size_t)ISLAND_MAP_SIZE * ISLAND_MAP_SIZE;
  memset(e, 0, sizeof(*e));
  e->land = (u8 *)calloc(count, sizeof(*e->land));
  e->land_id = (u8 *)calloc(count, sizeof(*e->land_id));
  e->surface = (u8 *)calloc(count, sizeof(*e->surface));
  e->coast = (u8 *)calloc(count, sizeof(*e->coast));
  e->coast_profile = (u8 *)calloc(count, sizeof(*e->coast_profile));
  e->coast_suitability = (float *)malloc(count * sizeof(*e->coast_suitability));
  e->height = (float *)malloc(count * sizeof(*e->height));
  e->shore_distance = (float *)malloc(count * sizeof(*e->shore_distance));
  e->water_distance = (float *)malloc(count * sizeof(*e->water_distance));
  e->shore_source = (u32 *)malloc(count * sizeof(*e->shore_source));
  e->moisture = (float *)malloc(count * sizeof(*e->moisture));
  e->thorn_score = (float *)malloc(count * sizeof(*e->thorn_score));
  e->tall_score = (float *)malloc(count * sizeof(*e->tall_score));
  e->reference_height = (float *)malloc(count * sizeof(*e->reference_height));
  e->reachable = (u8 *)calloc(count, sizeof(*e->reachable));
  e->formation_mask = (u8 *)calloc(count, sizeof(*e->formation_mask));
  e->formation_corridor_mask = (u8 *)calloc(count, sizeof(*e->formation_corridor_mask));
  e->formation_local_fix_mask = (u8 *)calloc(count, sizeof(*e->formation_local_fix_mask));
  e->access_repair_mask = (u8 *)calloc(count,
      sizeof(*e->access_repair_mask));
  return e->land && e->land_id && e->surface && e->coast && e->coast_profile &&
      e->coast_suitability && e->height && e->shore_distance &&
      e->water_distance && e->shore_source && e->moisture &&
      e->thorn_score && e->tall_score && e->reference_height &&
      e->reachable && e->formation_mask && e->formation_corridor_mask &&
      e->formation_local_fix_mask &&
      e->access_repair_mask;
}

static void s_ecology_free(IslandEcology *e)
{
  free(e->land);
  free(e->land_id);
  free(e->surface);
  free(e->coast);
  free(e->coast_profile);
  free(e->coast_suitability);
  free(e->height);
  free(e->shore_distance);
  free(e->water_distance);
  free(e->shore_source);
  free(e->moisture);
  free(e->thorn_score);
  free(e->tall_score);
  free(e->reference_height);
  free(e->reachable);
  free(e->formation_mask);
  free(e->formation_corridor_mask);
  free(e->formation_local_fix_mask);
  free(e->access_repair_mask);
  memset(e, 0, sizeof(*e));
}

/* The closest WATER cell propagates along eight-neighbor grid directions.
 * This yields a stable approximation to Euclidean shoreline distance and
 * gives each inland pixel a nearby shoreline coordinate for coast profiling. */
static void s_ecology_relax_shore(IslandEcology *e, u32 x, u32 y,
    u32 sx, u32 sy)
{
  const u32 size = ISLAND_MAP_SIZE;
  const u32 at = y * size + x;
  const u32 source = e->shore_source[sy * size + sx];
  if (source == UINT32_MAX)
    return;
  const float dx = (float)x - (float)(source % size);
  const float dy = (float)y - (float)(source / size);
  const float distance_squared = dx * dx + dy * dy;
  if (distance_squared < e->shore_distance[at])
  {
    e->shore_distance[at] = distance_squared;
    e->shore_source[at] = source;
  }
}

static void s_ecology_build_shore(IslandEcology *e)
{
  const u32 size = ISLAND_MAP_SIZE;
  for (u32 y = 0u; y < size; ++y)
    for (u32 x = 0u; x < size; ++x)
    {
      const u32 i = y * size + x;
      e->shore_source[i] = e->land[i] ? UINT32_MAX : i;
      e->shore_distance[i] = e->land[i] ? INFINITY : 0.0f;
    }

  for (u32 y = 0u; y < size; ++y)
    for (u32 x = 0u; x < size; ++x)
    {
      if (x > 0u) s_ecology_relax_shore(e, x, y, x - 1u, y);
      if (y > 0u)
      {
        s_ecology_relax_shore(e, x, y, x, y - 1u);
        if (x > 0u) s_ecology_relax_shore(e, x, y, x - 1u, y - 1u);
        if (x + 1u < size) s_ecology_relax_shore(e, x, y, x + 1u, y - 1u);
      }
    }
  for (u32 y = size; y-- > 0u;)
    for (u32 x = size; x-- > 0u;)
    {
      if (x + 1u < size) s_ecology_relax_shore(e, x, y, x + 1u, y);
      if (y + 1u < size)
      {
        s_ecology_relax_shore(e, x, y, x, y + 1u);
        if (x > 0u) s_ecology_relax_shore(e, x, y, x - 1u, y + 1u);
        if (x + 1u < size) s_ecology_relax_shore(e, x, y, x + 1u, y + 1u);
      }
    }
  for (u32 i = 0u; i < size * size; ++i)
    e->shore_distance[i] = sqrtf(e->shore_distance[i]);
}

/*
 * Once all land-side coastal profiles are classified, the nearest-source
 * scratch array is free to reuse for SEA pixels. Every LAND pixel becomes
 * a distance seed; ocean distance drives a smoothly descending seabed.
 * This is independent of the land shore distance, which stays untouched.
 */
static void s_ecology_relax_water(IslandEcology *e, u32 x, u32 y,
    u32 sx, u32 sy)
{
  const u32 size = ISLAND_MAP_SIZE;
  const u32 at = y * size + x;
  const u32 source = e->shore_source[sy * size + sx];
  if (source == UINT32_MAX)
    return;
  const float dx = (float)x - (float)(source % size);
  const float dy = (float)y - (float)(source / size);
  const float distance_squared = dx * dx + dy * dy;
  if (distance_squared < e->water_distance[at])
  {
    e->water_distance[at] = distance_squared;
    e->shore_source[at] = source;
  }
}

static void s_ecology_build_water_distance(IslandEcology *e)
{
  const u32 size = ISLAND_MAP_SIZE;
  for (u32 i = 0u; i < size * size; ++i)
  {
    e->shore_source[i] = e->land[i] ? i : UINT32_MAX;
    e->water_distance[i] = e->land[i] ? 0.0f : INFINITY;
  }
  for (u32 y = 0u; y < size; ++y)
    for (u32 x = 0u; x < size; ++x)
    {
      if (x > 0u) s_ecology_relax_water(e, x, y, x - 1u, y);
      if (y > 0u)
      {
        s_ecology_relax_water(e, x, y, x, y - 1u);
        if (x > 0u) s_ecology_relax_water(e, x, y, x - 1u, y - 1u);
        if (x + 1u < size) s_ecology_relax_water(e, x, y, x + 1u, y - 1u);
      }
    }
  for (u32 y = size; y-- > 0u;)
    for (u32 x = size; x-- > 0u;)
    {
      if (x + 1u < size) s_ecology_relax_water(e, x, y, x + 1u, y);
      if (y + 1u < size)
      {
        s_ecology_relax_water(e, x, y, x, y + 1u);
        if (x > 0u) s_ecology_relax_water(e, x, y, x - 1u, y + 1u);
        if (x + 1u < size) s_ecology_relax_water(e, x, y, x + 1u, y + 1u);
      }
    }
  for (u32 i = 0u; i < size * size; ++i)
    e->water_distance[i] = sqrtf(e->water_distance[i]);
}

/* Deterministic quantile threshold. Target fractions vary by seed, but the
 * gameplay-required thorn and tall-grass covers never vanish by chance. */
static float s_ecology_quantile(const float *scores,
    const u8 *land, const u8 *surface, u8 excluded,
    u32 count, float target_fraction)
{
  float low = INFINITY, high = -INFINITY;
  u32 candidates = 0u;
  for (u32 i = 0u; i < count; ++i)
  {
    if (!land[i] || surface[i] == ISLAND_SURFACE_SAND ||
        surface[i] == ISLAND_SURFACE_ROCK || surface[i] == excluded)
      continue;
    low = fminf(low, scores[i]);
    high = fmaxf(high, scores[i]);
    ++candidates;
  }
  if (!candidates)
    return INFINITY;
  for (u32 n = 0u; n < 22u; ++n)
  {
    float mid = 0.5f * (low + high);
    u32 above = 0u;
    for (u32 i = 0u; i < count; ++i)
      if (land[i] && surface[i] != ISLAND_SURFACE_SAND &&
          surface[i] != ISLAND_SURFACE_ROCK && surface[i] != excluded &&
          scores[i] > mid)
        ++above;
    if ((float)above / (float)candidates > target_fraction)
      low = mid;
    else
      high = mid;
  }
  return 0.5f * (low + high);
}

/* Four-neighbor coastline segments, each having one pixel_world length.
 * This avoids confusing coastline length with the area of the shore band. */
static u32 s_ecology_coast_edges(const IslandEcology *e, u32 x, u32 y)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 at = y * n + x;
  if (!e->land[at])
    return 0u;
  return (x == 0u || !e->land[at - 1u]) +
      (x + 1u == n || !e->land[at + 1u]) +
      (y == 0u || !e->land[at - n]) +
      (y + 1u == n || !e->land[at + n]);
}

/* A noise-warped escarpment crest, NOT a raised circular plateau.
 * The height rises gradually along the length of the ridge; across its
 * width one side is sharply cut off and the other gently descends.
 * This creates a natural detour toward the lower end of a barrier.
 * No changes to coastline, seabed, geology mask, or ecological rules. */
static float s_formation_height(const IslandFormation *f,
    float wx, float wy, float *coverage)
{
  const float dx = wx - f->x, dy = wy - f->y;
  const float c = cosf(f->angle), s = sinf(f->angle);
  const float u = (dx * c + dy * s) / f->radius;
  const float v = (-dx * s + dy * c) / f->radius_y;
  const float warp = s_fbm_compose_2d(
      u * 1.4f + 7.5f, v * 1.4f - 3.1f,
      f->seed ^ 0x71BB4E27u, 4u);
  const float warp2 = s_fbm_compose_2d(
      u * 3.1f - 4.2f, v * 3.1f + 5.8f,
      f->seed ^ 0x1C5C67B5u, 3u);
  const float bend = 0.36f * (warp - 0.5f) +
      0.08f * (warp2 - 0.5f);
  const float long_u = u + 0.09f * (warp2 - 0.5f);
  /* At the low end, the crest merges with the existing terrain.
   * The top gradually climbs, and its remote tip also slopes away. */
  const float rise_along = s_smoothstep_f32(s_clamp01(
      (long_u + 1.0f) / 1.55f));
  const float terminal = s_smoothstep_f32(s_clamp01(
      (1.0f - long_u) / 0.90f));
  /* Cross section: sharp front wall, broad gently declining backside.
   * A long narrow face is the obstacle, not a fully enclosed plateau. */
  const float front = s_smoothstep_f32(s_clamp01(
      (v - bend + 0.24f) / 0.043f));
  const float backside = 1.0f - s_smoothstep_f32(s_clamp01(
      (v - bend - 0.08f) / 1.38f));
  *coverage = rise_along * terminal * front * backside;
  const float detail = s_fbm_compose_2d(
      wx * 8.0f + 2.1f, wy * 8.0f - 6.1f,
      f->seed ^ 0x9F72A503u, 3u);
  return f->height_offset + f->rise +
      0.005f * (detail - 0.5f);
}

static void s_ecology_apply_formation(IslandEcology *e,
    const IslandGeographyView *view, const IslandFormation *f)
{
  const u32 n = ISLAND_MAP_SIZE;
  const float px = view->span / (float)n;
  const float half = fmaxf(f->radius, f->radius_y) * 1.35f;
  for (u32 y = 0u; y < n; ++y)
  {
    const float wy = view->center_y +
        (((float)y + 0.5f) / (float)n - 0.5f) * view->span;
    if (fabsf(wy - f->y) > half) continue;
    for (u32 x = 0u; x < n; ++x)
    {
      const u32 at = y * n + x;
      if (e->land_id[at] != 1u) continue;
      const float wx = view->center_x +
          (((float)x + 0.5f) / (float)n - 0.5f) * view->span;
      if (fabsf(wx - f->x) > half) continue;
      const float d = e->shore_distance[at] * px;
      if (d <= ISLAND_FORMATION_COAST_GUARD_START) continue;
      float cover = 0.0f;
      const float elevated = s_formation_height(f, wx, wy, &cover);
      if (cover <= 0.0f || elevated <= e->height[at]) continue;
      const float shore_guard = s_smoothstep_f32(s_clamp01(
          (d - ISLAND_FORMATION_COAST_GUARD_START) /
          (ISLAND_FORMATION_COAST_GUARD_END -
              ISLAND_FORMATION_COAST_GUARD_START)));
      e->height[at] += cover * shore_guard *
          (elevated - e->height[at]);
    }
  }
}

/* Walkability prototype: horizontal world units are the same units used by
 * the independent height generator. The real player thresholds are UNKNOWN
 * until we integrate with the controller. Eight neighbors, no corner cutting.
 * Shallow water is wadeable, deep water is not. */
static bool s_ecology_cell_walkable(const IslandEcology *e, u32 at)
{
  return e->land[at] ||
      e->surface[at] == ISLAND_SURFACE_SHALLOW_WATER;
}

static bool s_ecology_edge_walkable(const IslandEcology *e,
    u32 from, u32 to, float separation)
{
  if (!s_ecology_cell_walkable(e, to))
    return false;
  if (e->land[from] != e->land[to])
  {
    const u32 shore_cell = e->land[from] ? from : to;
    /* Shore accessibility is a separate categorical rule from land slope.
     * This ensures the quarter of coast declared cliff is not a landing,
     * including where quantization makes local gradients look gentle. */
    if (e->coast[shore_cell] != ISLAND_COAST_BEACH)
      return false;
  }
  return fabsf(e->height[from] - e->height[to]) <=
      ISLAND_WALKABLE_SLOPE * separation;
}

static bool s_ecology_check_access(IslandEcology *e, float world_pixel,
    u32 *out_unreachable)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 count = n * n;
  u32 *queue = (u32 *)malloc((size_t)count * sizeof(*queue));
  u32 start = UINT32_MAX, begin = 0u, end = 0u;
  if (!queue) return false;
  memset(e->reachable, 0, (size_t)count);
  /* A raft connects distinct landmasses. Ground traversal is therefore
   * validated from one beach on EACH island, not from a single beach for
   * the entire archipelago. Deep water stays non-wadeable. */
  for (u32 id = 1u; id <= ISLAND_MAX_SATELLITES + 1u; ++id)
  {
    start = UINT32_MAX;
    for (u32 y = 0u; y < n && start == UINT32_MAX; ++y)
      for (u32 x = 0u; x < n; ++x)
      {
        const u32 at = y * n + x;
        if (e->land_id[at] == id &&
            e->coast[at] == ISLAND_COAST_BEACH &&
            e->shore_distance[at] <= 1.5f)
        {
          start = at;
          break;
        }
      }
    if (start == UINT32_MAX)
    {
      bool present = false;
      for (u32 at = 0u; at < count; ++at)
        if (e->land_id[at] == id) {present = true; break;}
      if (present) {free(queue); return false;}
      continue;
    }
    e->reachable[start] = 1u;
    queue[end++] = start;
  }
  if (!end) {free(queue); return false;}
  while (begin < end)
  {
    const u32 from = queue[begin++];
    const u32 x = from % n, y = from / n;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (!dx && !dy) continue;
        const int nx = (int)x + dx, ny = (int)y + dy;
        if (nx < 0 || ny < 0 || nx >= (int)n || ny >= (int)n)
          continue;
        const u32 to = (u32)ny * n + (u32)nx;
        if (e->reachable[to] || !s_ecology_edge_walkable(e, from,
            to, world_pixel * (dx && dy ? 1.41421356f : 1.0f)))
          continue;
        if (dx && dy)
        {
          const u32 side_x = y * n + (u32)nx;
          const u32 side_y = (u32)ny * n + x;
          if (!s_ecology_edge_walkable(e, from, side_x, world_pixel) ||
              !s_ecology_edge_walkable(e, from, side_y, world_pixel))
            continue;
        }
        e->reachable[to] = 1u;
        queue[end++] = to;
      }
  }
  *out_unreachable = 0u;
  for (u32 i = 0u; i < count; ++i)
    if (e->land[i] && !e->reachable[i])
      ++*out_unreachable;
  free(queue);
  return true;
}

/* A validation failure is an opportunity to carve a small traversable
 * approach, rather than throw away the whole island. Only HEIGHT changes;
 * the approved land mask, coast classification and seabed remain untouched.
 *
 * Each iteration selects the least expensive blocked boundary edge between
 * already reachable terrain and an unreachable land cell. A modest height
 * adjustment at the unreachable end forges one guaranteed walkable step.
 * Unreachable neighbors receive a weaker adjustment to prevent an isolated
 * spike or pit. Re-flooding from the original beach ensures that the entire
 * result, not only the edited edge, is traversable. Existing reachable
 * heights are never modified: the accessible area grows monotonically.
 *
 * The categorical sea-to-land rule is preserved: a cliff never silently
 * becomes a beach. If no land/shallow-water frontier exists, a repair would
 * require altering the coastline or a deep-water passage; fail explicitly
 * rather than claim that the island is fully accessible. */
static bool s_ecology_repair_access(IslandEcology *e, float world_pixel)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 count = n * n;
  u32 unreachable = 0u;
  if (!s_ecology_check_access(e, world_pixel, &unreachable))
    return false;
  e->access_unreachable_before = unreachable;

  while (unreachable)
  {
    u32 best_from = UINT32_MAX;
    u32 best_to = UINT32_MAX;
    float best_cost = INFINITY;
    const float step_limit = ISLAND_WALKABLE_SLOPE * world_pixel;
    if (e->access_repair_steps >= ISLAND_ACCESS_REPAIR_MAX_STEPS)
      break;

    for (u32 at = 0u; at < count; ++at)
    {
      if (!e->reachable[at])
        continue;
      const u32 x = at % n, y = at / n;
      const u32 next[4] = {
        x > 0u ? at - 1u : at,
        x + 1u < n ? at + 1u : at,
        y > 0u ? at - n : at,
        y + 1u < n ? at + n : at
      };
      for (u32 k = 0u; k < 4u; ++k)
      {
        const u32 to = next[k];
        if (to == at || !e->land[to] || e->reachable[to])
          continue;
        /* Repairing a cliff's actual shoreline would undermine the
         * promised 60/40 coastline. Keep those cells untouched. */
        if (e->coast[to] != ISLAND_COAST_NONE &&
            s_ecology_coast_edges(e, to % n, to / n))
          continue;
        if (!e->land[at] && e->coast[to] != ISLAND_COAST_BEACH)
          continue;
        const float excess = fabsf(e->height[to] - e->height[at]) -
            step_limit;
        if (excess <= 0.0f)
          continue;
        /* Keep adjustments away from shore unless necessary. Their
         * magnitude still dominates the choice. */
        const float distance_to_coast = e->shore_distance[to] * world_pixel;
        const float coastal_penalty = 0.003f *
            (1.0f - s_clamp01(distance_to_coast / 0.060f));
        const float cost = excess + coastal_penalty;
        if (cost < best_cost)
        {
          best_cost = cost;
          best_from = at;
          best_to = to;
        }
      }
    }

    if (best_to == UINT32_MAX)
      break;
    {
      const float original = e->height[best_to];
      const float delta = e->height[best_from] - original;
      /* 85% of the allowed per-cell gradient gives a margin for
       * floating-point rounding in the subsequent flood fill. */
      const float permitted = 0.85f * step_limit;
      const float target = fmaxf(0.0005f,
          e->height[best_from] - fminf(permitted,
          fmaxf(-permitted, delta)));
      const float change = target - original;
      /* 'target' must be on the reachable side of the gradient limit:
       * reduce the cliff to a slope, without making a level trench. */
      e->height[best_to] = target;
      if (!e->access_repair_mask[best_to])
      {
        e->access_repair_mask[best_to] = 1u;
        ++e->access_repair_cells;
      }
      e->access_repair_max_change = fmaxf(
          e->access_repair_max_change, fabsf(change));
      const u32 cx = best_to % n, cy = best_to / n;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
          if (!dx && !dy) continue;
          const int nx = (int)cx + dx, ny = (int)cy + dy;
          if (nx < 0 || ny < 0 || nx >= (int)n || ny >= (int)n)
            continue;
          const u32 neighbor = (u32)ny * n + (u32)nx;
          if (!e->land[neighbor] || e->reachable[neighbor])
            continue;
          if (e->coast[neighbor] != ISLAND_COAST_NONE &&
              s_ecology_coast_edges(e, (u32)nx, (u32)ny))
            continue;
          const float change_neighbor = change *
              (dx && dy ? 0.19f : 0.32f);
          const float previous = e->height[neighbor];
          e->height[neighbor] = fmaxf(0.0005f,
              previous + change_neighbor);
          if (e->height[neighbor] != previous)
          {
            if (!e->access_repair_mask[neighbor])
            {
              e->access_repair_mask[neighbor] = 1u;
              ++e->access_repair_cells;
            }
            e->access_repair_max_change = fmaxf(
                e->access_repair_max_change,
                fabsf(e->height[neighbor] - previous));
          }
        }
    }
    ++e->access_repair_steps;
    if (!s_ecology_check_access(e, world_pixel, &unreachable))
      return false;
  }
  e->unreachable_land = unreachable;
  return unreachable == 0u;
}

/* A visible formation must have a substantial raised INTERIOR and a
 * traversability-blocking face. Requiring both prevents the old behavior
 * where accepted disks only painted a small bright spot at a coastal tip.
 * All measurements refer to current heights BEFORE/AFTER this formation. */
static bool s_ecology_formation_visible(const IslandEcology *e,
    const float *before, float world_pixel,
    u32 *out_raised, u32 *out_steep, float *out_highest)
{
  const u32 n = ISLAND_MAP_SIZE;
  u32 raised = 0u, steep = 0u;
  float highest = 0.0f;
  for (u32 y = 1u; y + 1u < n; ++y)
    for (u32 x = 1u; x + 1u < n; ++x)
    {
      const u32 at = y * n + x;
      if (e->land_id[at] != 1u ||
          e->shore_distance[at] * world_pixel <
              ISLAND_FORMATION_MIN_INTERIOR_DISTANCE)
        continue;
      const float uplift = e->height[at] - before[at];
      if (uplift > 0.015f) ++raised;
      highest = fmaxf(highest, uplift);
      if (uplift < 0.008f) continue;
      /* Both endpoints must be inland and at least one must be elevated by
       * this formation: pre-existing coastal cliffs do not qualify. */
      const u32 neighbors[2] = {at + 1u, at + n};
      for (u32 k = 0u; k < 2u; ++k)
      {
        const u32 to = neighbors[k];
        if (e->land_id[to] != 1u ||
            e->shore_distance[to] * world_pixel <
                ISLAND_FORMATION_MIN_INTERIOR_DISTANCE)
          continue;
        if (fabsf(e->height[to] - e->height[at]) >
            ISLAND_WALKABLE_SLOPE * world_pixel)
          ++steep;
      }
    }
  *out_raised = raised;
  *out_steep = steep;
  *out_highest = highest;
  return raised >= ISLAND_FORMATION_MIN_UPLIFT_CELLS &&
      steep >= ISLAND_FORMATION_MIN_STEEP_EDGES && highest >= ISLAND_FORMATION_MIN_MAX_UPLIFT;
}

/* An escarpment may block passage through an otherwise traversable path.
 * Instead of rejecting it, open a narrow graded corridor at the cheapest
 * blocked inland boundary, blending a walkable ramp between the two sides.
 * The ramp is rasterized exclusively on land with a shoreline safety band.
 * Its centerline is continuous; its flanks are feathered into the ridge.
 * Every candidate is tested with the same physical reachability checker
 * as the final island. Failures are rolled back byte-for-byte. */
typedef struct IslandCorridorCandidate
{
  u32 from, to;
  i32 dx, dy;
  float score;
} IslandCorridorCandidate;

static bool s_ecology_try_formation_corridor(IslandEcology *e,
    float world_pixel, const IslandCorridorCandidate *c,
    float *snapshot, u8 *candidate_mask, u32 *out_unreachable)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 count = n * n;
  const u32 id = e->land_id[c->from];
  const i32 fx = (i32)(c->from % n), fy = (i32)(c->from / n);
  const i32 tx = (i32)(c->to % n), ty = (i32)(c->to / n);
  u32 before = *out_unreachable;
  u32 pre = 0u, post = 0u;
  const float guard = ISLAND_FORMATION_COAST_GUARD_START + 0.009f;

  for (u32 k = 1u; k <= ISLAND_FORMATION_CORRIDOR_RUNUP; ++k)
  {
    const i32 x = fx - (i32)k * c->dx;
    const i32 y = fy - (i32)k * c->dy;
    if (x < 1 || y < 1 || x + 1 >= (i32)n || y + 1 >= (i32)n)
      break;
    const u32 at = (u32)y * n + (u32)x;
    if (e->land_id[at] != id || !e->reachable[at] ||
        e->shore_distance[at] * world_pixel < guard)
      break;
    pre = k;
  }
  for (u32 k = 1u; k <= ISLAND_FORMATION_CORRIDOR_RUNOUT; ++k)
  {
    const i32 x = tx + (i32)k * c->dx;
    const i32 y = ty + (i32)k * c->dy;
    if (x < 1 || y < 1 || x + 1 >= (i32)n || y + 1 >= (i32)n)
      break;
    const u32 at = (u32)y * n + (u32)x;
    if (e->land_id[at] != id || e->reachable[at] ||
        e->shore_distance[at] * world_pixel < guard)
      break;
    post = k;
  }
  if (pre < 2u || post < 2u)
    return false;

  const i32 sx = fx - (i32)pre * c->dx;
  const i32 sy = fy - (i32)pre * c->dy;
  const i32 ex = tx + (i32)post * c->dx;
  const i32 ey = ty + (i32)post * c->dy;
  const u32 steps = pre + post + 1u;
  const float a = e->height[(u32)sy * n + (u32)sx];
  const float b = e->height[(u32)ey * n + (u32)ex];
  if (fabsf(b - a) > steps * world_pixel *
      ISLAND_WALKABLE_SLOPE * 0.70f)
    return false;

  memcpy(snapshot, e->height, (size_t)count * sizeof(*snapshot));
  memset(candidate_mask, 0, (size_t)count);
  for (u32 k = 0u; k <= steps; ++k)
  {
    const i32 cx = sx + (i32)k * c->dx;
    const i32 cy = sy + (i32)k * c->dy;
    const float h = s_lerp_f32(a, b, (float)k / (float)steps);
    for (i32 off = -4; off <= 4; ++off)
    {
      const i32 x = cx - off * c->dy;
      const i32 y = cy + off * c->dx;
      if (x < 0 || y < 0 || x >= (i32)n || y >= (i32)n)
        continue;
      const u32 at = (u32)y * n + (u32)x;
      if (e->land_id[at] != id ||
          e->shore_distance[at] * world_pixel < guard)
        continue;
      const float d = fabsf((float)off);
      const float weight = 1.0f - s_smoothstep_f32(s_clamp01(
          (d - ISLAND_FORMATION_CORRIDOR_CORE) /
          (ISLAND_FORMATION_CORRIDOR_FEATHER -
              ISLAND_FORMATION_CORRIDOR_CORE)));
      const float new_height = s_lerp_f32(snapshot[at], h, weight);
      if (fabsf(new_height - snapshot[at]) > 0.00001f)
      {
        e->height[at] = fmaxf(0.0005f, new_height);
        candidate_mask[at] = 1u;
      }
    }
  }
  u32 after = 0u;
  if (!s_ecology_check_access(e, world_pixel, &after) || after >= before)
  {
    memcpy(e->height, snapshot, (size_t)count * sizeof(*snapshot));
    if (!s_ecology_check_access(e, world_pixel, out_unreachable))
      return false;
    return false;
  }
  *out_unreachable = after;
  return true;
}

static bool s_ecology_repair_formation_access(IslandEcology *e,
    const float *before, float world_pixel, u8 *pending_mask,
    u32 *out_corridors)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 count = n * n;
  float *snapshot = (float *)malloc((size_t)count * sizeof(*snapshot));
  u8 *trial_mask = (u8 *)calloc((size_t)count, 1u);
  if (!snapshot || !trial_mask)
  {
    free(snapshot); free(trial_mask);
    return false;
  }
  bool success = false;
  u32 missing = 0u;
  *out_corridors = 0u;
  if (!s_ecology_check_access(e, world_pixel, &missing))
    goto done;
  while (missing && *out_corridors < ISLAND_FORMATION_CORRIDOR_MAX_COUNT)
  {
    IslandCorridorCandidate candidates[ISLAND_FORMATION_CORRIDOR_MAX_TRIES];
    u32 candidate_count = 0u;
    for (u32 y = 1u; y + 1u < n; ++y)
      for (u32 x = 1u; x + 1u < n; ++x)
      {
        const u32 at = y * n + x;
        if (!e->reachable[at] || !e->land[at] ||
            e->shore_distance[at] * world_pixel <
                (ISLAND_FORMATION_COAST_GUARD_START + 0.018f))
          continue;
        const u32 neighbors[4] = {at - 1u, at + 1u, at - n, at + n};
        const i32 dxs[4] = {-1, 1, 0, 0};
        const i32 dys[4] = {0, 0, -1, 1};
        for (u32 dir = 0u; dir < 4u; ++dir)
        {
          const u32 to = neighbors[dir];
          if (!e->land[to] || e->land_id[to] != e->land_id[at] ||
              e->reachable[to] ||
              e->shore_distance[to] * world_pixel <
                  (ISLAND_FORMATION_COAST_GUARD_START + 0.018f))
            continue;
          const float diff = fabsf(e->height[to] - e->height[at]);
          if (diff <= ISLAND_WALKABLE_SLOPE * world_pixel)
            continue;
          const float uplift = fmaxf(e->height[at] - before[at],
              e->height[to] - before[to]);
          if (uplift < 0.006f)
            continue;
          const float score = diff + 0.0015f /
              fmaxf(0.02f, e->shore_distance[to] * world_pixel);
          u32 place = candidate_count;
          while (place > 0u && score < candidates[place - 1u].score)
            --place;
          if (place >= ISLAND_FORMATION_CORRIDOR_MAX_TRIES)
            continue;
          if (candidate_count < ISLAND_FORMATION_CORRIDOR_MAX_TRIES)
            ++candidate_count;
          for (u32 t = candidate_count - 1u; t > place; --t)
            candidates[t] = candidates[t - 1u];
          candidates[place] = (IslandCorridorCandidate){
            at, to, dxs[dir], dys[dir], score};
        }
      }
    bool repaired = false;
    for (u32 k = 0u; k < candidate_count; ++k)
    {
      if (!s_ecology_try_formation_corridor(e, world_pixel,
          &candidates[k], snapshot, trial_mask, &missing))
        continue;
      for (u32 at = 0u; at < count; ++at)
        pending_mask[at] |= trial_mask[at];
      ++*out_corridors;
      repaired = true;
      break;
    }
    if (!repaired)
      goto done;
  }
  success = missing == 0u;
done:
  free(snapshot); free(trial_mask);
  return success;
}

/* A nearly walkable crest sometimes develops isolated raster peaks from
 * the noise. Connect ONLY such high pixels to an already reached part of
 * the same formation, never cut a ramp through the front cliff or coast.
 * A genuine disconnected land region still needs a carved path. */
static bool s_ecology_repair_formation_crest(IslandEcology *e,
    const float *before, float world_pixel)
{
  const u32 n = ISLAND_MAP_SIZE, count = n * n;
  const float step = world_pixel * ISLAND_WALKABLE_SLOPE * 0.82f;
  u32 missing = 0u;
  for (u32 pass = 0u; pass < 60u; ++pass)
  {
    if (!s_ecology_check_access(e, world_pixel, &missing)) return false;
    if (!missing) return true;
    u32 adjusted = 0u;
    for (u32 at = n + 1u; at + n + 1u < count; ++at)
    {
      if (e->reachable[at] || e->land_id[at] != 1u ||
          e->height[at] - before[at] < 0.012f ||
          e->shore_distance[at] * world_pixel <
              ISLAND_FORMATION_MIN_INTERIOR_DISTANCE) continue;
      const u32 x=at%n;
      if (x==0u || x+1u==n) continue;
      const u32 near[4]={at-1u,at+1u,at-n,at+n};
      float best_change=INFINITY, best_height=e->height[at];
      for (u32 dir=0u;dir<4u;++dir)
      {
        const u32 to=near[dir];
        if (!e->reachable[to] || e->land_id[to]!=1u ||
            e->height[to]-before[to] < 0.020f) continue;
        const float original=e->height[at];
        const float h=fmaxf(before[at], fmaxf(e->height[to]-step,
            fminf(e->height[to]+step, original)));
        if (fabsf(original-h)>0.00001f &&
            fabsf(original-h)<best_change)
        {best_height=h; best_change=fabsf(original-h);}
      }
      if (best_change<INFINITY)
      {e->height[at]=best_height; ++adjusted;}
    }
    if(!adjusted) break;
  }
  if (!s_ecology_check_access(e, world_pixel, &missing)) return false;
  return missing==0u;
}

/* Select truly inland anchors. Candidate escarpments have steep faces
 * and accessible space around their ends, rather than requiring a plateau. Any disconnected land is repaired with a traversable
 * passage; no terrain, shoreline, or biome changes outside this stage. */
static bool s_ecology_generate_formations(IslandEcology *e,
    const IslandGeographyView *view, u32 seed)
{
  const u32 n = ISLAND_MAP_SIZE, count = n * n;
  const float pixel = view->span / (float)n;
  const u32 target = (s_hash_u32(seed ^ 0x17C3E922u) % ISLAND_FORMATION_MAX_COUNT);
  float *original_height = (float *)malloc((size_t)count * sizeof(float));
  u8 *pending_mask = (u8 *)calloc((size_t)count, 1u);
  u32 unreachable = 0u;
  if (!original_height || !pending_mask)
  {
    free(original_height); free(pending_mask);
    return false;
  }
  memcpy(original_height, e->height, (size_t)count * sizeof(float));
  if (!s_ecology_check_access(e, pixel, &unreachable) || unreachable)
  {
    free(original_height); free(pending_mask);
    return false;
  }

  for (u32 formation = 0u; formation < target; ++formation)
  {
    /* Try randomized anchors; favor wide inland terrain but permit the
     * noise-deformed footprint to cross the shoreline or an inlet. */
    for (u32 trial = 0u; trial < ISLAND_FORMATION_MAX_TRIALS; ++trial)
    {
      const u32 salt = formation * 0xD56A37B9u + trial * 0x9E3779B9u;
      IslandFormation f = {0};
      f.seed = s_hash_u32(seed ^ salt ^ 0xAB62193Fu);
      const float shore_min = ISLAND_FORMATION_MIN_INTERIOR_DISTANCE;
      u32 best_at = UINT32_MAX;
      float best_score = -INFINITY;
      for (u32 k = 0u; k < 28u; ++k)
      {
        const u32 at = s_hash_u32(f.seed ^
            (k * 0x85EBCA6Bu + 0x31A27C15u)) % count;
        if (e->land_id[at] != 1u) continue;
        const float d = e->shore_distance[at] * pixel;
        if (d < shore_min) continue;
        /* Prefer the deepest parts of the main island, while keeping
         * sampling stochastic across branches and across seeds. */
        const float score = fminf(d, 0.18f) +
            0.12f * s_ecology_random(f.seed, 0x189A732Du ^ k);
        if (score > best_score)
        {
          best_score = score;
          best_at = at;
        }
      }
      if (best_at == UINT32_MAX) continue;
      f.x = view->center_x +
          (((float)(best_at % n) + 0.5f) / (float)n - 0.5f) * view->span;
      f.y = view->center_y +
          (((float)(best_at / n) + 0.5f) / (float)n - 0.5f) * view->span;
      f.radius = 0.19f + 0.11f *
          s_ecology_random(f.seed, 0x62A56D33u);
      f.radius_y = f.radius * (0.72f + 0.18f *
          s_ecology_random(f.seed, 0x99453F0Bu));
      f.angle = 2.0f * ISLAND_PI *
          s_ecology_random(f.seed, 0x815A7C2Du);
      f.entry_angle = 2.0f * ISLAND_PI *
          s_ecology_random(f.seed, 0x27AEFF01u);
      f.rise = 0.102f + 0.016f *
          s_ecology_random(f.seed, 0x8B152AA9u);
      f.height_offset = e->height[best_at];

      memcpy(e->reference_height, e->height,
          (size_t)count * sizeof(float));
      s_ecology_apply_formation(e, view, &f);
      ++e->attempts;
      u32 raised = 0u, steep = 0u;
      float highest = 0.0f;
      bool accepted = false;
      const bool visible = s_ecology_formation_visible(e,
          e->reference_height, pixel, &raised, &steep, &highest);
#ifdef ISLAND_DEBUG_FORMATIONS
      if (formation == 0u && trial < 12u)
        fprintf(stderr, "trial %u raised %u steep %u high %.4f visible %d\n",
            trial, raised, steep, highest, (int)visible);
#endif
      if (visible && s_ecology_check_access(e, pixel, &unreachable))
      {
        if (unreachable && s_ecology_repair_formation_crest(e,
            e->reference_height, pixel)) unreachable=0u;
        else if (unreachable) s_ecology_check_access(e,pixel,&unreachable);
#ifdef ISLAND_DEBUG_FORMATIONS
        if (formation==0u && trial<12u) fprintf(stderr,"  unreachable BEFORE = %u\n",unreachable);
#endif
        if (!unreachable)
          accepted = true;
        else
        {
          u32 corridors = 0u;
          memset(pending_mask, 0, (size_t)count);
          if (unreachable > 110u && s_ecology_repair_formation_access(e,
              e->reference_height, pixel, pending_mask, &corridors))
          {
            for (u32 at = 0u; at < count; ++at)
              e->formation_corridor_mask[at] |= pending_mask[at];
            e->formation_corridors += corridors;
            accepted = true;
          }
          /* A rare thin isolated lip may remain after crest repair. In
           * that case grade the reachable/unreachable boundary locally,
           * never the shore. Otherwise reject instead of destroying a
           * whole escarpment to force an access. */
          if (!accepted && unreachable <= 110u)
          {
            float *saved_heights=(float *)malloc(count*sizeof(float));
            u8 *saved_mask=(u8 *)malloc(count);
            if (saved_heights && saved_mask)
            {
              memcpy(saved_heights,e->height,count*sizeof(float));
              memcpy(saved_mask,e->access_repair_mask,count);
              const u32 old_steps=e->access_repair_steps;
              const u32 old_cells=e->access_repair_cells;
              const float old_delta=e->access_repair_max_change;
              const u32 old_unreachable=e->access_unreachable_before;
              if (s_ecology_repair_access(e,pixel))
              {
                for(u32 at=0u;at<count;++at)
                  if(e->height[at]!=saved_heights[at])
                  {e->formation_local_fix_mask[at]=1u;}
                ++e->formation_local_fix_events;
                accepted=true;
              }
              else
              {
                memcpy(e->height,saved_heights,count*sizeof(float));
                memcpy(e->access_repair_mask,saved_mask,count);
                e->access_repair_steps=old_steps;
                e->access_repair_cells=old_cells;
                e->access_repair_max_change=old_delta;
                e->access_unreachable_before=old_unreachable;
              }
            }
            free(saved_heights); free(saved_mask);
          }
        }
      }
      if (!accepted)
      {
        memcpy(e->height, e->reference_height,
            (size_t)count * sizeof(float));
        continue;
      }
      e->formations[e->formation_count++] = f;
      break;
    }
  }
  for (u32 at = 0u; at < count; ++at)
  {
    const float uplift = e->height[at] - original_height[at];
    e->formation_mask[at] = (u8)(e->land[at] && uplift > 0.003f);
    if (e->land[at] && uplift > 0.014f) ++e->uplift_cells;
    if (e->land[at]) e->max_uplift = fmaxf(e->max_uplift, uplift);
    if (e->formation_corridor_mask[at]) ++e->formation_corridor_cells;
    if (e->formation_local_fix_mask[at]) ++e->formation_local_fix_cells;
  }
  free(original_height); free(pending_mask);
  if (!s_ecology_check_access(e, pixel, &e->unreachable_land))
    return false;
  return e->unreachable_land == 0u;
}

/* Satellites must be truly separate contiguous land masses, never tiny
 * detached artifacts or accidentally touching the primary silhouette. */
static bool s_ecology_validate_islands(const IslandEcology *e,
    const IslandSatellites *satellites, u32 seed)
{
  const u32 n = ISLAND_MAP_SIZE, count = n * n;
  u32 *queue = (u32 *)malloc((size_t)count * sizeof(*queue));
  u8 *seen = (u8 *)calloc(count, sizeof(*seen));
  bool ok = true;
  if (!queue || !seen) {free(queue); free(seen); return false;}
  for (u32 id = 1u; id <= satellites->count + 1u && ok; ++id)
  {
    u32 start = UINT32_MAX, land_cells = 0u;
    for (u32 at = 0u; at < count; ++at)
      if (e->land_id[at] == id)
      {
        if (start == UINT32_MAX) start = at;
        ++land_cells;
      }
    if (start == UINT32_MAX) {ok = false; break;}
    u32 begin = 0u, end = 0u;
    queue[end++] = start;
    seen[start] = 1u;
    while (begin < end)
    {
      const u32 at = queue[begin++], x = at % n, y = at / n;
      u32 neighbors[4] = {
        x ? at - 1u : at, x+1u<n ? at+1u : at,
        y ? at-n : at, y+1u<n ? at+n : at
      };
      for (u32 k = 0u; k < 4u; ++k)
      {
        const u32 to = neighbors[k];
        if (to == at || seen[to] || e->land_id[to] != id) continue;
        seen[to] = 1u;
        queue[end++] = to;
      }
    }
    if (end != land_cells) ok = false;
  }
  if (!ok)
    fprintf(stderr, "seed %u: an island fragmented at final resolution\\n",
        seed);
  free(queue);
  free(seen);
  return ok;
}

/* Keep the inter-island rafts distinct from walking/wading. A landing must
 * have a BEACH edge that is physically passable from adjacent shallow water
 * within a short reach of the selected crossing points. The crossing route
 * itself can be deep; the camera visibility range is not yet known. */
static bool s_ecology_validate_raft_landings(const IslandEcology *e,
    const IslandGeographyView *view, const IslandSatellites *satellites,
    const IslandSatelliteConfig *config, u32 seed)
{
  const u32 n = ISLAND_MAP_SIZE;
  const float pixel = view->span / (float)n;
  const float max_gap = s_island_gap_world(config->max_gap_tiles, config);
  for (u32 i = 0u; i < satellites->count; ++i)
  {
    const IslandSatellite *sat = &satellites->islands[i];
    const u8 labels[2] = {sat->depart_id, (u8)(i + 2u)};
    const float targets_x[2] = {sat->depart_x, sat->arrive_x};
    const float targets_y[2] = {sat->depart_y, sat->arrive_y};
    float position_x[2] = {0}, position_y[2] = {0};
    for (u32 endpoint = 0u; endpoint < 2u; ++endpoint)
    {
      float best = INFINITY;
      for (u32 y = 1u; y + 1u < n; ++y)
        for (u32 x = 1u; x + 1u < n; ++x)
        {
          const u32 at = y*n+x;
          if (e->land_id[at] != labels[endpoint] ||
              e->coast[at] != ISLAND_COAST_BEACH) continue;
          const float wx = s_island_world_x(view, x),
              wy = s_island_world_y(view, y);
          const float dx = wx - targets_x[endpoint],
              dy = wy - targets_y[endpoint];
          const float d2 = dx*dx + dy*dy;
          if (d2 >= best || d2 > 0.062f * 0.062f) continue;
          const u32 neighbors[4] = {at-1u,at+1u,at-n,at+n};
          bool landed = false;
          for (u32 k = 0u; k < 4u; ++k)
            if (!e->land[neighbors[k]] &&
                s_ecology_edge_walkable(e, neighbors[k], at, pixel))
              landed = true;
          if (!landed) continue;
          best = d2;
          position_x[endpoint] = wx;
          position_y[endpoint] = wy;
        }
      if (!isfinite(best))
      {
        fprintf(stderr, "seed %u satellite %u: no beach landing at endpoint %u\\n",
            seed, i + 1u, endpoint);
        return false;
      }
    }
    const float dx = position_x[0] - position_x[1];
    const float dy = position_y[0] - position_y[1];
    const float distance = sqrtf(dx*dx + dy*dy);
    if (distance > max_gap)
    {
      fprintf(stderr, "seed %u satellite %u: landing gap %.4f exceeds limit\\n",
          seed, i + 1u, distance);
      return false;
    }
  }
  return true;
}

static bool s_ecology_build(IslandEcology *e,
    const IslandGeography *graph, const IslandGeographyView *view,
    const IslandSatellites *satellites,
    const IslandSatelliteConfig *config, u32 seed)
{
  const u32 size = ISLAND_MAP_SIZE;
  const u32 count = size * size;
  const float pixel_world = view->span / (float)size;
  const float beach_width = ISLAND_BEACH_WIDTH;
  const float coast_profile_width = ISLAND_COAST_PROFILE_WIDTH;
  const float target_beach = 0.60f;
  float beach_cutoff;
  const float global_moisture =
      (s_ecology_random(seed, 0xA418CE23u) - 0.5f) * 0.28f;
  const float target_thorns = 0.065f +
      s_ecology_random(seed, 0xD1337167u) * 0.13f;
  const float target_tall = 0.13f +
      s_ecology_random(seed, 0x0AE8B23Fu) * 0.17f;
  float thorn_threshold, tall_threshold;

  /* Exact same graph, field, threshold and view as <seed>_map.ppm. */
  for (u32 y = 0u; y < size; ++y)
  {
    float wy = view->center_y +
        (((float)y + 0.5f) / (float)size - 0.5f) * view->span;
    for (u32 x = 0u; x < size; ++x)
    {
      float wx = view->center_x +
          (((float)x + 0.5f) / (float)size - 0.5f) * view->span;
      u32 at = y * size + x;
      e->land_id[at] = s_island_land_id(
          graph, seed, view, satellites, wx, wy);
      e->land[at] = (u8)(e->land_id[at] != 0u);
      e->land_count += (u64)e->land[at];
    }
  }
  if (!e->land_count ||
      !s_ecology_validate_islands(e, satellites, seed))
    return false;
  e->slope_world_span = view->span;
  s_ecology_build_shore(e);

  /* Normalize beach coverage along the SHORELINE, rather than using a
   * fixed noise threshold that can make an entire seed sandy. */
  {
    float lo = INFINITY, hi = -INFINITY;
    u64 shoreline_edges = 0u;
    for (u32 y = 0u; y < size; ++y)
    {
      for (u32 x = 0u; x < size; ++x)
      {
        const u32 at = y * size + x;
        if (!e->land[at] || e->shore_distance[at] * pixel_world >
            coast_profile_width)
          continue;
        const float wx = view->center_x +
            (((float)x + 0.5f) / (float)size - 0.5f) * view->span;
        const float wy = view->center_y +
            (((float)y + 0.5f) / (float)size - 0.5f) * view->span;
        /* Continuous in world space; never sample a jumping nearest-source
         * coordinate to determine coastal elevation. */
        float suitability = 0.70f * s_fbm_compose_2d(
            wx * 3.0f + 7.0f, wy * 3.0f - 4.0f,
            seed ^ 0x7C2E830Bu, 4u) +
            0.30f * s_fbm_compose_2d(
            wx * 7.0f, wy * 7.0f,
            seed ^ 0x731AD427u, 3u);
        /* Favor a short, physically beachable crossing on BOTH sides of
         * each satellite route. This changes the shape of the coast profile
         * locally, but the global 60/40 quantile remains the same. */
        float landing_boost = 0.0f;
        for (u32 si = 0u; si < satellites->count; ++si)
        {
          const IslandSatellite *sat = &satellites->islands[si];
          const float dx0 = wx - sat->depart_x;
          const float dy0 = wy - sat->depart_y;
          const float dx1 = wx - sat->arrive_x;
          const float dy1 = wy - sat->arrive_y;
          const float d2 = fminf(dx0*dx0 + dy0*dy0,
              dx1*dx1 + dy1*dy1);
          const float radius = 0.042f;
          if (d2 < radius*radius)
            landing_boost = fmaxf(landing_boost,
                1.0f - s_smoothstep_f32(sqrtf(d2) / radius));
        }
        suitability += landing_boost;
        e->coast_suitability[at] = suitability;
        {
          const u32 edges = s_ecology_coast_edges(e, x, y);
          if (edges)
          {
            lo = fminf(lo, suitability);
            hi = fmaxf(hi, suitability);
            shoreline_edges += edges;
          }
        }
      }
    }
    if (!shoreline_edges)
      return false;
    for (u32 pass = 0u; pass < 22u; ++pass)
    {
      float mid = 0.5f * (lo + hi);
      u64 above = 0u;
      for (u32 y = 0u; y < size; ++y)
        for (u32 x = 0u; x < size; ++x)
        {
          const u32 at = y * size + x;
          if (e->land[at] && e->coast_suitability[at] > mid)
            above += s_ecology_coast_edges(e, x, y);
        }
      if ((double)above / (double)shoreline_edges > target_beach)
        lo = mid;
      else
        hi = mid;
    }
    beach_cutoff = 0.5f * (lo + hi);
  }

  /* Coast type uses large-scale continuous noise at world position.
   * The coastline still controls where the profile can appear. */
  for (u32 y = 0u; y < size; ++y)
  {
    const float wy = view->center_y +
        (((float)y + 0.5f) / (float)size - 0.5f) * view->span;
    for (u32 x = 0u; x < size; ++x)
    {
      const float wx = view->center_x +
          (((float)x + 0.5f) / (float)size - 0.5f) * view->span;
      const u32 at = y * size + x;
      float moisture, dryness, dense, thorn, tall;
      if (!e->land[at])
        continue;

      moisture = s_clamp01(s_fbm_compose_2d(
          wx * 2.2f + 19.3f, wy * 2.2f - 8.7f,
          seed ^ 0x3A87D9B2u, 4u) + global_moisture);
      dense = s_fbm_compose_2d(wx * 5.5f, wy * 5.5f,
          seed ^ 0x9516AC47u, 4u);
      dryness = 1.0f - moisture;
      thorn = s_fbm_compose_2d(wx * 4.2f + 7.1f,
          wy * 4.2f - 4.1f, seed ^ 0xE784FA11u, 4u);
      tall = s_fbm_compose_2d(wx * 3.7f - 8.0f,
          wy * 3.7f + 3.5f, seed ^ 0xC583A3B1u, 4u);
      e->moisture[at] = moisture;
      e->thorn_score[at] = thorn + 0.20f * dryness;
      e->tall_score[at] = tall + 0.20f * moisture;

      /* Assign non-gameplay surfaces initially; hazardous vegetation is
       * layered in a second pass, after coast profiles have been fixed. */
      if (moisture > 0.56f && dense > 0.44f)
        e->surface[at] = ISLAND_SURFACE_FOREST;
      else if (moisture < 0.42f && dense < 0.60f)
        e->surface[at] = ISLAND_SURFACE_DRY;
      else
        e->surface[at] = ISLAND_SURFACE_GRASS;

      if (e->shore_distance[at] * pixel_world <= coast_profile_width)
      {
        if (e->coast_suitability[at] >= beach_cutoff)
          e->coast_profile[at] = ISLAND_COAST_BEACH;
        else
        {
          const float cliff_suitability = s_fbm_compose_2d(
              wx * 2.4f, wy * 2.4f,
              seed ^ 0x9D1D3A75u, 3u);
          e->coast_profile[at] = cliff_suitability > 0.50f
              ? ISLAND_COAST_CLIFF : ISLAND_COAST_OVERGROWN;
        }
        if (e->shore_distance[at] * pixel_world <= beach_width)
        {
          e->coast[at] = e->coast_profile[at];
          if (e->coast[at] == ISLAND_COAST_BEACH)
            e->surface[at] = ISLAND_SURFACE_SAND;
          else if (e->coast[at] == ISLAND_COAST_CLIFF)
            e->surface[at] = ISLAND_SURFACE_ROCK;
          else
            e->surface[at] = ISLAND_SURFACE_GRASS;
        }
      }
    }
  }

  thorn_threshold = s_ecology_quantile(e->thorn_score,
      e->land, e->surface, ISLAND_SURFACE_THORNS, count, target_thorns);
  for (u32 i = 0u; i < count; ++i)
    if (e->land[i] && e->surface[i] != ISLAND_SURFACE_SAND &&
        e->surface[i] != ISLAND_SURFACE_ROCK &&
        e->thorn_score[i] >= thorn_threshold)
      e->surface[i] = ISLAND_SURFACE_THORNS;

  tall_threshold = s_ecology_quantile(e->tall_score,
      e->land, e->surface, ISLAND_SURFACE_THORNS, count, target_tall);
  for (u32 i = 0u; i < count; ++i)
  {
    if (e->land[i] && e->surface[i] != ISLAND_SURFACE_SAND &&
        e->surface[i] != ISLAND_SURFACE_ROCK &&
        e->surface[i] != ISLAND_SURFACE_THORNS &&
        e->tall_score[i] >= tall_threshold)
      e->surface[i] = ISLAND_SURFACE_TALL_GRASS;
    /* A non-beach vegetated coast always has tall grass or thorns. */
    if (e->coast[i] == ISLAND_COAST_OVERGROWN &&
        e->surface[i] != ISLAND_SURFACE_THORNS)
      e->surface[i] = ISLAND_SURFACE_TALL_GRASS;
  }

  /*
   * The sea floor and land heights share one heightmap with sea level = 0.
   * Only the APPROVED land mask defines the shoreline: land heights are
   * strictly positive; submerged heights are strictly negative. Bathymetry
   * cannot open/close straits or change the macro-geography.
   *
   * Two distance fields have deliberately different purposes:
   *  - land -> water: a short transition to a low, mostly flat interior;
   *  - water -> land: a submerged shelf dropping to deep ocean.
   */
  s_ecology_build_water_distance(e);
  for (u32 y = 0u; y < size; ++y)
  {
    const float wy = view->center_y +
        (((float)y + 0.5f) / (float)size - 0.5f) * view->span;
    for (u32 x = 0u; x < size; ++x)
    {
      const u32 at = y * size + x;
      const float wx = view->center_x +
          (((float)x + 0.5f) / (float)size - 0.5f) * view->span;
      if (!e->land[at])
      {
        const float d = e->water_distance[at] * pixel_world;
        const float shelf = s_smoothstep_f32(s_clamp01(d / 0.050f));
        const float deep = s_smoothstep_f32(
            s_clamp01((d - 0.030f) / 0.110f));
        const float seafloor_noise = s_fbm_compose_2d(
            wx * 2.5f + 5.0f, wy * 2.5f - 9.0f,
            seed ^ 0xA3E16F21u, 3u);
        const float depth = 0.002f + 0.038f * shelf +
            0.15f * deep + 0.010f * (seafloor_noise - 0.5f) * deep;
        e->height[at] = -depth;
        e->surface[at] = depth < ISLAND_SHALLOW_DEPTH_LIMIT
            ? ISLAND_SURFACE_SHALLOW_WATER : ISLAND_SURFACE_DEEP_WATER;
        continue;
      }
      {
        const float d = e->shore_distance[at] * pixel_world;
        const float inshore = s_smoothstep_f32(s_clamp01(d / 0.075f));
        const float hills = s_fbm_compose_2d(
            wx * 2.35f + 9.0f, wy * 2.35f - 3.0f,
            seed ^ 0x85DEA971u, 4u);
        const float small_hills = s_fbm_compose_2d(
            wx * 6.5f, wy * 6.5f, seed ^ 0x77A1234Du, 3u);
        /* Beaches begin nearly at sea level. Away from shore, land settles
         * into a LOW plateau with independent smooth relief, rather than
         * climbing until the midpoint between two opposite shorelines. */
        float height = 0.0006f + 0.0224f * inshore +
            inshore * (0.042f * (hills - 0.5f) +
                       0.007f * (small_hills - 0.5f));
        if (d < coast_profile_width)
        {
          /* Smooth raised/flat coast preference; categorical color changes
           * do not introduce height seams. Its influence fades BEFORE the
           * interior, with no Voronoi/nearest-shore source involved. */
          const float suitability = e->coast_suitability[at];
          const float nonbeach = s_smoothstep_f32(s_clamp01(
              (beach_cutoff - suitability) / 0.55f));
          const float cliff = s_fbm_compose_2d(
              wx * 2.4f, wy * 2.4f,
              seed ^ 0x9D1D3A75u, 3u);
          /* Elevated coastline, fading gradually inland to avoid isolating
           * the upper terrain. The sea-to-land face can remain steep. */
          const float rise = 1.0f;
          const float fade = 1.0f - s_smoothstep_f32(s_clamp01(
              (d - 0.012f) / (coast_profile_width - 0.012f)));
          height += nonbeach * (0.086f + 0.038f * cliff) * rise * fade;
        }
        /* Every non-beach shoreline must be a real raised bank, not
         * just a categorical prohibition on leaving the water. The lower
         * bound is strongest at the coast and dies out inside the island.
         * Beaches deliberately do not receive this raised profile. */
        if (e->coast_profile[at] != ISLAND_COAST_BEACH && d < 0.024f)
        {
          const float fade = 1.0f - s_smoothstep_f32(
              s_clamp01(d / 0.024f));
          height = fmaxf(height, 0.0055f * fade);
        }
        e->height[at] = fmaxf(0.0005f, height);
      }
    }
  }

  /* Repair any slopes that isolate land BEFORE placing interior disks.
   * A disk is still rejected if it would remove an existing passage. */
  if (!s_ecology_repair_access(e, pixel_world))
  {
    fprintf(stderr, "seed %u: access repair could not connect %u cells "
        "without modifying the shoreline or island silhouette\n",
        seed, e->unreachable_land);
    return false;
  }
  if (!s_ecology_generate_formations(e, view, seed))
  {
    fprintf(stderr, "seed %u: pre-existing or generated inaccessible land: "
        "%u cells\n", seed, e->unreachable_land);
    return false;
  }
  for (u32 y = 0u; y < size; ++y)
    for (u32 x = 0u; x < size; ++x)
    {
      const u32 at = y * size + x;
      const u32 edges = s_ecology_coast_edges(e, x, y);
      e->coast_edges_total += edges;
      if (e->coast[at] == ISLAND_COAST_BEACH)
      {
        e->coast_edges_beach += edges;
        if (edges)
        {
          const float pixel = view->span / (float)size;
          const u32 neighbors[4] = {
            x > 0u ? at - 1u : at,
            x + 1u < size ? at + 1u : at,
            y > 0u ? at - size : at,
            y + 1u < size ? at + size : at
          };
          for (u32 k = 0u; k < 4u; ++k)
            if (neighbors[k] != at && !e->land[neighbors[k]] &&
                s_ecology_edge_walkable(e, neighbors[k], at, pixel))
              ++e->beach_edges_walkable;
        }
      }
      else if (edges)
      {
        const float pixel = view->span / (float)size;
        const u32 neighbors[4] = {
          x > 0u ? at - 1u : at,
          x + 1u < size ? at + 1u : at,
          y > 0u ? at - size : at,
          y + 1u < size ? at + size : at
        };
        for (u32 k = 0u; k < 4u; ++k)
          if (neighbors[k] != at && !e->land[neighbors[k]])
          {
            ++e->cliff_edges_total;
            if (e->height[at] - e->height[neighbors[k]] >
                ISLAND_WALKABLE_SLOPE * pixel)
              ++e->cliff_edges_steep;
          }
      }
    }

  if (e->beach_edges_walkable != e->coast_edges_beach)
  {
    fprintf(stderr, "seed %u: only %llu of %llu beach shore edges "
        "are physically walkable\n", seed,
        (unsigned long long)e->beach_edges_walkable,
        (unsigned long long)e->coast_edges_beach);
    return false;
  }
  if (!s_ecology_validate_raft_landings(
          e, view, satellites, config, seed))
    return false;
  for (u32 i = 0u; i < count; ++i)
  {
    e->surface_count[e->surface[i]]++;
    e->coast_count[e->coast[i]]++;
  }
  return true;
}


/* A value outside the 600-sample ecology view is deep sea, never land.
 * Expose the samples in the game output grid without changing the v1.3
 * topology, shoreline classifier or path-repair code. */
static bool s_generator_export(const IslandEcology *e,
    const IslandGeographyView *view, const TFTFIslandGeneratorConfig *config,
    float meters_per_world_unit, TFTFIslandGeneratorMap *out)
{
  const u32 n = ISLAND_MAP_SIZE;
  const u32 size = config->map_size;
  const size_t count = (size_t)size * (size_t)size;
  const float world_per_cell = config->cell_size_meters /
      meters_per_world_unit;
  const float world_span = (float)size * world_per_cell;
  if (world_span < view->span - world_per_cell)
    return false; /* No silent clipping of satellite islands. */
  out->cells = (TFTFIslandGeneratorCell *)calloc(count, sizeof(*out->cells));
  if (!out->cells) return false;
  out->width = size;
  out->height = size;
  out->seed = config->seed;
  out->meters_per_world_unit = meters_per_world_unit;
  out->sampled_world_span = view->span;
  out->formation_count = e->formation_count;
  out->satellite_count = 0u; /* Set after satellites are validated. */
  out->unreachable_land_cells = e->unreachable_land;
  for (u32 y = 0u; y < size; ++y)
  {
    const float wy = view->center_y +
        ((float)y + 0.5f - 0.5f * (float)size) * world_per_cell;
    const float sy = (wy - view->center_y) / view->span * (float)n +
        0.5f * (float)n - 0.5f;
    for (u32 x = 0u; x < size; ++x)
    {
      const float wx = view->center_x +
          ((float)x + 0.5f - 0.5f * (float)size) * world_per_cell;
      const float sx = (wx - view->center_x) / view->span * (float)n +
          0.5f * (float)n - 0.5f;
      TFTFIslandGeneratorCell *dst = &out->cells[(size_t)y * size + x];
      dst->surface = TFTF_ISLAND_GENERATOR_SURFACE_DEEP_WATER;
      dst->height = -0.19f;
      if (sx < 0.0f || sy < 0.0f || sx > (float)(n - 1u) ||
          sy > (float)(n - 1u))
        continue;
      const u32 ix = (u32)floorf(sx + 0.5f);
      const u32 iy = (u32)floorf(sy + 0.5f);
      const u32 at = iy * n + ix;
      dst->surface = e->surface[at];
      dst->coast = e->coast[at];
      dst->land_id = e->land_id[at];
      dst->height = e->height[at];
      /* Noise is generated here, never in the grass renderer. Density is
       * independent of the choice of a grass type in the TFTF editor. */
      const float density_noise = s_fbm_compose_2d(
          wx * 12.5f + 4.0f, wy * 12.5f - 8.0f,
          config->seed ^ 0xA1D283C9u, 4u);
      dst->density = s_clamp01((density_noise - 0.25f) * 1.5f);
    }
  }
  return true;
}

void tftf_island_generator_release(TFTFIslandGeneratorMap *map)
{
  if (!map) return;
  free(map->cells);
  memset(map, 0, sizeof(*map));
}

void tftf_island_generator_config_defaults(TFTFIslandGeneratorConfig *config)
{
  if (!config) return;
  *config = (TFTFIslandGeneratorConfig){0};
  config->seed = 1u;
  config->map_size = 600u;
  config->cell_size_meters = 1.0f;
  config->land_percent = ISLAND_DEFAULT_AREA_PERCENT;
  config->min_satellite_gap_tiles = ISLAND_DEFAULT_MIN_GAP_TILES;
  config->max_satellite_gap_tiles = ISLAND_DEFAULT_MAX_GAP_TILES;
}

bool tftf_island_generator_generate(const TFTFIslandGeneratorConfig *config,
    TFTFIslandGeneratorMap *out)
{
  if (!config || !out || !config->map_size ||
      config->map_size > 2048u || !config->seed ||
      !isfinite(config->cell_size_meters) ||
      config->cell_size_meters <= 0.0f ||
      !isfinite(config->land_percent) ||
      config->land_percent < 1.0f || config->land_percent > 80.0f ||
      !isfinite(config->min_satellite_gap_tiles) ||
      !isfinite(config->max_satellite_gap_tiles) ||
      config->min_satellite_gap_tiles < 0.0f ||
      config->max_satellite_gap_tiles < config->min_satellite_gap_tiles)
    return false;
  memset(out, 0, sizeof(*out));
  IslandGeography graph;
  IslandGeographyView original_view;
  s_island_geography_build(config->seed, ISLAND_BRANCHING, ISLAND_LOOPING,
      &graph);
  if (!s_island_geography_view_build(&graph, config->seed,
          config->land_percent * 0.04f, &original_view))
    return false;
  /* The provisional test's 200 m/world-unit is replaced by a calibration
   * derived from the actual game map size. A margin is reserved for 0-2
   * satellites without depending on the 600x600 diagnostic raster. */
  const float meters_per_world_unit =
      (float)config->map_size * config->cell_size_meters /
      (original_view.span + ISLAND_SATELLITE_PLACEMENT_MARGIN);
  IslandSatelliteConfig satellite_config = {
    config->min_satellite_gap_tiles,
    config->max_satellite_gap_tiles,
    config->cell_size_meters,
    meters_per_world_unit
  };
  for (u32 variant = 0u; variant < 12u; ++variant)
  {
    IslandSatellites satellites = {0};
    IslandGeographyView view = original_view;
    if (!s_island_satellites_build(&graph, config->seed, variant,
            &satellite_config, &view, &satellites))
      continue;
    IslandEcology e;
    if (!s_ecology_allocate(&e))
    {
      s_ecology_free(&e);
      return false;
    }
    const bool built = s_ecology_build(&e, &graph, &view, &satellites,
        &satellite_config, config->seed);
    const bool copied = built && s_generator_export(&e, &view, config,
        meters_per_world_unit, out);
    if (copied) out->satellite_count = satellites.count;
    s_ecology_free(&e);
    if (copied) return true;
    tftf_island_generator_release(out);
  }
  return false;
}
