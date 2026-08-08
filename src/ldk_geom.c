#include <ldk_common.h>
#include <ldk_geom.h>
#include <ldk_color.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <stdx/stdx_math.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

LDKSize ldk_size(i32 width, i32 height)
{
  LDKSize size = {.w = width, .h = height}; 
  return size;
}

LDKSize ldk_size_zero()
{
  LDKSize size = {0};
  return size;
}

LDKSize ldk_size_one()
{
  LDKSize size = {.w = 1, .h = 1}; 
  return size;
}

LDKSizef ldk_sizef(float width, float height)
{
  LDKSizef size = {.w = width, .h = height}; 
  return size;
}

LDKSizef ldk_sizef_zero()
{
  LDKSizef size = {0};
  return size;
}

LDKSizef ldk_sizef_one()
{
  LDKSizef size = {.w = 1.0f, .h = 1.0f}; 
  return size;
}

LDKRect ldk_rect(i32 x, i32 y, i32 width, i32 height)
{
  LDKRect rect = {.x = x, .y = y, .w = width, .h = height};
  return rect;
}

bool ldk_rect_contains(const LDKRect* rect, i32 x, i32 y)
{
  if (x < rect->x) { return false; }
  if (y < rect->y) { return false; }
  if (x >= rect->x + rect->w) { return false; }
  if (y >= rect->y + rect->h) { return false; }
  return true;
}

LDKRect ldk_rect_intersect(const LDKRect* a, const LDKRect* b)
{
  LDKRect rect;
  i32 x0 = (i32) float_max((float)a->x, (float)b->x);
  i32 y0 = (i32) float_max((float)a->y, (float)b->y);
  i32 x1 = (i32) float_min((float)(a->x + a->w), (float)(b->x + b->w));
  i32 y1 = (i32) float_min((float)(a->y + a->h), (float)(b->y + b->h));

  rect.x = x0;
  rect.y = y0;
  rect.w = (i32)float_max(0.0f, (float)(x1 - x0));
  rect.h = (i32)float_max(0.0f, (float)(y1 - y0));

  return rect;
}

LDKRectf ldk_rectf(float x, float y, float width, float height)
{
  LDKRectf rectf = {.x = x, .y = y, .w = width, .h = height};
  return rectf;
}

bool ldk_rectf_contains(const LDKRectf* rect, float x, float y)
{
  if (x < rect->x) { return false; }
  if (y < rect->y) { return false; }
  if (x >= rect->x + rect->w) { return false; }
  if (y >= rect->y + rect->h) { return false; }
  return true;
}

LDKRectf ldk_rectf_intersect(const LDKRectf* a, const LDKRectf* b)
{
  LDKRectf rect;
  float x0 = float_max(a->x, b->x);
  float y0 = float_max(a->y, b->y);
  float x1 = float_min(a->x + a->w, b->x + b->w);
  float y1 = float_min(a->y + a->h, b->y + b->h);

  rect.x = x0;
  rect.y = y0;
  rect.w = float_max(0.0f, x1 - x0);
  rect.h = float_max(0.0f, y1 - y0);

  return rect;
}

LDKPoint ldk_point(i32 x, i32 y)
{
  LDKPoint point = {.x = x, .y = y};
  return point;
}

LDKPointf ldk_pointf(float x, float y)
{
  LDKPointf point = {.x = x, .y = y};
  return point;
}

LDKRGB ldk_rgb(u8 r, u8 g, u8 b)
{
  LDKRGB rgb = {.r = r, .g = g, .b = b};
  return rgb;
}

LDKRGBA ldk_rgba(u8 r, u8 g, u8 b, u8 a)
{
  LDKRGBA rgba = {.r = r, .g = g, .b = b, .a = a};
  return rgba;
}

// ---------------------------------------------------------------------------
// Built-in mesh primitives
// ---------------------------------------------------------------------------

#define LDK_MESH_PRIMITIVE_COLOR 0xFFFFFFFFu
#define LDK_MESH_SPHERE_SLICES 24u
#define LDK_MESH_SPHERE_STACKS 12u
#define LDK_MESH_CAPSULE_SLICES 24u
#define LDK_MESH_CAPSULE_HEMISPHERE_RINGS 6u

static bool s_mesh_data_allocate(
    LDKMeshData* mesh, u32 vertex_count, u32 index_count)
{
  if (!mesh || vertex_count == 0 || index_count == 0)
  {
    return false;
  }

  memset(mesh, 0, sizeof(*mesh));

  mesh->vertices =
      (LDKMeshVertex*)malloc(sizeof(LDKMeshVertex) * (size_t)vertex_count);
  if (!mesh->vertices)
  {
    return false;
  }

  mesh->indices = (u32*)malloc(sizeof(u32) * (size_t)index_count);
  if (!mesh->indices)
  {
    free(mesh->vertices);
    memset(mesh, 0, sizeof(*mesh));
    return false;
  }

  mesh->vertex_count = vertex_count;
  mesh->index_count = index_count;
  return true;
}

