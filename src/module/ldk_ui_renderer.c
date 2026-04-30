#include <ldk_common.h>
#include "ldk_ui_renderer.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKUIRendererParams
{
  float viewport_size[2];
  float padding[2];
} LDKUIRendererParams;

static char const* LDK_UI_RENDERER_VERTEX_SHADER =
  "#version 330 core\n"
  "layout(location = 0) in vec2 a_position;\n"
  "layout(location = 1) in vec2 a_uv;\n"
  "layout(location = 2) in vec4 a_color;\n"
  "layout(std140) uniform LDK_UBO_0\n"
  "{\n"
  "  vec2 u_viewport_size;\n"
  "};\n"
  "out vec2 v_uv;\n"
  "out vec4 v_color;\n"
  "void main()\n"
  "{\n"
  "  vec2 ndc = vec2(\n"
  "    (a_position.x / u_viewport_size.x) * 2.0 - 1.0,\n"
  "    1.0 - (a_position.y / u_viewport_size.y) * 2.0\n"
  "  );\n"
  "  v_uv = a_uv;\n"
  "  v_color = a_color;\n"
  "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
  "}\n";

static char const* LDK_UI_RENDERER_FRAGMENT_SHADER =
  "#version 330 core\n"
  "in vec2 v_uv;\n"
  "in vec4 v_color;\n"
  "out vec4 out_color;\n"
  "uniform sampler2D LDK_TEXTURE_1;\n"
  "void main()\n"
  "{\n"
  "  vec4 tex = texture(LDK_TEXTURE_1, v_uv);\n"
  "  out_color = tex * v_color;\n"
  "}\n";

static u32 ldk_ui_renderer_cstr_size(char const* cstr)
{
  return (u32)strlen(cstr);
}

static bool ldk_ui_renderer_create_shaders(LDKUIRenderer* renderer)
{
  LDKRHIShaderModuleDesc vertex_desc = {0};
  ldk_rhi_shader_module_desc_defaults(&vertex_desc);
  vertex_desc.stage = LDK_RHI_SHADER_STAGE_VERTEX;
  vertex_desc.code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
  vertex_desc.code = LDK_UI_RENDERER_VERTEX_SHADER;
  vertex_desc.code_size = ldk_ui_renderer_cstr_size(LDK_UI_RENDERER_VERTEX_SHADER);

  renderer->vertex_shader_module = ldk_rhi_create_shader_module(renderer->rhi, &vertex_desc);
  if (renderer->vertex_shader_module == LDK_RHI_INVALID_SHADER_MODULE)
  {
    return false;
  }

  LDKRHIShaderModuleDesc fragment_desc = {0};
  ldk_rhi_shader_module_desc_defaults(&fragment_desc);
  fragment_desc.stage = LDK_RHI_SHADER_STAGE_FRAGMENT;
  fragment_desc.code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
  fragment_desc.code = LDK_UI_RENDERER_FRAGMENT_SHADER;
  fragment_desc.code_size = ldk_ui_renderer_cstr_size(LDK_UI_RENDERER_FRAGMENT_SHADER);

  renderer->fragment_shader_module = ldk_rhi_create_shader_module(renderer->rhi, &fragment_desc);
  if (renderer->fragment_shader_module == LDK_RHI_INVALID_SHADER_MODULE)
  {
    return false;
  }

  return true;
}

static bool ldk_ui_renderer_create_bindings_layout(LDKUIRenderer* renderer)
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

  renderer->bindings_layout = ldk_rhi_create_bindings_layout(renderer->rhi, &desc);
  return renderer->bindings_layout != LDK_RHI_INVALID_BINDINGS_LAYOUT;
}

static bool ldk_ui_renderer_create_pipeline(LDKUIRenderer* renderer)
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

  renderer->pipeline = ldk_rhi_create_pipeline(renderer->rhi, &desc);
  return renderer->pipeline != LDK_RHI_INVALID_PIPELINE;
}

static bool ldk_ui_renderer_create_white_texture(LDKUIRenderer* renderer)
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

  renderer->white_texture = ldk_rhi_create_texture(renderer->rhi, &texture_desc);
  return renderer->white_texture != LDK_RHI_INVALID_TEXTURE;
}

static bool ldk_ui_renderer_create_sampler(LDKUIRenderer* renderer)
{
  LDKRHISamplerDesc desc = {0};
  ldk_rhi_sampler_desc_defaults(&desc);
  desc.min_filter = LDK_RHI_FILTER_NEAREST;
  desc.mag_filter = LDK_RHI_FILTER_NEAREST;
  desc.mip_filter = LDK_RHI_FILTER_NEAREST;
  desc.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;

  renderer->sampler = ldk_rhi_create_sampler(renderer->rhi, &desc);
  return renderer->sampler != LDK_RHI_INVALID_SAMPLER;
}

