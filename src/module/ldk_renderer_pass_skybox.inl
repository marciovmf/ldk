// ---------------------------------------------------------------------------
// Internal pass: Skybox
// ---------------------------------------------------------------------------

#define LDK_RENDERER_SKYBOX_VERTEX_COUNT 36u

typedef struct LDKRendererSkyboxVertex
{
  float x;
  float y;
  float z;
} LDKRendererSkyboxVertex;

typedef struct LDKRendererSkyboxParams
{
  Mat4 view;
  Mat4 projection;
  float fog_color[4];
  float fog_params[4];
} LDKRendererSkyboxParams;

static bool s_renderer_skybox_pass_create_shaders(LDKRendererSkyboxPass *pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_SKYBOX_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_SKYBOX_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_skybox_pass_create_bindings_layout(
    LDKRendererSkyboxPass *pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 2;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;

  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static LDKRHIPipeline s_renderer_skybox_pass_create_pipeline(
    LDKRendererSkyboxPass *pass, LDKRHIFormat color_format)
{
  LDKRHIPipelineDesc desc = {0};
  ldk_rhi_pipeline_desc_defaults(&desc);
  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;
  desc.bindings_layout = pass->bindings_layout;
  desc.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.raster_state.scissor_enabled = false;
  desc.vertex_layout.stride = sizeof(LDKRendererSkyboxVertex);
  desc.vertex_layout.attribute_count = 1;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset = 0;
  desc.color_attachment_count = 1;
  desc.color_formats[0] = color_format;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;
  return ldk_rhi_pipeline_create(pass->rhi, &desc);
}

static bool s_renderer_skybox_pass_create_buffers(LDKRendererSkyboxPass *pass)
{
  static const LDKRendererSkyboxVertex
      vertices[LDK_RENDERER_SKYBOX_VERTEX_COUNT] = {
      {-1.0f, 1.0f, -1.0f}, {-1.0f, -1.0f, -1.0f},
      {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
      {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f},
      {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
      {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, 1.0f},
      {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
      {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},
      {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f},
      {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, -1.0f},
      {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
      {-1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f},
      {1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f},
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
  params_desc.size = sizeof(LDKRendererSkyboxParams);
  params_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->params_buffer = ldk_rhi_buffer_create(pass->rhi, &params_desc);
  return pass->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_skybox_pass_initialize(
    LDKRendererSkyboxPass *pass, const LDKRendererConfig *config)
{
  if (!pass || !config || !config->rhi)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;
  if (!s_renderer_skybox_pass_create_shaders(pass) ||
      !s_renderer_skybox_pass_create_bindings_layout(pass))
  {
    s_renderer_skybox_pass_terminate(pass);
    return false;
  }

  pass->ldr_pipeline = s_renderer_skybox_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA8_UNORM);
  pass->hdr_pipeline = s_renderer_skybox_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA16_FLOAT);
  if (pass->ldr_pipeline == LDK_RHI_INVALID_RESOURCE ||
      pass->hdr_pipeline == LDK_RHI_INVALID_RESOURCE ||
      !s_renderer_skybox_pass_create_buffers(pass))
  {
    s_renderer_skybox_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_skybox_pass_terminate(LDKRendererSkyboxPass *pass)
{
  if (!pass)
  {
    return;
  }

  if (pass->rhi)
  {
    ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->hdr_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->ldr_pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  memset(pass, 0, sizeof(*pass));
}

static void s_renderer_skybox_pass_draw(LDKRenderer *renderer,
    LDKRendererSkyboxPass *pass, const LDKRendererView *view)
{
  if (!renderer || !pass || !pass->is_initialized || !view ||
      !view->submitted)
  {
    return;
  }

  LDKRendererSkyboxResource *resource =
      s_renderer_skybox_get_resource(renderer, view->skybox);
  if (!resource)
  {
    return;
  }

  LDKRendererSkyboxParams params = {0};
  params.view = view->view;
  params.projection = view->projection;
  LDKRHIColor fog_color = ldk_renderer_color_from_rgba32(renderer->fog.color);
  params.fog_color[0] = fog_color.r;
  params.fog_color[1] = fog_color.g;
  params.fog_color[2] = fog_color.b;
  params.fog_color[3] = 1.0f;
  params.fog_params[0] = renderer->fog.start;
  params.fog_params[1] = renderer->fog.enabled ? renderer->fog.density : 0.0f;
  if (!ldk_rhi_buffer_update(pass->rhi, pass->params_buffer, 0,
          sizeof(params), &params))
  {
    return;
  }

  LDKRHIPipeline pipeline =
      view->target.color_format == LDK_RHI_FORMAT_RGBA16_FLOAT
      ? pass->hdr_pipeline : pass->ldr_pipeline;
  ldk_rhi_pipeline_bind(pass->rhi, pipeline);
  ldk_rhi_bindings_bind(pass->rhi, resource->bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, pass->vertex_buffer, 0);

  LDKRHIDrawDesc draw = {0};
  draw.vertex_count = LDK_RENDERER_SKYBOX_VERTEX_COUNT;
  ldk_rhi_draw(pass->rhi, &draw);

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  stats->draw_call_count += 1;
  stats->skybox_draw_call_count += 1;
}