void ldk_mesh_data_destroy(LDKMeshData* mesh)
{
  if (!mesh)
  {
    return;
  }

  free(mesh->vertices);
  free(mesh->indices);
  memset(mesh, 0, sizeof(*mesh));
}

static bool s_mesh_primitive_cube_create(LDKMeshData* mesh)
{
  static const LDKMeshVertex vertices[] =
  {
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},

    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},

    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},

    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},

    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},

    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
  };

  static const u32 indices[] =
  {
    0, 2, 1, 0, 3, 2,
    4, 6, 5, 4, 7, 6,
    8, 10, 9, 8, 11, 10,
    12, 14, 13, 12, 15, 14,
    16, 18, 17, 16, 19, 18,
    20, 22, 21, 20, 23, 22
  };

  if (!s_mesh_data_allocate(mesh, 24u, 36u))
  {
    return false;
  }

  memcpy(mesh->vertices, vertices, sizeof(vertices));
  memcpy(mesh->indices, indices, sizeof(indices));
  return true;
}

static bool s_mesh_primitive_plane_create(LDKMeshData* mesh)
{
  static const LDKMeshVertex vertices[] =
  {
    {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
  };
  static const u32 indices[] = {0, 2, 1, 0, 3, 2};

  if (!s_mesh_data_allocate(mesh, 4u, 6u))
  {
    return false;
  }

  memcpy(mesh->vertices, vertices, sizeof(vertices));
  memcpy(mesh->indices, indices, sizeof(indices));
  return true;
}

static bool s_mesh_primitive_quad_create(LDKMeshData* mesh)
{
  static const LDKMeshVertex vertices[] =
  {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, LDK_MESH_PRIMITIVE_COLOR},
  };
  static const u32 indices[] = {0, 1, 2, 0, 2, 3};

  if (!s_mesh_data_allocate(mesh, 4u, 6u))
  {
    return false;
  }

  memcpy(mesh->vertices, vertices, sizeof(vertices));
  memcpy(mesh->indices, indices, sizeof(indices));
  return true;
}

static void s_mesh_latitude_ring_write(LDKMeshVertex* vertices, u32 ring,
    u32 slices, float radius, float center_y, float latitude, float v)
{
  const float pi = 3.14159265358979323846f;
  float ring_radius = radius * cosf(latitude);
  float normal_y = sinf(latitude);
  float y = center_y + radius * normal_y;
  u32 stride = slices + 1u;

  for (u32 slice = 0; slice <= slices; ++slice)
  {
    float u = (float)slice / (float)slices;
    float longitude = u * pi * 2.0f;
    float normal_x = cosf(latitude) * cosf(longitude);
    float normal_z = cosf(latitude) * sinf(longitude);
    LDKMeshVertex* vertex = &vertices[ring * stride + slice];

    vertex->position = vec3_make(
        ring_radius * cosf(longitude), y, ring_radius * sinf(longitude));
    vertex->normal = vec3_make(normal_x, normal_y, normal_z);
    vertex->uv = vec2_make(u, v);
    vertex->color = LDK_MESH_PRIMITIVE_COLOR;
  }
}

static void s_mesh_ring_indices_write(
    u32* indices, u32 ring_count, u32 slices)
{
  u32 stride = slices + 1u;
  u32 index = 0;

  for (u32 ring = 0; ring + 1u < ring_count; ++ring)
  {
    for (u32 slice = 0; slice < slices; ++slice)
    {
      u32 bottom_left = ring * stride + slice;
      u32 top_left = (ring + 1u) * stride + slice;
      u32 bottom_right = bottom_left + 1u;
      u32 top_right = top_left + 1u;

      indices[index++] = bottom_left;
      indices[index++] = top_left;
      indices[index++] = bottom_right;

      indices[index++] = bottom_right;
      indices[index++] = top_left;
      indices[index++] = top_right;
    }
  }
}

static bool s_mesh_primitive_sphere_create(LDKMeshData* mesh)
{
  const float pi = 3.14159265358979323846f;
  const float radius = 0.5f;
  const u32 slices = LDK_MESH_SPHERE_SLICES;
  const u32 ring_count = LDK_MESH_SPHERE_STACKS + 1u;
  const u32 vertex_count = ring_count * (slices + 1u);
  const u32 index_count = (ring_count - 1u) * slices * 6u;

  if (!s_mesh_data_allocate(mesh, vertex_count, index_count))
  {
    return false;
  }

  for (u32 ring = 0; ring < ring_count; ++ring)
  {
    float v = (float)ring / (float)(ring_count - 1u);
    float latitude = -pi * 0.5f + v * pi;
    s_mesh_latitude_ring_write(
        mesh->vertices, ring, slices, radius, 0.0f, latitude, v);
  }

  s_mesh_ring_indices_write(mesh->indices, ring_count, slices);
  return true;
}

