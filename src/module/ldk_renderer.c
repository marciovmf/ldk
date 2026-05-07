#include <ldk_common.h>
#include <module/ldk_renderer.h>

#include <stddef.h>
#include <string.h>

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer);
static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer);
static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer);

typedef struct LDKRendererUIParams
{
  float viewport_size[2];
} LDKRendererUIParams;

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(LDKRHIContext* rhi, u32 shader, u32 stage);

inline LDKRHIColor ldk_renderer_color_from_rgba32(u32 color)
{
  LDKRHIColor result = {0};
  result.r = (float)((color >> 24) & 0xffu) / 255.0f;
  result.g = (float)((color >> 16) & 0xffu) / 255.0f;
  result.b = (float)((color >> 8) & 0xffu) / 255.0f;
  result.a = (float)(color & 0xffu) / 255.0f;

  return result;
}

// ---------------------------------------------------------------------------
// Internal renderer resources: Mesh
// ---------------------------------------------------------------------------

static bool s_renderer_resource_key_equal(LDKRendererResourceKey a, LDKRendererResourceKey b)
{
  return a.id == b.id && a.version == b.version;
}

LDKRendererMesh ldk_renderer_mesh_null(void)
{
  LDKRendererMesh mesh = {0};
  return mesh;
}

static LDKRendererMeshResource* s_renderer_mesh_get_resource(LDKRenderer* renderer, LDKRendererMesh mesh)
{
  if (renderer == NULL || mesh.id == 0)
  {
    return NULL;
  }

  u32 index = (u32)(mesh.id - 1u);
  if (index >= renderer->mesh_count)
  {
    return NULL;
  }

  LDKRendererMeshResource* resource = &renderer->meshes[index];
  if (!resource->alive)
  {
    return NULL;
  }

  return resource;
}

bool ldk_renderer_mesh_is_valid(LDKRenderer* renderer, LDKRendererMesh mesh)
{
  return s_renderer_mesh_get_resource(renderer, mesh) != NULL;
}

static bool s_renderer_grow_mesh_cache(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->mesh_capacity == 0 ? 64 : renderer->mesh_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererMeshResource);
  LDKRendererMeshResource* new_meshes = renderer->meshes == NULL
    ? (LDKRendererMeshResource*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererMeshResource*)LDK_RENDERER_REALLOC(renderer->meshes, new_size);

  if (new_meshes == NULL)
  {
    return false;
  }

  memset(
      new_meshes + renderer->mesh_capacity,
      0,
      (size_t)(new_capacity - renderer->mesh_capacity) * sizeof(LDKRendererMeshResource));

  renderer->meshes = new_meshes;
  renderer->mesh_capacity = new_capacity;
  return true;
}

static bool s_renderer_mesh_desc_is_valid(LDKRendererMeshDesc const* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->vertices == NULL || desc->vertex_count == 0)
  {
    return false;
  }

  if (desc->indices == NULL || desc->index_count == 0)
  {
    return false;
  }

  return true;
}

static bool s_renderer_mesh_resource_create_buffers(
    LDKRenderer* renderer,
    LDKRendererMeshResource* resource,
    LDKRendererMeshDesc const* desc)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = desc->vertex_count * (u32)sizeof(LDKMeshVertex);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = desc->vertices;

  resource->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (resource->vertex_buffer == LDK_RHI_INVALID_BUFFER)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = desc->index_count * (u32)sizeof(u32);
  index_desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  index_desc.initial_data = desc->indices;

  resource->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (resource->index_buffer == LDK_RHI_INVALID_BUFFER)
  {
    ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
    resource->vertex_buffer = LDK_RHI_INVALID_BUFFER;
    return false;
  }

  resource->vertex_count = desc->vertex_count;
  resource->index_count = desc->index_count;
  return true;
}

