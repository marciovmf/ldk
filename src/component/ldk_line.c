/**
 * @file ldk_line.c
 * @brief Dynamic line segments using an owned mesh source.
 */

#include <component/ldk_line.h>
#include <ldk.h>
#include <ldk_mesh_asset.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LDK_LINE_DEFAULT_MAX_SEGMENTS 64u
#define LDK_LINE_VERTICES_PER_SEGMENT 4u
#define LDK_LINE_INDICES_PER_SEGMENT 12u

static bool s_line_max_valid(u32 maximum)
{
  /* GPU buffer byte counts and segment indices are all u32. */
  return maximum <= UINT32_MAX /
          (LDK_LINE_VERTICES_PER_SEGMENT * sizeof(LDKMeshVertex)) &&
      maximum <= UINT32_MAX /
          (LDK_LINE_INDICES_PER_SEGMENT * sizeof(u32));
}

static bool s_line_segment_valid(const LDKLineSegment *segment)
{
  if (!segment)
  {
    return false;
  }
  const float values[] = {segment->pos_start.x, segment->pos_start.y,
      segment->pos_start.z, segment->pos_end.x, segment->pos_end.y,
      segment->pos_end.z, segment->initial_width, segment->final_width,
      segment->initial_alpha, segment->final_alpha};
  for (u32 i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
  {
    if (!isfinite(values[i]))
    {
      return false;
    }
  }
  return segment->initial_width >= 0.0f && segment->final_width >= 0.0f &&
      segment->initial_alpha >= 0.0f && segment->initial_alpha <= 1.0f &&
      segment->final_alpha >= 0.0f && segment->final_alpha <= 1.0f;
}

LDKLineSegment ldk_line_segment_default(void)
{
  LDKLineSegment segment = {0};
  segment.initial_color = 0xffffffffu;
  segment.final_color = 0xffffffffu;
  segment.initial_width = 0.1f;
  segment.final_width = 0.1f;
  segment.initial_alpha = 1.0f;
  segment.final_alpha = 1.0f;
  return segment;
}

void ldk_line_segment_initialize(void *element)
{
  if (element) *(LDKLineSegment *)element = ldk_line_segment_default();
}

void ldk_line_segments_changed(void *component)
{
  if (component) ((LDKLineComponent *)component)->dirty = true;
}

u32 ldk_line_segment_count(const LDKLineComponent *line)
{
  return line && line->segments ? x_array_count(line->segments) : 0u;
}

bool ldk_line_max_segments_set(LDKLineComponent *line, u32 maximum)
{
  if (!line || !s_line_max_valid(maximum) ||
      maximum < ldk_line_segment_count(line)) return false;
  line->max_segments = maximum;
  return true;
}

bool ldk_line_segment_add(LDKLineComponent *line,
    const LDKLineSegment *segment, u32 *out_index)
{
  if (!line || !s_line_segment_valid(segment) ||
      ldk_line_segment_count(line) >= line->max_segments) return false;
  if (!line->segments)
  {
    line->segments = x_array_LDKLineSegment_create(0);
    if (!line->segments) return false;
  }
  u32 index = ldk_line_segment_count(line);
  if (x_array_LDKLineSegment_add_ptr(line->segments, segment) != XARRAY_OK)
    return false;
  if (out_index) *out_index = index;
  line->dirty = true;
  return true;
}

bool ldk_line_segment_set(LDKLineComponent *line, u32 index,
    const LDKLineSegment *segment)
{
  LDKLineSegment *existing = line && line->segments
      ? x_array_LDKLineSegment_get(line->segments, index) : NULL;
  if (!line || index >= ldk_line_segment_count(line) ||
      !s_line_segment_valid(segment) || !existing) return false;
  if (memcmp(existing, segment, sizeof(*segment)) != 0)
  {
    *existing = *segment;
    line->dirty = true;
  }
  return true;
}

const LDKLineSegment *ldk_line_segment_get(
    const LDKLineComponent *line, u32 index)
{
  if (!line || !line->segments || index >= ldk_line_segment_count(line))
    return NULL;
  return x_array_LDKLineSegment_get_const(line->segments, index);
}

bool ldk_line_segment_remove(LDKLineComponent *line, u32 index)
{
  if (!line || !line->segments || index >= ldk_line_segment_count(line))
    return false;
  x_array_delete_at(line->segments, index);
  line->dirty = true;
  return true;
}

void ldk_line_clear(LDKLineComponent *line)
{
  if (line && ldk_line_segment_count(line))
  {
    x_array_clear(line->segments);
    line->dirty = true;
  }
}

#ifdef LDK_ENGINE
static u32 s_line_vertex_color(u32 color, float alpha)
{
  const u32 source_alpha = color & 0xffu;
  const u32 result_alpha = (u32)((float)source_alpha * alpha + 0.5f);
  return LDK_RGBA32((color & 0xffffff00u) | result_alpha);
}

static void s_line_segment_build(const LDKLineSegment *segment,
    LDKMeshVertex *vertices, u32 *indices, u32 segment_index)
{
  Vec3 direction = vec3_sub(segment->pos_end, segment->pos_start);
  if (vec3_len2(direction) < 1e-12f)
  {
    direction = vec3_make(1.0f, 0.0f, 0.0f);
  }
  direction = vec3_norm(direction);
  Vec3 axis = fabsf(direction.y) < 0.9f
      ? vec3_make(0.0f, 1.0f, 0.0f)
      : vec3_make(1.0f, 0.0f, 0.0f);
  Vec3 side = vec3_norm(vec3_cross(direction, axis));
  Vec3 normal = vec3_cross(side, direction);
  Vec3 start_offset = vec3_mul(side, segment->initial_width * 0.5f);
  Vec3 end_offset = vec3_mul(side, segment->final_width * 0.5f);
  u32 first_color = s_line_vertex_color(
      segment->initial_color, segment->initial_alpha);
  u32 last_color = s_line_vertex_color(
      segment->final_color, segment->final_alpha);

  vertices[0] = (LDKMeshVertex){.position =
          vec3_sub(segment->pos_start, start_offset),
      .normal = normal, .uv = vec2_make(0.0f, 0.0f), .color = first_color};
  vertices[1] = (LDKMeshVertex){.position =
          vec3_add(segment->pos_start, start_offset),
      .normal = normal, .uv = vec2_make(0.0f, 1.0f), .color = first_color};
  vertices[2] = (LDKMeshVertex){.position =
          vec3_sub(segment->pos_end, end_offset),
      .normal = normal, .uv = vec2_make(1.0f, 0.0f), .color = last_color};
  vertices[3] = (LDKMeshVertex){.position =
          vec3_add(segment->pos_end, end_offset),
      .normal = normal, .uv = vec2_make(1.0f, 1.0f), .color = last_color};

  u32 base = segment_index * LDK_LINE_VERTICES_PER_SEGMENT;
  indices[0] = base;
  indices[1] = base + 1u;
  indices[2] = base + 2u;
  indices[3] = base + 2u;
  indices[4] = base + 1u;
  indices[5] = base + 3u;
  /* Regular mesh pipelines cull back faces; render both sides of each strip. */
  indices[6] = base + 2u;
  indices[7] = base + 1u;
  indices[8] = base;
  indices[9] = base + 3u;
  indices[10] = base + 1u;
  indices[11] = base + 2u;
}

static bool s_line_mesh_reserve(LDKLineComponent *line,
    LDKAssetMeshData *data, u32 required_segments)
{
  if (required_segments <= line->mesh_capacity)
  {
    return true;
  }
  u32 capacity = line->max_segments;
  if (!s_line_max_valid(capacity) || capacity < required_segments)
  {
    return false;
  }
  LDKMeshVertex *vertices = calloc((size_t)capacity * 4u, sizeof(*vertices));
  u32 *indices = calloc((size_t)capacity * LDK_LINE_INDICES_PER_SEGMENT, sizeof(*indices));
  if (!vertices || !indices)
  {
    free(vertices);
    free(indices);
    return false;
  }
  free(data->mesh.vertices);
  free(data->mesh.indices);
  data->mesh.vertices = vertices;
  data->mesh.indices = indices;
  line->mesh_capacity = capacity;
  return true;
}

bool ldk_line_mesh_sync(LDKLineComponent *line, LDKAssetManager *assets)
{
  if (!line || !assets)
  {
    return false;
  }
  if (!ldk_line_segment_count(line))
  {
    /* Keep the mesh buffers, but don't render them while there are no lines. */
    line->dirty = false;
    return true;
  }
  if (!line->segments || !s_line_max_valid(line->max_segments) ||
      ldk_line_segment_count(line) > line->max_segments)
  {
    return false;
  }
  if (!line->dirty && !x_handle_is_null(line->source.source_asset.h))
  {
    return true;
  }

  LDKAssetMeshData *data = NULL;
  if (x_handle_is_null(line->source.source_asset.h))
  {
    /* Meshes with zero vertices/indices cannot be created. Create only when
     * the first segment exists, reserving enough space for the full limit. */
    u32 capacity = line->max_segments;
    LDKMeshVertex *vertices = calloc((size_t)capacity * 4u, sizeof(*vertices));
    u32 *indices = calloc((size_t)capacity * LDK_LINE_INDICES_PER_SEGMENT, sizeof(*indices));
    if (!vertices || !indices)
    {
      free(vertices);
      free(indices);
      return false;
    }
    LDKAssetMesh asset = ldk_asset_manager_mesh_create(assets, vertices,
        capacity * LDK_LINE_VERTICES_PER_SEGMENT, indices,
        capacity * LDK_LINE_INDICES_PER_SEGMENT);
    free(vertices);
    free(indices);
    if (x_handle_is_null(asset.h))
    {
      return false;
    }
    data = ldk_asset_manager_mesh_get(assets, asset);
    if (!data)
    {
      ldk_asset_manager_mesh_unload(assets, asset);
      return false;
    }
    line->source.source_asset = asset;
    line->source.mesh_index = 0u;
    line->mesh_capacity = capacity;
  }
  else
  {
    data = ldk_asset_manager_mesh_get(assets, line->source.source_asset);
    if (!data || !s_line_mesh_reserve(line, data, ldk_line_segment_count(line)))
    {
      return false;
    }
  }

  for (u32 i = 0; i < ldk_line_segment_count(line); ++i)
  {
    s_line_segment_build(x_array_LDKLineSegment_get(line->segments, i),
        &data->mesh.vertices[i * LDK_LINE_VERTICES_PER_SEGMENT],
        &data->mesh.indices[i * LDK_LINE_INDICES_PER_SEGMENT], i);
  }
  data->mesh.vertex_count = ldk_line_segment_count(line) * LDK_LINE_VERTICES_PER_SEGMENT;
  data->mesh.index_count = ldk_line_segment_count(line) * LDK_LINE_INDICES_PER_SEGMENT;
  data->mesh.has_tangents = false;
  line->source.dirty = true;
  line->dirty = false;
  return true;
}

static bool s_line_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, const void *initial_value, void *user)
{
  LDKLineComponent *line = component;
  const LDKLineComponent *initial = initial_value;
  if (!line)
  {
    return false;
  }

  u32 maximum = initial ? initial->max_segments : LDK_LINE_DEFAULT_MAX_SEGMENTS;
  u32 count = initial ? ldk_line_segment_count(initial) : 0u;
  if (!s_line_max_valid(maximum) || count > maximum) return false;
  XArray_LDKLineSegment *segments = x_array_LDKLineSegment_create(count);
  if (!segments) return false;
  for (u32 i = 0; i < count; ++i)
  {
    const LDKLineSegment *item = ldk_line_segment_get(initial, i);
    if (!s_line_segment_valid(item) ||
        x_array_LDKLineSegment_add_ptr(segments, item) != XARRAY_OK)
    {
      x_array_destroy(segments);
      return false;
    }
  }

  LDKComponentDesc base = ldk_mesh_source_component_desc(0);
  if (!base.attach(entities, components, entity, &line->source, index,
          NULL, user))
  {
    x_array_destroy(segments);
    return false;
  }
  line->max_segments = maximum;
  line->segments = segments;
  line->mesh_capacity = 0u;
  line->dirty = true;
  ldk_material_desc_defaults(
      LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT, &line->source.material);
  line->source.material.alpha_mode = LDK_MATERIAL_ALPHA_MODE_BLEND;
  line->source.material_dirty = true;
  line->source.casts_shadows = false;
  return true;
}

static void s_line_destroy(LDKEntityRegistry *entities,
    LDKComponentRegistry *components, LDKEntity entity, void *component,
    u32 index, void *user)
{
  LDKLineComponent *line = component;
  if (!line)
  {
    return;
  }
  LDKAssetMesh asset = line->source.source_asset;
  LDKComponentDesc base = ldk_mesh_source_component_desc(0);
  base.destroy(entities, components, entity, &line->source, index, user);
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (assets && !x_handle_is_null(asset.h))
  {
    ldk_asset_manager_mesh_unload(assets, asset);
  }
  x_array_destroy(line->segments);
  line->segments = NULL;
  line->mesh_capacity = 0u;
  line->source.source_asset = ldk_asset_mesh_null();
}

LDKComponentDesc ldk_line_component_desc(u32 initial_capacity)
{
  LDKComponentDesc desc = {0};
  desc.name = "Line";
  desc.type = LDK_COMPONENT_TYPE_LINE;
  desc.entry_size = sizeof(LDKLineComponent);
  desc.initial_capacity = initial_capacity;
  desc.attach = s_line_attach;
  desc.destroy = s_line_destroy;
  return desc;
}
#endif // LDK_ENGINE
