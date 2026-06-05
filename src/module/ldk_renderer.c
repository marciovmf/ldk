#include <ldk_common.h>
#include <module/ldk_renderer.h>

#include <stddef.h>
#include <string.h>

static void s_renderer_ui_pass_terminate(LDKRendererUIPass* renderer);
static void s_renderer_mesh_pass_terminate(LDKRendererMeshPass* pass);
static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer);
static void s_renderer_destroy_mesh_resources(LDKRenderer* renderer);
static void s_renderer_destroy_texture_resources(LDKRenderer* renderer);

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

static void s_renderer_target_destroy(LDKRenderer* renderer, LDKRendererTarget* target)
{
  if (renderer == NULL || renderer->rhi == NULL || target == NULL)
  {
    return;
  }

  if (target->color_texture != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, target->color_texture);
  }

  if (target->depth_texture != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, target->depth_texture);
  }

  memset(target, 0, sizeof(*target));
}

static bool s_renderer_target_create(LDKRenderer* renderer, LDKRendererTarget* target, i32 width, i32 height)
{
  if (renderer == NULL || renderer->rhi == NULL || target == NULL)
  {
    return false;
  }

  if (width <= 0 || height <= 0)
  {
    return false;
  }

  target->color_format = LDK_RHI_FORMAT_RGBA8_UNORM;
  target->depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  LDKRHITextureDesc color_desc = {0};
  ldk_rhi_texture_desc_defaults(&color_desc);
  color_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  color_desc.format = target->color_format;
  color_desc.width = (u32)width;
  color_desc.height = (u32)height;
  color_desc.depth = 1;
  color_desc.mip_count = 1;
  color_desc.layer_count = 1;
  color_desc.usage = LDK_RHI_TEXTURE_USAGE_RENDER_TARGET | LDK_RHI_TEXTURE_USAGE_SAMPLED;

  target->color_texture = ldk_rhi_texture_create(renderer->rhi, &color_desc);
  if (target->color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHITextureDesc depth_desc = {0};
  ldk_rhi_texture_desc_defaults(&depth_desc);
  depth_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  depth_desc.format = target->depth_format;
  depth_desc.width = (u32)width;
  depth_desc.height = (u32)height;
  depth_desc.depth = 1;
  depth_desc.mip_count = 1;
  depth_desc.layer_count = 1;
  depth_desc.usage = LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL;

  target->depth_texture = ldk_rhi_texture_create(renderer->rhi, &depth_desc);
  if (target->depth_texture == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_texture_destroy(renderer->rhi, target->color_texture);
    memset(target, 0, sizeof(*target));
    return false;
  }

  target->width = width;
  target->height = height;
  return true;
}

static bool s_renderer_target_ensure(LDKRenderer* renderer, LDKRendererTarget* target, i32 width, i32 height)
{
  if (target != NULL &&
      target->color_texture != LDK_RHI_INVALID_RESOURCE &&
      target->depth_texture != LDK_RHI_INVALID_RESOURCE &&
      target->width == width &&
      target->height == height)
  {
    return true;
  }

  s_renderer_target_destroy(renderer, target);
  return s_renderer_target_create(renderer, target, width, height);
}

// ---------------------------------------------------------------------------
// Internal renderer resources: Mesh
// ---------------------------------------------------------------------------

LDKResourceMesh ldk_renderer_mesh_null(void)
{
  return LDK_RESOURCE_MESH_INVALID;
}

static LDKRendererMeshResource* s_renderer_mesh_get_resource(
    LDKRenderer* renderer, LDKResourceMesh mesh)
{
  if (renderer == NULL || mesh.id == LDK_RHI_INVALID_RESOURCE)
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

bool ldk_renderer_mesh_is_valid(LDKRenderer* renderer, LDKResourceMesh mesh)
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

static bool s_renderer_mesh_resource_create_buffers(LDKRenderer* renderer,
    LDKRendererMeshResource* resource, LDKRendererMeshDesc const* desc)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = desc->vertex_count * (u32)sizeof(LDKMeshVertex);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = desc->vertices;

  resource->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (resource->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
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
  if (resource->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_buffer_destroy(renderer->rhi, resource->vertex_buffer);
    resource->vertex_buffer = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  resource->vertex_count = desc->vertex_count;
  resource->index_count = desc->index_count;
  return true;
}

LDKResourceMesh ldk_renderer_mesh_create(LDKRenderer* renderer, LDKRendererMeshDesc const* desc)
{
  LDKResourceMesh invalid = ldk_renderer_mesh_null();

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

  LDKResourceMesh mesh = {0};
  mesh.id = (LDKRHIResource)(index + 1u);
  return mesh;
}

bool ldk_renderer_mesh_update(LDKRenderer* renderer, LDKResourceMesh mesh, LDKRendererMeshDesc const* desc)
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
  resource->vertex_buffer = LDK_RHI_INVALID_RESOURCE;
  resource->index_buffer = LDK_RHI_INVALID_RESOURCE;
  resource->vertex_count = 0;
  resource->index_count = 0;

  if (!s_renderer_mesh_resource_create_buffers(renderer, resource, desc))
  {
    resource->alive = false;
    return false;
  }

  return true;
}

void ldk_renderer_mesh_destroy(LDKRenderer* renderer, LDKResourceMesh mesh)
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
      LDKResourceMesh mesh = {0};
      mesh.id = (LDKRHIResource)(i + 1u);
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

typedef struct LDKRendererMeshCameraParams
{
  Mat4 view;
  Mat4 projection;
} LDKRendererMeshCameraParams;

typedef struct LDKRendererMeshObjectParams
{
  Mat4 world;
} LDKRendererMeshObjectParams;

static bool s_renderer_mesh_pass_create_shaders(LDKRendererMeshPass* pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_mesh_pass_create_bindings_layout(LDKRendererMeshPass* pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 2;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;

  pass->bindings_layout = ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_pipeline(LDKRendererMeshPass* pass)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);

  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;
  desc.bindings_layout = pass->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = false;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_BACK;
  desc.raster_state.front_face = LDK_RHI_FRONT_FACE_CCW;
  desc.raster_state.scissor_enabled = false;

  desc.vertex_layout.stride = sizeof(LDKMeshVertex);
  desc.vertex_layout.attribute_count = 4;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset = (u32)offsetof(LDKMeshVertex, position);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[1].offset = (u32)offsetof(LDKMeshVertex, normal);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[2].offset = (u32)offsetof(LDKMeshVertex, uv);
  desc.vertex_layout.attributes[3].location = 3;
  desc.vertex_layout.attributes[3].format = LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[3].offset = (u32)offsetof(LDKMeshVertex, color);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  pass->pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  return pass->pipeline != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_buffers(LDKRendererMeshPass* pass)
{
  LDKRHIBufferDesc camera_desc = {0};
  ldk_rhi_buffer_desc_defaults(&camera_desc);
  camera_desc.size = sizeof(LDKRendererMeshCameraParams);
  camera_desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  camera_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->camera_buffer = ldk_rhi_buffer_create(pass->rhi, &camera_desc);
  if (pass->camera_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc object_desc = {0};
  ldk_rhi_buffer_desc_defaults(&object_desc);
  object_desc.size = sizeof(LDKRendererMeshObjectParams);
  object_desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  object_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->object_buffer = ldk_rhi_buffer_create(pass->rhi, &object_desc);
  return pass->object_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_bindings(LDKRendererMeshPass* pass)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 2;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererMeshCameraParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_offset = 0;
  desc.bindings[1].buffer_size = sizeof(LDKRendererMeshObjectParams);

  pass->bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  return pass->bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_initialize(LDKRendererMeshPass* pass, LDKRendererConfig const* config)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  if (!s_renderer_mesh_pass_create_shaders(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_bindings_layout(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_pipeline(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_buffers(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_mesh_pass_create_bindings(pass))
  {
    s_renderer_mesh_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_mesh_pass_terminate(LDKRendererMeshPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_mesh_pass(LDKRenderer* renderer, LDKRendererMeshPass* pass, LDKRendererFrameDesc const* frame_desc)
{
  if (renderer == NULL || pass == NULL || !pass->is_initialized || frame_desc == NULL)
  {
    return false;
  }

  if (!renderer->has_camera || renderer->submitted_mesh_count == 0)
  {
    return false;
  }

  if (frame_desc->framebuffer_width <= 0 || frame_desc->framebuffer_height <= 0)
  {
    return false;
  }

  if (!s_renderer_target_ensure(renderer, &renderer->scene_target, frame_desc->framebuffer_width, frame_desc->framebuffer_height))
  {
    return false;
  }

  LDKRHIPassDesc pass_desc;
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = renderer->scene_target.color_texture;
  pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.color_attachments[0].clear_color = ldk_renderer_color_from_rgba32(frame_desc->clear_color);
  pass_desc.depth_attachment.valid = true;
  pass_desc.depth_attachment.texture = renderer->scene_target.depth_texture;
  pass_desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.depth_attachment.depth_store_op = LDK_RHI_STORE_OP_DONT_CARE;
  pass_desc.depth_attachment.clear_depth = 1.0f;
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)frame_desc->framebuffer_width;
  pass_desc.viewport.height = (float)frame_desc->framebuffer_height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(pass->rhi, &pass_desc);

  LDKRendererMeshCameraParams camera_params = {0};
  camera_params.view = renderer->camera_view;
  camera_params.projection = renderer->camera_projection;
  ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0, sizeof(camera_params), &camera_params);
  ldk_rhi_pipeline_bind(pass->rhi, pass->pipeline);
  ldk_rhi_bindings_bind(pass->rhi, pass->bindings);

  for (u32 i = 0; i < renderer->submitted_mesh_count; i++)
  {
    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[i];
    LDKRendererMeshResource* mesh = s_renderer_mesh_get_resource(renderer, submit->mesh);

    if (mesh == NULL || mesh->index_count == 0)
    {
      continue;
    }

    LDKRendererMeshObjectParams object_params = {0};
    object_params.world = submit->world;
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0, sizeof(object_params), &object_params);
    ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
    ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

    LDKRHIDrawIndexedDesc draw_desc = {0};
    draw_desc.index_count = mesh->index_count;
    draw_desc.first_index = 0;
    draw_desc.vertex_offset = 0;
    ldk_rhi_draw_indexed(pass->rhi, &draw_desc);
  }

  ldk_rhi_pass_end(pass->rhi);
  return true;
}

// ---------------------------------------------------------------------------
// Internal pass: UI / present
// ---------------------------------------------------------------------------

static bool s_renderer_ui_pass_create_shaders(LDKRendererUIPass* renderer)
{
  renderer->vertex_shader_module = ldk_rhi_create_builtin_shader_module(renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (renderer->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  renderer->fragment_shader_module = ldk_rhi_create_builtin_shader_module(renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (renderer->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(renderer->rhi, renderer->vertex_shader_module);
    renderer->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
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
  return renderer->bindings_layout != LDK_RHI_INVALID_RESOURCE;
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
  return renderer->pipeline != LDK_RHI_INVALID_RESOURCE;
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
  return renderer->white_texture != LDK_RHI_INVALID_RESOURCE;
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
  return renderer->sampler != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_buffer_creates(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (renderer->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = renderer->index_capacity * (u32)sizeof(u32);
  index_desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (renderer->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererUIParams);
  params_desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->params_buffer = ldk_rhi_buffer_create(renderer->rhi, &params_desc);
  return renderer->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_recreate_vertex_buffer(LDKRendererUIPass* renderer)
{
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
  renderer->vertex_buffer = LDK_RHI_INVALID_RESOURCE;

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->vertex_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_recreate_index_buffer(LDKRendererUIPass* renderer)
{
  ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
  renderer->index_buffer = LDK_RHI_INVALID_RESOURCE;

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &desc);
  return renderer->index_buffer != LDK_RHI_INVALID_RESOURCE;
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

  return LDK_RHI_INVALID_RESOURCE;
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
  if (cached_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    return cached_bindings;
  }

  if (renderer->bindings_cache_count == renderer->bindings_cache_capacity)
  {
    if (!s_renderer_ui_pass_grow_bindings_cache(renderer))
    {
      return LDK_RHI_INVALID_RESOURCE;
    }
  }

  LDKRHIBindings bindings = s_renderer_ui_pass_create_draw_bindings(renderer, texture);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
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
  pass_desc.color_attachments[0].texture = LDK_RHI_INVALID_RESOURCE;
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

  for (u32 i = 0; i < render_data->command_count; i++)
  {
    const LDKUIDrawCmd* cmd = &render_data->commands[i];
    LDKRHITexture texture = (LDKRHITexture)cmd->texture;
    if (texture == LDK_RHI_INVALID_RESOURCE)
    {
      texture = renderer->white_texture;
    }

    LDKRHIBindings bindings = s_renderer_ui_pass_get_draw_bindings(renderer, texture);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }

    LDKRHIRect scissor = {0};
    scissor.x = (i32)cmd->clip_rect.x;
    scissor.y = (i32)cmd->clip_rect.y;
    scissor.width = (i32)cmd->clip_rect.w;
    scissor.height = (i32)cmd->clip_rect.h;
    ldk_rhi_scissor_set(renderer->rhi, &scissor);


    if (scissor.width <= 0 || scissor.height <= 0)
    {
      continue;
    }

    ldk_rhi_bindings_bind(renderer->rhi, bindings);
    ldk_rhi_scissor_set(renderer->rhi, &scissor);

    LDKRHIDrawIndexedDesc draw_desc = {0};
    draw_desc.index_count = cmd->index_count;
    draw_desc.first_index = cmd->index_offset;
    draw_desc.vertex_offset = 0;
    ldk_rhi_draw_indexed(renderer->rhi, &draw_desc);
  }

  ldk_rhi_pass_end(renderer->rhi);
}

static void s_renderer_present_scene_pass(LDKRenderer* renderer, const LDKRendererFrameDesc* frame_desc)
{
  if (renderer == NULL || frame_desc == NULL)
  {
    return;
  }

  if (renderer->scene_target.color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  i32 width = frame_desc->framebuffer_width;
  i32 height = frame_desc->framebuffer_height;

  if (width <= 0 || height <= 0)
  {
    return;
  }

  LDKUIVertex vertices[4] =
  {
    {0.0f,         0.0f,          0.0f, 0.0f, 0xffffffffu},
    {(float)width, 0.0f,          1.0f, 0.0f, 0xffffffffu},
    {(float)width, (float)height, 1.0f, 1.0f, 0xffffffffu},
    {0.0f,         (float)height, 0.0f, 1.0f, 0xffffffffu},
  };

  u32 indices[6] =
  {
    0, 1, 2,
    0, 2, 3,
  };

  LDKUIDrawCmd command = {0};
  command.texture = (LDKUITextureHandle)renderer->scene_target.color_texture;
  command.clip_rect.x = 0.0f;
  command.clip_rect.y = 0.0f;
  command.clip_rect.w = (float)width;
  command.clip_rect.h = (float)height;
  command.index_offset = 0;
  command.index_count = 6;

  LDKUIRenderData render_data = {0};
  render_data.vertices = vertices;
  render_data.vertex_count = 4;
  render_data.indices = indices;
  render_data.index_count = 6;
  render_data.commands = &command;
  render_data.command_count = 1;

  LDKRendererFrameDesc present_desc = *frame_desc;
  present_desc.clear_color_enabled = true;

  s_renderer_ui_pass(&renderer->ui_pass, &render_data, &present_desc);
}


// ---------------------------------------------------------------------------
// Internal renderer resources: Mesh
// ---------------------------------------------------------------------------
// Internal renderer resources: Texture
// ---------------------------------------------------------------------------

static LDKRendererTextureResource* s_renderer_texture_get_resource(
    LDKRenderer* renderer,
    LDKResourceTexture texture)
{
  if (renderer == NULL || texture.id == LDK_RHI_INVALID_RESOURCE)
  {
    return NULL;
  }

  u32 index = (u32)(texture.id - 1u);
  if (index >= renderer->texture_count)
  {
    return NULL;
  }

  LDKRendererTextureResource* resource = &renderer->textures[index];
  if (!resource->alive)
  {
    return NULL;
  }

  return resource;
}

static bool s_renderer_grow_texture_cache(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->texture_capacity == 0 ? 64 : renderer->texture_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererTextureResource);
  LDKRendererTextureResource* new_textures = renderer->textures == NULL
    ? (LDKRendererTextureResource*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererTextureResource*)LDK_RENDERER_REALLOC(renderer->textures, new_size);

  if (new_textures == NULL)
  {
    return false;
  }

  memset(
      new_textures + renderer->texture_capacity,
      0,
      (size_t)(new_capacity - renderer->texture_capacity) * sizeof(LDKRendererTextureResource));

  renderer->textures = new_textures;
  renderer->texture_capacity = new_capacity;
  return true;
}

static LDKRHIFormat s_renderer_texture_format_from_desc(LDKRendererTextureDesc const* desc)
{
  if (desc == NULL)
  {
    return LDK_RHI_FORMAT_INVALID;
  }

  if (desc->channel_count == 1)
  {
    return LDK_RHI_FORMAT_R8_UNORM;
  }

  if (desc->channel_count == 2)
  {
    return LDK_RHI_FORMAT_RG8_UNORM;
  }

  if (desc->channel_count == 4)
  {
    if ((desc->flags & LDK_RENDERER_TEXTURE_FLAG_SRGB) != 0)
    {
      return LDK_RHI_FORMAT_RGBA8_SRGB;
    }

    return LDK_RHI_FORMAT_RGBA8_UNORM;
  }

  return LDK_RHI_FORMAT_INVALID;
}

static bool s_renderer_texture_desc_is_valid(LDKRendererTextureDesc const* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->width == 0 || desc->height == 0)
  {
    return false;
  }

  if (desc->pixels == NULL && desc->byte_count != 0)
  {
    return false;
  }

  if (desc->byte_count > UINT32_MAX)
  {
    return false;
  }

  return s_renderer_texture_format_from_desc(desc) != LDK_RHI_FORMAT_INVALID;
}

static bool s_renderer_texture_resource_create_rhi_texture(
    LDKRenderer* renderer,
    LDKRendererTextureResource* resource,
    LDKRendererTextureDesc const* desc)
{
  LDKRHIFormat format = s_renderer_texture_format_from_desc(desc);
  if (format == LDK_RHI_FORMAT_INVALID)
  {
    return false;
  }

  LDKRHITextureDesc rhi_desc = {0};
  ldk_rhi_texture_desc_defaults(&rhi_desc);
  rhi_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  rhi_desc.format = format;
  rhi_desc.width = desc->width;
  rhi_desc.height = desc->height;
  rhi_desc.depth = 1;
  rhi_desc.mip_count = 1;
  rhi_desc.layer_count = 1;
  rhi_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  rhi_desc.initial_data = desc->pixels;
  rhi_desc.initial_data_size = (u32)desc->byte_count;

  if ((desc->flags & LDK_RENDERER_TEXTURE_FLAG_RENDERABLE) != 0)
  {
    rhi_desc.usage |= LDK_RHI_TEXTURE_USAGE_RENDER_TARGET;
  }

  resource->texture = ldk_rhi_texture_create(renderer->rhi, &rhi_desc);
  if (resource->texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  resource->width = desc->width;
  resource->height = desc->height;
  resource->channel_count = desc->channel_count;
  resource->format = format;
  resource->flags = desc->flags;
  return true;
}

LDKResourceTexture ldk_renderer_texture_null(void)
{
  return LDK_RESOURCE_TEXTURE_INVALID;
}

bool ldk_renderer_texture_is_valid(LDKRenderer* renderer, LDKResourceTexture texture)
{
  return s_renderer_texture_get_resource(renderer, texture) != NULL;
}

LDKResourceTexture ldk_renderer_texture_create(LDKRenderer* renderer,
    LDKRendererTextureDesc const* desc)
{
  LDKResourceTexture invalid = ldk_renderer_texture_null();

  if (renderer == NULL || !renderer->is_initialized || !s_renderer_texture_desc_is_valid(desc))
  {
    return invalid;
  }

  if (renderer->texture_count == renderer->texture_capacity)
  {
    if (!s_renderer_grow_texture_cache(renderer))
    {
      return invalid;
    }
  }

  u32 index = renderer->texture_count;
  LDKRendererTextureResource* resource = &renderer->textures[index];
  memset(resource, 0, sizeof(*resource));

  if (!s_renderer_texture_resource_create_rhi_texture(renderer, resource, desc))
  {
    memset(resource, 0, sizeof(*resource));
    return invalid;
  }

  resource->alive = true;
  renderer->texture_count += 1;

  LDKResourceTexture texture = {0};
  texture.id = (LDKRHITexture)(index + 1u);
  return texture;
}

bool ldk_renderer_texture_update(LDKRenderer* renderer,
    LDKResourceTexture texture, void const* pixels, u64 byte_count)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return false;
  }

  if (pixels == NULL || byte_count == 0 || byte_count > UINT32_MAX)
  {
    return false;
  }

  LDKRendererTextureResource* resource = s_renderer_texture_get_resource(renderer, texture);
  if (resource == NULL)
  {
    return false;
  }

  return ldk_rhi_texture_update(renderer->rhi, resource->texture, 0, 0, pixels, (u32)byte_count);
}

LDKResourceTexture ldk_renderer_texture_create_from_image(LDKRenderer* renderer,
    LDKImage const* image, u32 flags)
{
  LDKImageInfo info = {0};

  if (!ldk_image_get_info(image, &info))
  {
    return ldk_renderer_texture_null();
  }

  LDKRendererTextureDesc desc = {0};
  desc.width = info.width;
  desc.height = info.height;
  desc.channel_count = info.channel_count;
  desc.flags = flags;
  desc.pixels = info.pixels;
  desc.byte_count = info.byte_count;

  return ldk_renderer_texture_create(renderer, &desc);
}

void ldk_renderer_texture_destroy(LDKRenderer* renderer, LDKResourceTexture texture)
{
  if (renderer == NULL || renderer->rhi == NULL)
  {
    return;
  }

  LDKRendererTextureResource* resource = s_renderer_texture_get_resource(renderer, texture);
  if (resource == NULL)
  {
    return;
  }

  ldk_rhi_texture_destroy(renderer->rhi, resource->texture);
  memset(resource, 0, sizeof(*resource));
}

static void s_renderer_destroy_texture_resources(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->texture_count; i++)
    {
      LDKResourceTexture texture = {0};
      texture.id = (LDKRHITexture)(i + 1u);
      ldk_renderer_texture_destroy(renderer, texture);
    }
  }

  LDK_RENDERER_FREE(renderer->textures);
  renderer->textures = NULL;
  renderer->texture_count = 0;
  renderer->texture_capacity = 0;
}

// ---------------------------------------------------------------------------
// Font cache
// ---------------------------------------------------------------------------

static void s_renderer_destroy_font_page_cache(LDKRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->font_page_count; i++)
    {
      ldk_rhi_texture_destroy(renderer->rhi, renderer->font_pages[i].texture);
    }
  }

  LDK_RENDERER_FREE(renderer->font_pages);
  renderer->font_pages = NULL;
  renderer->font_page_count = 0;
  renderer->font_page_capacity = 0;
}

static bool s_renderer_grow_font_page_cache(LDKRenderer* renderer)
{
  u32 new_capacity = renderer->font_page_capacity == 0 ? 32 : renderer->font_page_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKRendererFontPageCacheEntry);
  LDKRendererFontPageCacheEntry* new_pages = renderer->font_pages == NULL
    ? (LDKRendererFontPageCacheEntry*)LDK_RENDERER_ALLOC(new_size)
    : (LDKRendererFontPageCacheEntry*)LDK_RENDERER_REALLOC(renderer->font_pages, new_size);

  if (new_pages == NULL)
  {
    return false;
  }

  renderer->font_pages = new_pages;
  renderer->font_page_capacity = new_capacity;
  return true;
}

LDKUITextureHandle ldk_renderer_get_font_page_texture(LDKRenderer* renderer, LDKFontInstance* font, u32 page_index)
{
  if (renderer == NULL || renderer->rhi == NULL || font == NULL)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  for (u32 i = 0; i < renderer->font_page_count; i++)
  {
    LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[i];
    if (entry->font == font && entry->page_index == page_index)
    {
      return (LDKUITextureHandle)entry->texture;
    }
  }

  LDKFontPageInfo page = {0};
  if (!ldk_ttf_get_page_info(font, page_index, &page))
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  if (page.pixels == NULL || page.width == 0 || page.height == 0)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHITextureDesc texture_desc = {0};
  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_R8_UNORM;
  texture_desc.width = page.width;
  texture_desc.height = page.height;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = page.pixels;
  texture_desc.initial_data_size = page.width * page.height;
  texture_desc.swizzle_r = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_g = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_b = LDK_RHI_TEXTURE_SWIZZLE_ONE;
  texture_desc.swizzle_a = LDK_RHI_TEXTURE_SWIZZLE_R;

  LDKRHITexture texture = ldk_rhi_texture_create(renderer->rhi, &texture_desc);
  if (texture == LDK_RHI_INVALID_RESOURCE)
  {
    return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
  }

  if (renderer->font_page_count == renderer->font_page_capacity)
  {
    if (!s_renderer_grow_font_page_cache(renderer))
    {
      ldk_rhi_texture_destroy(renderer->rhi, texture);
      return (LDKUITextureHandle)LDK_RHI_INVALID_RESOURCE;
    }
  }

  LDKRendererFontPageCacheEntry* entry = &renderer->font_pages[renderer->font_page_count];
  entry->font = font;
  entry->page_index = page_index;
  entry->width = page.width;
  entry->height = page.height;
  entry->texture = texture;
  renderer->font_page_count += 1;

  ldk_ttf_clear_page_dirty(font, page_index);
  return (LDKUITextureHandle)texture;
}

LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index)
{
  return ldk_renderer_get_font_page_texture((LDKRenderer*)user, font, page_index);
}

// ---------------------------------------------------------------------------
// Public renderer API
// ---------------------------------------------------------------------------

bool ldk_renderer_initialize(LDKRenderer* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;

  if (!s_renderer_mesh_pass_initialize(&renderer->mesh_pass, config))
  {
    ldk_renderer_terminate(renderer);
    return false;
  }

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

  s_renderer_target_destroy(renderer, &renderer->scene_target);
  s_renderer_destroy_font_page_cache(renderer);
  s_renderer_destroy_texture_resources(renderer);
  s_renderer_destroy_mesh_resources(renderer);
  s_renderer_ui_pass_terminate(&renderer->ui_pass);
  s_renderer_mesh_pass_terminate(&renderer->mesh_pass);
  memset(renderer, 0, sizeof(*renderer));
}

void ldk_renderer_render_frame(LDKRenderer* renderer, LDKRendererFrameDesc const* desc)
{
  if (renderer == NULL || !renderer->is_initialized || desc == NULL || renderer->rhi == NULL)
  {
    return;
  }

  ldk_rhi_frame_begin(renderer->rhi);

  bool rendered_scene = s_renderer_mesh_pass(renderer, &renderer->mesh_pass, desc);
  if (rendered_scene)
  {
    s_renderer_present_scene_pass(renderer, desc);
  }


  if (renderer->submitted_ui != NULL)
  {
    LDKRendererFrameDesc ui_desc = *desc;
    if (rendered_scene)
    {
      ui_desc.clear_color_enabled = false;
    }

    s_renderer_ui_pass(&renderer->ui_pass, renderer->submitted_ui, &ui_desc);
  }


  ldk_rhi_frame_end(renderer->rhi);

  renderer->submitted_mesh_count = 0;
  renderer->submitted_ui = NULL;
  renderer->has_camera = false;
}

bool ldk_renderer_submit_view(LDKRenderer* renderer, Mat4 view, Mat4 projection)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return false;
  }

  renderer->camera_view = view;
  renderer->camera_projection = projection;
  renderer->has_camera = true;
  return true;
}

void ldk_renderer_submit_ui(LDKRenderer* renderer, LDKUIRenderData const* render_data)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return;
  }

  renderer->submitted_ui = render_data;
}

bool ldk_renderer_submit_mesh(LDKRenderer* renderer, LDKResourceMesh mesh, Mat4 world)
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