LDKRendererMesh ldk_renderer_mesh_create(LDKRenderer* renderer, LDKRendererMeshDesc const* desc)
{
  LDKRendererMesh invalid = ldk_renderer_mesh_null();

  if (renderer == NULL || !renderer->is_initialized || !s_renderer_mesh_desc_is_valid(desc))
  {
    return invalid;
  }

  if (renderer->mesh_count == renderer->mesh_capacity)
  {
    if (!s_renderer_grow_mesh_cache(renderer))
    {
      return invalid;
    }
  }

  u32 index = renderer->mesh_count;
  LDKRendererMeshResource* resource = &renderer->meshes[index];
  memset(resource, 0, sizeof(*resource));

  if (!s_renderer_mesh_resource_create_buffers(renderer, resource, desc))
  {
    memset(resource, 0, sizeof(*resource));
    return invalid;
  }

  resource->alive = true;
  renderer->mesh_count += 1;

  LDKRendererMesh mesh = {0};
  mesh.id = (uintptr_t)(index + 1u);
  return mesh;
}

static LDKRendererMesh s_renderer_mesh_find_by_key(LDKRenderer* renderer, LDKRendererResourceKey key)
{
  if (renderer == NULL || key.id == 0)
  {
    return ldk_renderer_mesh_null();
  }

  for (u32 i = 0; i < renderer->mesh_count; i++)
  {
    LDKRendererMeshResource* resource = &renderer->meshes[i];
    if (resource->alive && s_renderer_resource_key_equal(resource->key, key))
    {
      LDKRendererMesh mesh = {0};
      mesh.id = (uintptr_t)(i + 1u);
      return mesh;
    }
  }

  return ldk_renderer_mesh_null();
}

LDKRendererMesh ldk_renderer_mesh_get_or_create(
    LDKRenderer* renderer,
    LDKRendererResourceKey key,
    LDKRendererMeshDesc const* desc)
{
  if (key.id == 0)
  {
    return ldk_renderer_mesh_create(renderer, desc);
  }

  LDKRendererMesh mesh = s_renderer_mesh_find_by_key(renderer, key);
  if (ldk_renderer_mesh_is_valid(renderer, mesh))
  {
    return mesh;
  }

  mesh = ldk_renderer_mesh_create(renderer, desc);
  LDKRendererMeshResource* resource = s_renderer_mesh_get_resource(renderer, mesh);
  if (resource != NULL)
  {
    resource->key = key;
  }

  return mesh;
}

bool ldk_renderer_mesh_update(LDKRenderer* renderer, LDKRendererMesh mesh, LDKRendererMeshDesc const* desc)
{
  if (renderer == NULL || !renderer->is_initialized || !s_renderer_mesh_desc_is_valid(desc))
  {
    return false;
  }

  LDKRendererMeshResource* resource = s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return false;
  }

  ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->index_buffer);
  resource->vertex_buffer = LDK_RHI_INVALID_BUFFER;
  resource->index_buffer = LDK_RHI_INVALID_BUFFER;
  resource->vertex_count = 0;
  resource->index_count = 0;

  if (!s_renderer_mesh_resource_create_buffers(renderer, resource, desc))
  {
    resource->alive = false;
    return false;
  }

  return true;
}

void ldk_renderer_mesh_destroy(LDKRenderer* renderer, LDKRendererMesh mesh)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return;
  }

  LDKRendererMeshResource* resource = s_renderer_mesh_get_resource(renderer, mesh);
  if (resource == NULL)
  {
    return;
  }

  ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
  ldk_rhi_buffer_destroy(renderer->rhi, resource->index_buffer);
  memset(resource, 0, sizeof(*resource));
}

static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->mesh_count; i++)
    {
      LDKRendererMesh mesh = {0};
      mesh.id = (uintptr_t)(i + 1u);
      ldk_renderer_mesh_destroy(renderer, mesh);
    }
  }

  LDK_RENDERER_FREE(renderer->meshes);
  renderer->meshes = NULL;
  renderer->mesh_count = 0;
  renderer->mesh_capacity = 0;

  LDK_RENDERER_FREE(renderer->submitted_meshes);
  renderer->submitted_meshes = NULL;
  renderer->submitted_mesh_count = 0;
  renderer->submitted_mesh_capacity = 0;
}

static bool s_renderer_grow_mesh_submit_queue(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->submitted_mesh_capacity == 0 ? 256 : renderer->submitted_mesh_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererMeshSubmit);
  LDKRendererMeshSubmit* new_submits = renderer->submitted_meshes == NULL
    ? (LDKRendererMeshSubmit*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererMeshSubmit*)LDK_RENDERER_REALLOC(renderer->submitted_meshes, new_size);

  if (new_submits == NULL)
  {
    return false;
  }

  renderer->submitted_meshes = new_submits;
  renderer->submitted_mesh_capacity = new_capacity;
  return true;
}