static bool ldk_ui_renderer_create_buffers(LDKUIRenderer* renderer)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->vertex_buffer = ldk_rhi_create_buffer(renderer->rhi, &vertex_desc);
  if (renderer->vertex_buffer == LDK_RHI_INVALID_BUFFER)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = renderer->index_capacity * (u32)sizeof(u32);
  index_desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->index_buffer = ldk_rhi_create_buffer(renderer->rhi, &index_desc);
  if (renderer->index_buffer == LDK_RHI_INVALID_BUFFER)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKUIRendererParams);
  params_desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->params_buffer = ldk_rhi_create_buffer(renderer->rhi, &params_desc);
  return renderer->params_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool ldk_ui_renderer_recreate_vertex_buffer(LDKUIRenderer* renderer)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_destroy_buffer(renderer->rhi, renderer->vertex_buffer);
  renderer->vertex_buffer = LDK_RHI_INVALID_BUFFER;
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->vertex_buffer = ldk_rhi_create_buffer(renderer->rhi, &desc);
  return renderer->vertex_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool ldk_ui_renderer_recreate_index_buffer(LDKUIRenderer* renderer)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_destroy_buffer(renderer->rhi, renderer->index_buffer);
  renderer->index_buffer = LDK_RHI_INVALID_BUFFER;
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = renderer->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  renderer->index_buffer = ldk_rhi_create_buffer(renderer->rhi, &desc);
  return renderer->index_buffer != LDK_RHI_INVALID_BUFFER;
}

static bool ldk_ui_renderer_ensure_vertex_capacity(LDKUIRenderer* renderer, u32 vertex_count)
{
  if (vertex_count <= renderer->vertex_capacity)
  {
    return true;
  }

  while (renderer->vertex_capacity < vertex_count)
  {
    renderer->vertex_capacity *= 2;
  }

  return ldk_ui_renderer_recreate_vertex_buffer(renderer);
}

static bool ldk_ui_renderer_ensure_index_capacity(LDKUIRenderer* renderer, u32 index_count)
{
  if (index_count <= renderer->index_capacity)
  {
    return true;
  }

  while (renderer->index_capacity < index_count)
  {
    renderer->index_capacity *= 2;
  }

  return ldk_ui_renderer_recreate_index_buffer(renderer);
}