static bool s_mesh_primitive_capsule_create(LDKMeshData* mesh)
{
  const float pi = 3.14159265358979323846f;
  const float radius = 0.5f;
  const float cylinder_half_height = 0.5f;
  const u32 slices = LDK_MESH_CAPSULE_SLICES;
  const u32 hemisphere_rings = LDK_MESH_CAPSULE_HEMISPHERE_RINGS;
  const u32 ring_count = (hemisphere_rings + 1u) * 2u;
  const u32 vertex_count = ring_count * (slices + 1u);
  const u32 index_count = (ring_count - 1u) * slices * 6u;
  u32 ring = 0;

  if (!s_mesh_data_allocate(mesh, vertex_count, index_count))
  {
    return false;
  }

  for (u32 i = 0; i <= hemisphere_rings; ++i)
  {
    float t = (float)i / (float)hemisphere_rings;
    float latitude = -pi * 0.5f + t * pi * 0.5f;
    float y = -cylinder_half_height + radius * sinf(latitude);
    float v = (y + 1.0f) * 0.5f;

    s_mesh_latitude_ring_write(mesh->vertices, ring++, slices, radius,
        -cylinder_half_height, latitude, v);
  }

  for (u32 i = 0; i <= hemisphere_rings; ++i)
  {
    float t = (float)i / (float)hemisphere_rings;
    float latitude = t * pi * 0.5f;
    float y = cylinder_half_height + radius * sinf(latitude);
    float v = (y + 1.0f) * 0.5f;

    s_mesh_latitude_ring_write(mesh->vertices, ring++, slices, radius,
        cylinder_half_height, latitude, v);
  }

  s_mesh_ring_indices_write(mesh->indices, ring_count, slices);
  return true;
}

bool ldk_mesh_primitive_create(
    LDKMeshPrimitive primitive, LDKMeshData* out_mesh)
{
  if (!out_mesh)
  {
    return false;
  }

  memset(out_mesh, 0, sizeof(*out_mesh));

  switch (primitive)
  {
  case LDK_MESH_PRIMITIVE_CUBE:
    return s_mesh_primitive_cube_create(out_mesh);
  case LDK_MESH_PRIMITIVE_SPHERE:
    return s_mesh_primitive_sphere_create(out_mesh);
  case LDK_MESH_PRIMITIVE_CAPSULE:
    return s_mesh_primitive_capsule_create(out_mesh);
  case LDK_MESH_PRIMITIVE_PLANE:
    return s_mesh_primitive_plane_create(out_mesh);
  case LDK_MESH_PRIMITIVE_QUAD:
    return s_mesh_primitive_quad_create(out_mesh);
  default:
    return false;
  }
}

typedef struct LDKPrimitiveAssetFindContext
{
  const char* path;
  LDKAssetMesh mesh;
} LDKPrimitiveAssetFindContext;

static bool s_mesh_primitive_asset_find(
    LDKAssetHandle asset, LDKAssetInfo* info, void* user)
{
  LDKPrimitiveAssetFindContext* context =
      (LDKPrimitiveAssetFindContext*)user;

  if (!context || !info || info->type != LDK_ASSET_TYPE_MESH)
  {
    return true;
  }

  if (strcmp(x_fs_path_cstr(&info->asset_path), context->path) != 0)
  {
    return true;
  }

  context->mesh.h = asset.h;
  return false;
}

static const char* s_mesh_primitive_asset_path(LDKMeshPrimitive primitive)
{
  switch (primitive)
  {
  case LDK_MESH_PRIMITIVE_CUBE:
    return "builtin:mesh/cube";
  case LDK_MESH_PRIMITIVE_SPHERE:
    return "builtin:mesh/sphere";
  case LDK_MESH_PRIMITIVE_CAPSULE:
    return "builtin:mesh/capsule";
  case LDK_MESH_PRIMITIVE_PLANE:
    return "builtin:mesh/plane";
  case LDK_MESH_PRIMITIVE_QUAD:
    return "builtin:mesh/quad";
  default:
    return NULL;
  }
}

LDKAssetMesh ldk_mesh_primitive_asset_get(
    LDKAssetManager* manager, LDKMeshPrimitive primitive)
{
  LDKAssetMesh result = ldk_asset_mesh_null();
  LDKMeshData mesh = {0};
  const char* path;

  if (!manager)
  {
    return result;
  }

  path = s_mesh_primitive_asset_path(primitive);
  if (!path)
  {
    return result;
  }

  LDKPrimitiveAssetFindContext context = {
      .path = path,
      .mesh = ldk_asset_mesh_null(),
  };

  ldk_asset_foreach(manager, s_mesh_primitive_asset_find, &context);
  if (!x_handle_is_null(context.mesh.h))
  {
    return context.mesh;
  }

  if (!ldk_mesh_primitive_create(primitive, &mesh))
  {
    return result;
  }

  result = ldk_asset_manager_mesh_create(
      manager, mesh.vertices, mesh.vertex_count, mesh.indices, mesh.index_count);
  ldk_mesh_data_destroy(&mesh);

  if (x_handle_is_null(result.h))
  {
    return result;
  }

  LDKAssetHandle generic = {result.h};
  LDKAssetInfo* info = ldk_asset_get_info(manager, generic);
  if (!info)
  {
    ldk_asset_manager_mesh_unload(manager, result);
    return ldk_asset_mesh_null();
  }

  x_fs_path_set(&info->asset_path, path);
  return result;
}