// ---------------------------------------------------------------------------
// Internal pass: UI
// ---------------------------------------------------------------------------

static bool s_renderer_ui_pass_create_shaders(LDKRendererUIPass* renderer)
{
  renderer->vertex_shader_module = ldk_rhi_create_builtin_shader_module(renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (renderer->vertex_shader_module == LDK_RHI_INVALID_SHADER_MODULE)
  {
    return false;
  }

  renderer->fragment_shader_module = ldk_rhi_create_builtin_shader_module(renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (renderer->fragment_shader_module == LDK_RHI_INVALID_SHADER_MODULE)
  {
    ldk_rhi_shader_module_destroy(renderer->rhi, renderer->vertex_shader_module);
    renderer->vertex_shader_module = LDK_RHI_INVALID_SHADER_MODULE;
    return false;
  }

  return true;
}

static bool s_renderer_ui_pass_bindings_create_layout(LDKRendererUIPass* renderer)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 2;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;

  renderer->bindings_layout = ldk_rhi_bindings_layout_create(renderer->rhi, &desc);
  return renderer->bindings_layout != LDK_RHI_INVALID_BINDINGS_LAYOUT;
}

static bool s_renderer_ui_pass_create_pipeline(LDKRendererUIPass* renderer)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);
  desc.vertex_shader_module = renderer->vertex_shader_module;
  desc.fragment_shader_module = renderer->fragment_shader_module;
  desc.bindings_layout = renderer->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor = LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.raster_state.scissor_enabled = true;
  desc.vertex_layout.stride = sizeof(LDKUIVertex);
  desc.vertex_layout.attribute_count = 3;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[0].offset = (u32)offsetof(LDKUIVertex, x);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[1].offset = (u32)offsetof(LDKUIVertex, u);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[2].offset = (u32)offsetof(LDKUIVertex, color);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_INVALID;

  renderer->pipeline = ldk_rhi_pipeline_create(renderer->rhi, &desc);
  return renderer->pipeline != LDK_RHI_INVALID_PIPELINE;
}

static bool s_renderer_ui_pass_create_white_texture(LDKRendererUIPass* renderer)
{
  u32 pixel = 0xffffffffu;
  LDKRHITextureDesc texture_desc = {0};
  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  texture_desc.width = 1;
  texture_desc.height = 1;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = &pixel;
  texture_desc.initial_data_size = sizeof(pixel);

  renderer->white_texture = ldk_rhi_texture_create(renderer->rhi, &texture_desc);
  return renderer->white_texture != LDK_RHI_INVALID_TEXTURE;
}

static bool s_renderer_ui_pass_create_sampler(LDKRendererUIPass* renderer)
{
  LDKRHISamplerDesc desc = {0};
  ldk_rhi_sampler_desc_defaults(&desc);
  desc.min_filter = LDK_RHI_FILTER_NEAREST;
  desc.mag_filter = LDK_RHI_FILTER_NEAREST;
  desc.mip_filter = LDK_RHI_FILTER_NEAREST;
  desc.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;

  renderer->sampler = ldk_rhi_sampler_create(renderer->rhi, &desc);
  return renderer->sampler != LDK_RHI_INVALID_SAMPLER;
}