bool ldk_ui_renderer_initialize(LDKUIRenderer* renderer, LDKUIRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;
  renderer->vertex_capacity = config->initial_vertex_capacity > 0 ? config->initial_vertex_capacity : 1024;
  renderer->index_capacity = config->initial_index_capacity > 0 ? config->initial_index_capacity : 2048;

  if (!ldk_ui_renderer_create_shaders(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_bindings_layout(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_pipeline(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_white_texture(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_sampler(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_buffers(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;
  return true;
}

void ldk_ui_renderer_terminate(LDKUIRenderer* renderer)
{
  if (renderer == NULL)
  {
    return;
  }

  if (renderer->rhi != NULL)
  {
    for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
    {
      ldk_rhi_destroy_bindings(renderer->rhi, renderer->bindings_cache[i].bindings);
    }

    ldk_rhi_destroy_buffer(renderer->rhi, renderer->params_buffer);
    ldk_rhi_destroy_buffer(renderer->rhi, renderer->index_buffer);
    ldk_rhi_destroy_buffer(renderer->rhi, renderer->vertex_buffer);
    ldk_rhi_destroy_sampler(renderer->rhi, renderer->sampler);
    ldk_rhi_destroy_texture(renderer->rhi, renderer->white_texture);
    ldk_rhi_destroy_pipeline(renderer->rhi, renderer->pipeline);
    ldk_rhi_destroy_bindings_layout(renderer->rhi, renderer->bindings_layout);
    ldk_rhi_destroy_shader_module(renderer->rhi, renderer->fragment_shader_module);
    ldk_rhi_destroy_shader_module(renderer->rhi, renderer->vertex_shader_module);
  }

  free(renderer->bindings_cache);
  memset(renderer, 0, sizeof(*renderer));
}

static LDKRHIBindings ldk_ui_renderer_create_draw_bindings(LDKUIRenderer* renderer, LDKRHITexture texture)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = renderer->bindings_layout;
  desc.binding_count = 2;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = renderer->params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKUIRendererParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].texture = texture;
  desc.bindings[1].sampler = renderer->sampler;
  return ldk_rhi_create_bindings(renderer->rhi, &desc);
}


static LDKRHIBindings ldk_ui_renderer_find_cached_bindings(LDKUIRenderer* renderer, LDKRHITexture texture)
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

static bool ldk_ui_renderer_grow_bindings_cache(LDKUIRenderer* renderer)
{
  u32 new_capacity = renderer->bindings_cache_capacity == 0 ? 16 : renderer->bindings_cache_capacity * 2;
  size_t new_size = (size_t)new_capacity * sizeof(LDKUIRendererBindingsCacheEntry);
  LDKUIRendererBindingsCacheEntry* new_cache = (LDKUIRendererBindingsCacheEntry*)realloc(renderer->bindings_cache, new_size);
  if (new_cache == NULL)
  {
    return false;
  }

  renderer->bindings_cache = new_cache;
  renderer->bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings ldk_ui_renderer_get_draw_bindings(LDKUIRenderer* renderer, LDKRHITexture texture)
{
  LDKRHIBindings cached_bindings = ldk_ui_renderer_find_cached_bindings(renderer, texture);
  if (cached_bindings != LDK_RHI_INVALID_BINDINGS)
  {
    return cached_bindings;
  }

  if (renderer->bindings_cache_count == renderer->bindings_cache_capacity)
  {
    if (!ldk_ui_renderer_grow_bindings_cache(renderer))
    {
      return LDK_RHI_INVALID_BINDINGS;
    }
  }

  LDKRHIBindings bindings = ldk_ui_renderer_create_draw_bindings(renderer, texture);
  if (bindings == LDK_RHI_INVALID_BINDINGS)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  LDKUIRendererBindingsCacheEntry* entry = &renderer->bindings_cache[renderer->bindings_cache_count];
  entry->texture = texture;
  entry->bindings = bindings;
  renderer->bindings_cache_count += 1;
  return bindings;
}
void ldk_ui_renderer_draw(
  LDKUIRenderer* renderer,
  LDKUIRenderData const* render_data,
  i32 framebuffer_width,
  i32 framebuffer_height
)
{
  if (renderer == NULL || !renderer->is_initialized)
  {
    return;
  }

  if (render_data == NULL)
  {
    return;
  }

  if (framebuffer_width <= 0 || framebuffer_height <= 0)
  {
    return;
  }

  if (render_data->vertex_count == 0 || render_data->index_count == 0 || render_data->command_count == 0)
  {
    return;
  }

  if (!ldk_ui_renderer_ensure_vertex_capacity(renderer, render_data->vertex_count))
  {
    return;
  }

  if (!ldk_ui_renderer_ensure_index_capacity(renderer, render_data->index_count))
  {
    return;
  }

  LDKUIRendererParams params = {0};
  params.viewport_size[0] = (float)framebuffer_width;
  params.viewport_size[1] = (float)framebuffer_height;
  params.padding[0] = 0.0f;
  params.padding[1] = 0.0f;

  ldk_rhi_update_buffer(renderer->rhi, renderer->params_buffer, 0, sizeof(params), &params);
  ldk_rhi_update_buffer(renderer->rhi, renderer->vertex_buffer, 0, render_data->vertex_count * (u32)sizeof(LDKUIVertex), render_data->vertices);
  ldk_rhi_update_buffer(renderer->rhi, renderer->index_buffer, 0, render_data->index_count * (u32)sizeof(u32), render_data->indices);

  ldk_rhi_bind_pipeline(renderer->rhi, renderer->pipeline);
  ldk_rhi_bind_vertex_buffer(renderer->rhi, renderer->vertex_buffer, 0);
  ldk_rhi_bind_index_buffer(renderer->rhi, renderer->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

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

    LDKRHIBindings bindings = ldk_ui_renderer_get_draw_bindings(renderer, texture);
    if (bindings == LDK_RHI_INVALID_BINDINGS)
    {
      continue;
    }

    LDKRHIRect scissor = {0};
    scissor.x = scissor_x;
    scissor.y = scissor_y;
    scissor.width = scissor_w;
    scissor.height = scissor_h;
    ldk_rhi_set_scissor(renderer->rhi, &scissor);
    ldk_rhi_bind_bindings(renderer->rhi, bindings);

    LDKRHIDrawIndexedDesc draw_desc = {0};
    draw_desc.index_count = cmd->index_count;
    draw_desc.first_index = cmd->index_offset;
    draw_desc.vertex_offset = 0;
    ldk_rhi_draw_indexed(renderer->rhi, &draw_desc);
  }
}
