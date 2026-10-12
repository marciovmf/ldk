// ---------------------------------------------------------------------------
// Internal pass: Procedural grid
// ---------------------------------------------------------------------------

#define LDK_RENDERER_GRID_VERTEX_COUNT 6u

typedef struct LDKRendererGridVertex
{
  float x;
  float y;
  float z;
} LDKRendererGridVertex;

typedef struct LDKRendererGridParams
{
  Mat4 view;
  Mat4 projection;
  float center_extent[4];
  float settings[4];
} LDKRendererGridParams;

static bool s_renderer_grid_pass_create_shaders(LDKRendererGridPass* pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_GRID_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_GRID_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_grid_pass_create_bindings_layout(
    LDKRendererGridPass* pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 1;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;

  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_pipeline(LDKRendererGridPass* pass)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);
  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;
  desc.bindings_layout = pass->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = false;
  desc.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.raster_state.scissor_enabled = false;
  desc.vertex_layout.stride = sizeof(LDKRendererGridVertex);
  desc.vertex_layout.attribute_count = 1;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset = 0;
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  pass->pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  return pass->pipeline != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_buffers(LDKRendererGridPass* pass)
{
  static const LDKRendererGridVertex vertices[LDK_RENDERER_GRID_VERTEX_COUNT] = {
      {-1.0f, 0.0f, -1.0f},
      {-1.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, 1.0f},
      {-1.0f, 0.0f, -1.0f},
      {1.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, -1.0f},
  };
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = sizeof(vertices);
  vertex_desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_desc.initial_data = vertices;

  pass->vertex_buffer = ldk_rhi_buffer_create(pass->rhi, &vertex_desc);
  if (pass->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererGridParams);
  params_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->params_buffer = ldk_rhi_buffer_create(pass->rhi, &params_desc);
  return pass->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_create_bindings(LDKRendererGridPass* pass)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 1;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererGridParams);

  pass->bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  return pass->bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_grid_pass_initialize(
    LDKRendererGridPass* pass, LDKRendererConfig const* config)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  if (!s_renderer_grid_pass_create_shaders(pass) ||
      !s_renderer_grid_pass_create_bindings_layout(pass) ||
      !s_renderer_grid_pass_create_pipeline(pass) ||
      !s_renderer_grid_pass_create_buffers(pass) ||
      !s_renderer_grid_pass_create_bindings(pass))
  {
    s_renderer_grid_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_grid_pass_terminate(LDKRendererGridPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  memset(pass, 0, sizeof(*pass));
}

static void s_renderer_grid_pass_draw(LDKRenderer* renderer,
    LDKRendererGridPass* pass, LDKRendererView const* view)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if (pass == NULL || !pass->is_initialized || view == NULL ||
      !view->submitted || !view->grid_submitted)
  {
    return;
  }

  LDKRendererGridParams params = {0};
  params.view = view->view;
  params.projection = view->projection;
  params.center_extent[0] = view->grid_center.x;
  params.center_extent[1] = view->grid_center.y;
  params.center_extent[2] = view->grid_center.z;
  params.center_extent[3] = view->grid_extent;
  params.settings[0] = view->grid_spacing;
  ldk_rhi_buffer_update(pass->rhi, pass->params_buffer, 0,
      sizeof(params), &params);

  ldk_rhi_pipeline_bind(pass->rhi, pass->pipeline);
  ldk_rhi_bindings_bind(pass->rhi, pass->bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, pass->vertex_buffer, 0);

  LDKRHIDrawDesc draw_desc = {0};
  draw_desc.vertex_count = LDK_RENDERER_GRID_VERTEX_COUNT;
  draw_desc.first_vertex = 0;
  ldk_rhi_draw(pass->rhi, &draw_desc);
  stats->draw_call_count += 1;
  stats->grid_draw_call_count += 1;
}