static bool s_renderer_ui_pass_buffer_creates(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (renderer->vertex_buffer == LDK_RHI_INVALID_BUFFER)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = renderer->index_capacity * (u32)sizeof(u32);
  index_desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (renderer->index_buffer == LDK_RHI_INVALID_BUFFER)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererUIParams);
  params_desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->params_buffer = ldk_rhi_buffer_create(renderer->rhi, &params_desc);
  return renderer->params_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool s_renderer_ui_pass_recreate_vertex_buffer(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
  renderer->vertex_buffer = LDK_RHI_INVALID_BUFFER;
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->vertex_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool s_renderer_ui_pass_recreate_index_buffer(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
  renderer->index_buffer = LDK_RHI_INVALID_BUFFER;
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->index_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool s_renderer_ui_pass_ensure_vertex_capacity(LDKRendererUIPass* renderer, u32 vertex_count)
{
  if (vertex_count <= renderer->vertex_capacity)
  {
    return true;
  }

  while (renderer->vertex_capacity < vertex_count)
  {
    renderer->vertex_capacity *= 2;
  }

  return s_renderer_ui_pass_recreate_vertex_buffer(renderer);
}

static bool s_renderer_ui_pass_ensure_index_capacity(LDKRendererUIPass* renderer, u32 index_count)
{
  if (index_count <= renderer->index_capacity)
  {
    return true;
  }

  while (renderer->index_capacity < index_count)
  {
    renderer->index_capacity *= 2;
  }

  return s_renderer_ui_pass_recreate_index_buffer(renderer);
}

static bool s_renderer_ui_pass_initialize(LDKRendererUIPass* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;
  renderer->vertex_capacity = config->initial_ui_vertex_capacity > 0 ? config->initial_ui_vertex_capacity : 1024;
  renderer->index_capacity = config->initial_ui_index_capacity > 0 ? config->initial_ui_index_capacity : 2048;

  if (!s_renderer_ui_pass_create_shaders(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_bindings_create_layout(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_pipeline(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_white_texture(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_create_sampler(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  if (!s_renderer_ui_pass_buffer_creates(renderer))
  {
    s_renderer_ui_pass_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;
  return true;
}

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
    {
      ldk_rhi_bindings_destroy(renderer->rhi, renderer->bindings_cache[i].bindings);
    }

    ldk_rhi_buffer_destroy(renderer->rhi, renderer->params_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
    ldk_rhi_destroy_sampler(renderer->rhi, renderer->sampler);
    ldk_rhi_texture_destroy(renderer->rhi, renderer->white_texture);
    ldk_rhi_pipeline_destroy(renderer->rhi, renderer->pipeline);
    ldk_rhi_bindings_layout_destroy(renderer->rhi, renderer->bindings_layout);
    ldk_rhi_shader_module_destroy(renderer->rhi, renderer->fragment_shader_module);
    ldk_rhi_shader_module_destroy(renderer->rhi, renderer->vertex_shader_module);
  }

  LDK_RENDERER_FREE(renderer->bindings_cache);
  memset(renderer, 0, sizeof(*renderer));
}

static LDKRHIBindings s_renderer_ui_pass_create_draw_bindings(LDKRendererUIPass* renderer, LDKRHITexture texture)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = renderer->bindings_layout;
  desc.binding_count = 2;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = renderer->params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererUIParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].texture = texture;
  desc.bindings[1].sampler = renderer->sampler;
  return ldk_rhi_bindings_create(renderer->rhi, &desc);
}

static LDKRHIBindings s_renderer_ui_pass_find_cached_bindings(LDKRendererUIPass* renderer, LDKRHITexture texture)
{
  for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
  {
    if (renderer->bindings_cache[i].texture == texture)
    {
      return renderer->bindings_cache[i].bindings;
    }
  }

  return LDK_RHI_INVALID_BINDINGS;
}

static bool s_renderer_ui_pass_grow_bindings_cache(LDKRendererUIPass* renderer)
{
  u32 new_capacity = renderer->bindings_cache_capacity == 0 ? 16 : renderer->bindings_cache_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererBindingsCacheEntry);
  LDKRendererBindingsCacheEntry* new_cache = renderer->bindings_cache == NULL
    ? (LDKRendererBindingsCacheEntry*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererBindingsCacheEntry*)LDK_RENDERER_REALLOC(renderer->bindings_cache, new_size);
  if (new_cache == NULL)
  {
    return false;
  }

  renderer->bindings_cache = new_cache;
  renderer->bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_ui_pass_get_draw_bindings(LDKRendererUIPass* renderer, LDKRHITexture texture)
{
  LDKRHIBindings cached_bindings = s_renderer_ui_pass_find_cached_bindings(renderer, texture);
  if (cached_bindings != LDK_RHI_INVALID_BINDINGS)
  {
    return cached_bindings;
  }

  if (renderer->bindings_cache_count == renderer->bindings_cache_capacity)
  {
    if (!s_renderer_ui_pass_grow_bindings_cache(renderer))
    {
      return LDK_RHI_INVALID_BINDINGS;
    }
  }

  LDKRHIBindings bindings = s_renderer_ui_pass_create_draw_bindings(renderer, texture);
  if (bindings == LDK_RHI_INVALID_BINDINGS)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  LDKRendererBindingsCacheEntry* entry = &renderer->bindings_cache[renderer->bindings_cache_count];
  entry->texture = texture;
  entry->bindings = bindings;
  renderer->bindings_cache_count += 1;
  return bindings;
}

static void s_renderer_ui_pass(LDKRendererUIPass* renderer, LDKUIRenderData const* render_data, const LDKRendererFrameDesc* frame_desc)
{
  if (renderer == NULL || !renderer->is_initialized || render_data == NULL || frame_desc == NULL)
  {
    return;
  }

  i32 framebuffer_width = frame_desc->framebuffer_width;
  i32 framebuffer_height = frame_desc->framebuffer_height;

  if (framebuffer_width <= 0 || framebuffer_height <= 0)
  {
    return;
  }

  if (render_data->vertex_count == 0 || render_data->index_count == 0 || render_data->command_count == 0)
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_vertex_capacity(renderer, render_data->vertex_count))
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_index_capacity(renderer, render_data->index_count))
  {
    return;
  }

  LDKRHIPassDesc pass_desc;
  ldk_rhi_pass_desc_defaults(&pass_desc);

  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = LDK_RHI_INVALID_TEXTURE;
  pass_desc.color_attachments[0].load_op = frame_desc->clear_color_enabled ? LDK_RHI_LOAD_OP_CLEAR : LDK_RHI_LOAD_OP_LOAD;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.color_attachments[0].clear_color = ldk_renderer_color_from_rgba32(frame_desc->clear_color);
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)framebuffer_width;
  pass_desc.viewport.height = (float)framebuffer_height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);

  LDKRendererUIParams params = {0};
  params.viewport_size[0] = (float)framebuffer_width;
  params.viewport_size[1] = (float)framebuffer_height;

  ldk_rhi_buffer_update(renderer->rhi, renderer->params_buffer, 0, sizeof(params), &params);
  ldk_rhi_buffer_update(renderer->rhi, renderer->vertex_buffer, 0, render_data->vertex_count * (u32)sizeof(LDKUIVertex), render_data->vertices);
  ldk_rhi_buffer_update(renderer->rhi, renderer->index_buffer, 0, render_data->index_count * (u32)sizeof(u32), render_data->indices);

  ldk_rhi_pipeline_bind(renderer->rhi, renderer->pipeline);
  ldk_rhi_vertex_buffer_bind(renderer->rhi, renderer->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(renderer->rhi, renderer->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < render_data->command_count; ++i)
  {
    LDKUIDrawCmd const* cmd = &render_data->commands[i];
    i32 scissor_x = (i32)cmd->clip_rect.x;
    i32 scissor_y = framebuffer_height - (i32)(cmd->clip_rect.y + cmd->clip_rect.h);
    i32 scissor_w = (i32)cmd->clip_rect.w;
    i32 scissor_h = (i32)cmd->clip_rect.h;
    LDKRHITexture texture = cmd->texture != 0 ? (LDKRHITexture)cmd->texture : renderer->white_texture;

    if (scissor_w <= 0 || scissor_h <= 0 || cmd->index_count == 0)
    {
      continue;
    }

    LDKRHIBindings bindings = s_renderer_ui_pass_get_draw_bindings(renderer, texture);
    if (bindings == LDK_RHI_INVALID_BINDINGS)
    {
      continue;
    }

    LDKRHIRect scissor = {0};
    scissor.x = scissor_x;
    scissor.y = scissor_y;
    scissor.width = scissor_w;
    scissor.height = scissor_h;
    ldk_rhi_scissor_set(renderer->rhi, &scissor);
    ldk_rhi_bindings_bind(renderer->rhi, bindings);

    LDKRHIDrawIndexedDesc draw_desc = {0};
    draw_desc.index_count = cmd->index_count;
    draw_desc.first_index = cmd->index_offset;
    draw_desc.vertex_offset = 0;
    ldk_rhi_draw_indexed(renderer->rhi, &draw_desc);
  }

  ldk_rhi_pass_end(renderer->rhi);
}

// ---------------------------------------------------------------------------
// Renderer lifecycle
// ---------------------------------------------------------------------------

bool ldk_renderer_initialize(LDKRenderer* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;

  if (!s_renderer_ui_pass_initialize(&renderer->ui_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;
  return true;
}

void ldk_renderer_terminate(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  s_renderer_ui_pass_terminate(&renderer->ui_pass);
  s_renderer_destroy_font_page_cache(renderer);
  s_renderer_destroy_mesh_resources(renderer);
  memset(renderer, 0, sizeof(*renderer));
}

void ldk_renderer_submit_ui(LDKRenderer* renderer, LDKUIRenderData const* render_data)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return;
  }

  renderer->submitted_ui = render_data;
}

void ldk_renderer_render_frame(LDKRenderer* renderer, LDKRendererFrameDesc const* desc)
{
  if (renderer == NULL || !renderer->is_initialized || desc == NULL)
  {
    return;
  }

  if (desc->framebuffer_width <= 0 || desc->framebuffer_height <= 0)
  {
    renderer->submitted_ui = NULL;
    renderer->submitted_mesh_count = 0;
    return;
  }

  ldk_rhi_frame_begin(renderer->rhi);

  // Mesh pass will consume renderer->submitted_meshes here.
  s_renderer_ui_pass(&renderer->ui_pass, renderer->submitted_ui, desc);

  ldk_rhi_frame_end(renderer->rhi);

  renderer->submitted_ui = NULL;
  renderer->submitted_mesh_count = 0;
  renderer->has_camera = false;
}

// ---------------------------------------------------------------------------
// Primitive Submission
// ---------------------------------------------------------------------------

bool ldk_renderer_submit_view(LDKRenderer* renderer, Mat4 view, Mat4 projection)
{
  if (!renderer || !renderer->is_initialized)
  {
    return false;
  }

  renderer->camera_view = view;
  renderer->camera_projection = projection;
  renderer->has_camera = true;

  return true;
}

bool ldk_renderer_submit_mesh(LDKRenderer* renderer, LDKRendererMesh mesh, Mat4 world)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return false;
  }

  if (!ldk_renderer_mesh_is_valid(renderer, mesh))
  {
    return false;
  }

  if (renderer->submitted_mesh_count == renderer->submitted_mesh_capacity)
  {
    if (!s_renderer_grow_mesh_submit_queue(renderer))
    {
      return false;
    }
  }

  LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[renderer->submitted_mesh_count];
  submit->mesh = mesh;
  submit->world = world;

  renderer->submitted_mesh_count += 1;
  return true;
}

// ---------------------------------------------------------------------------
// Font cache
// ---------------------------------------------------------------------------

static LDKRendererFontPageCacheEntry* s_renderer_find_font_page(
    LDKRenderer* renderer,
    LDKFontInstance* font,
    u32 page_index)
{
  if (!renderer || !font)
  {
    return NULL;
  }

  for (u32 i = 0; i < renderer->font_page_count; ++i)
  {
    LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[i];

    if (entry->font == font && entry->page_index == page_index)
    {
      return entry;
    }
  }

  return NULL;
}

static bool s_renderer_grow_font_page_cache(LDKRenderer* renderer)
{
  if (!renderer)
  {
    return false;
  }

  u32 new_capacity = renderer->font_page_capacity == 0 ? 16 : renderer->font_page_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererFontPageCacheEntry);

  LDKRendererFontPageCacheEntry* new_pages = renderer->font_pages == NULL
    ? (LDKRendererFontPageCacheEntry*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererFontPageCacheEntry*)LDK_RENDERER_REALLOC(renderer->font_pages, new_size);

  if (!new_pages)
  {
    return false;
  }

  renderer->font_pages = new_pages;
  renderer->font_page_capacity = new_capacity;

  return true;
}

static LDKRHITexture s_renderer_create_font_page_texture(
    LDKRenderer* renderer,
    const LDKFontPageInfo* page)
{
  if (!renderer || !renderer->rhi || !page)
  {
    return LDK_RHI_INVALID_TEXTURE;
  }

  if (!page->pixels || page->width == 0 || page->height == 0)
  {
    return LDK_RHI_INVALID_TEXTURE;
  }

  LDKRHITextureDesc desc = {0};
  ldk_rhi_texture_desc_defaults(&desc);

  desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  desc.format = LDK_RHI_FORMAT_R8_UNORM;
  desc.width = page->width;
  desc.height = page->height;
  desc.depth = 1;
  desc.mip_count = 1;
  desc.layer_count = 1;
  desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED | LDK_RHI_TEXTURE_USAGE_TRANSFER_DST;
  desc.initial_data = page->pixels;
  desc.initial_data_size = page->width * page->height;

  desc.swizzle_r = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  desc.swizzle_g = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  desc.swizzle_b = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  desc.swizzle_a = LDK_RHI_TEXTURE_SWIZZLE_R;

  return ldk_rhi_texture_create(renderer->rhi, &desc);
}

static bool s_renderer_update_font_page_texture(
    LDKRenderer* renderer,
    LDKRendererFontPageCacheEntry* entry,
    const LDKFontPageInfo* page)
{
  if (!renderer || !renderer->rhi || !entry || !page)
  {
    return false;
  }

  if (!page->pixels || page->width == 0 || page->height == 0)
  {
    return false;
  }

  if (entry->texture == LDK_RHI_INVALID_TEXTURE)
  {
    entry->texture = s_renderer_create_font_page_texture(renderer, page);
    entry->width = page->width;
    entry->height = page->height;
    return entry->texture != LDK_RHI_INVALID_TEXTURE;
  }

  if (entry->width != page->width || entry->height != page->height)
  {
    ldk_rhi_texture_destroy(renderer->rhi, entry->texture);

    entry->texture = s_renderer_create_font_page_texture(renderer, page);
    entry->width = page->width;
    entry->height = page->height;

    return entry->texture != LDK_RHI_INVALID_TEXTURE;
  }

  return ldk_rhi_texture_update(
      renderer->rhi,
      entry->texture,
      0,
      0,
      page->pixels,
      page->width * page->height);
}

static LDKRendererFontPageCacheEntry* s_renderer_create_font_page_entry(
    LDKRenderer* renderer,
    LDKFontInstance* font,
    const LDKFontPageInfo* page)
{
  if (!renderer || !font || !page)
  {
    return NULL;
  }

  if (renderer->font_page_count == renderer->font_page_capacity)
  {
    if (!s_renderer_grow_font_page_cache(renderer))
    {
      return NULL;
    }
  }

  LDKRHITexture texture = s_renderer_create_font_page_texture(renderer, page);

  if (texture == LDK_RHI_INVALID_TEXTURE)
  {
    return NULL;
  }

  LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[renderer->font_page_count];
  memset(entry, 0, sizeof(*entry));

  entry->font = font;
  entry->page_index = page->page_index;
  entry->width = page->width;
  entry->height = page->height;
  entry->texture = texture;

  renderer->font_page_count += 1;

  return entry;
}

static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer)
{
  if (!renderer)
  {
    return;
  }

  if (renderer->rhi)
  {
    for (u32 i = 0; i < renderer->font_page_count; ++i)
    {
      LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[i];

      if (entry->texture != LDK_RHI_INVALID_TEXTURE)
      {
        ldk_rhi_texture_destroy(renderer->rhi, entry->texture);
      }
    }
  }

  LDK_RENDERER_FREE(renderer->font_pages);

  renderer->font_pages = NULL;
  renderer->font_page_count = 0;
  renderer->font_page_capacity = 0;
}

LDKUITextureHandle ldk_renderer_get_font_page_texture(
    LDKRenderer* renderer,
    LDKFontInstance* font,
    u32 page_index)
{
  if (!renderer || !renderer->is_initialized || !font)
  {
    return 0;
  }

  LDKFontPageInfo page = {0};

  if (!ldk_font_get_page_info(font, page_index, &page))
  {
    return 0;
  }

  LDKRendererFontPageCacheEntry* entry = s_renderer_find_font_page(
      renderer,
      font,
      page_index);

  if (!entry)
  {
    entry = s_renderer_create_font_page_entry(renderer, font, &page);

    if (!entry)
    {
      return 0;
    }

    ldk_font_clear_page_dirty(font, page_index);
    return (LDKUITextureHandle)entry->texture;
  }

  if (page.dirty)
  {
    if (s_renderer_update_font_page_texture(renderer, entry, &page))
    {
      ldk_font_clear_page_dirty(font, page_index);
    }
  }

  return (LDKUITextureHandle)entry->texture;
}

LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(
    void* user,
    LDKFontInstance* font,
    u32 page_index)
{
  return ldk_renderer_get_font_page_texture(
      (LDKRenderer*)user,
      font,
      page_index);
}
