// ---------------------------------------------------------------------------
// Internal pass: post processing
// ---------------------------------------------------------------------------

typedef struct LDKRendererPostProcessVertex
{
  float x;
  float y;
  float u;
  float v;
} LDKRendererPostProcessVertex;

typedef struct LDKRendererPostProcessParams
{
  float tonemapping[4];
  float color_adjustments[4];
  float blur_focus[4];
  float blur_options[4];
  float vignette[4];
  float lens[4];
  float heat[4];
  float stylization_options[4];
  float retro[4];
  float drunk_primary[4];
  float drunk_secondary[4];
  float animation[4];
} LDKRendererPostProcessParams;

typedef struct LDKRendererBlurParams
{
  float direction[4];
} LDKRendererBlurParams;

static float s_renderer_saturate(float value)
{
  if (!isfinite(value))
  {
    return 0.0f;
  }
  if (value < 0.0f)
  {
    return 0.0f;
  }
  if (value > 1.0f)
  {
    return 1.0f;
  }
  return value;
}

static float s_renderer_clamp_finite(
    float value, float min_value, float max_value, float fallback)
{
  if (!isfinite(value))
  {
    return fallback;
  }
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return value;
}

static bool s_renderer_blur_active(
    LDKRendererPostProcessing const* post_processing)
{
  return post_processing != NULL && post_processing->enabled &&
      post_processing->blur_enabled &&
      isfinite(post_processing->blur_strength) &&
      post_processing->blur_strength > 0.0f;
}

static void s_renderer_post_process_bindings_destroy(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL)
  {
    return;
  }

  if (view->post_process_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_bindings_destroy(renderer->rhi, view->post_process_bindings);
  }

  view->post_process_bindings = LDK_RHI_INVALID_RESOURCE;
  view->post_process_input_texture = LDK_RHI_INVALID_RESOURCE;
  view->post_process_blurred_texture = LDK_RHI_INVALID_RESOURCE;
}

static void s_renderer_post_process_blur_bindings_destroy(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL)
  {
    return;
  }

  for (u32 i = 0; i < 2; i++)
  {
    if (view->post_process_blur_bindings[i] != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_bindings_destroy(
          renderer->rhi, view->post_process_blur_bindings[i]);
    }
    view->post_process_blur_bindings[i] = LDK_RHI_INVALID_RESOURCE;
    view->post_process_blur_input_textures[i] = LDK_RHI_INVALID_RESOURCE;
  }
}

static void s_renderer_post_process_blur_textures_destroy(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL)
  {
    return;
  }

  s_renderer_post_process_bindings_destroy(renderer, view);
  s_renderer_post_process_blur_bindings_destroy(renderer, view);

  for (u32 i = 0; i < 2; i++)
  {
    if (view->post_process_blur_textures[i] != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_texture_destroy(
          renderer->rhi, view->post_process_blur_textures[i]);
    }
    view->post_process_blur_textures[i] = LDK_RHI_INVALID_RESOURCE;
  }
  view->post_process_blur_format = LDK_RHI_FORMAT_INVALID;
}

static void s_renderer_post_process_view_destroy(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL)
  {
    return;
  }

  s_renderer_post_process_blur_textures_destroy(renderer, view);

  if (view->post_process_texture != LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_ui_pass_remove_texture_bindings(
        &renderer->ui_pass, view->post_process_texture);
    ldk_rhi_texture_destroy(renderer->rhi, view->post_process_texture);
  }

  view->post_process_texture = LDK_RHI_INVALID_RESOURCE;

  if (view->overlay_display_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_bindings_destroy(renderer->rhi, view->overlay_display_bindings);
  }
  view->overlay_display_bindings = LDK_RHI_INVALID_RESOURCE;

  if (view->overlay_display_texture != LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_ui_pass_remove_texture_bindings(
        &renderer->ui_pass, view->overlay_display_texture);
    ldk_rhi_texture_destroy(renderer->rhi, view->overlay_display_texture);
  }
  view->overlay_display_texture = LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_overlay_display_ensure(
    LDKRenderer *renderer, LDKRendererView *view)
{
  if (!renderer || !renderer->rhi || !view ||
      view->overlay_target.color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  if (view->overlay_display_texture == LDK_RHI_INVALID_RESOURCE)
  {
    LDKRHITextureDesc texture_desc = {0};
    ldk_rhi_texture_desc_defaults(&texture_desc);
    texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
    texture_desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
    texture_desc.width = view->width;
    texture_desc.height = view->height;
    texture_desc.depth = 1;
    texture_desc.mip_count = 1;
    texture_desc.layer_count = 1;
    texture_desc.usage = LDK_RHI_TEXTURE_USAGE_RENDER_TARGET |
        LDK_RHI_TEXTURE_USAGE_SAMPLED;
    view->overlay_display_texture =
        ldk_rhi_texture_create(renderer->rhi, &texture_desc);
    if (view->overlay_display_texture == LDK_RHI_INVALID_RESOURCE)
    {
      return false;
    }
  }

  if (view->overlay_display_bindings == LDK_RHI_INVALID_RESOURCE)
  {
    LDKRHIBindingsDesc bindings_desc = {0};
    ldk_rhi_bindings_desc_defaults(&bindings_desc);
    bindings_desc.layout = renderer->post_process_pass.bindings_layout;
    bindings_desc.binding_count = 3;
    bindings_desc.bindings[0].slot = 0;
    bindings_desc.bindings[0].buffer = renderer->post_process_pass.params_buffer;
    bindings_desc.bindings[0].buffer_size =
        sizeof(LDKRendererPostProcessParams);
    bindings_desc.bindings[1].slot = 1;
    bindings_desc.bindings[1].texture = view->overlay_target.color_texture;
    bindings_desc.bindings[1].sampler = renderer->post_process_pass.sampler;
    bindings_desc.bindings[2].slot = 2;
    bindings_desc.bindings[2].texture = view->overlay_target.color_texture;
    bindings_desc.bindings[2].sampler = renderer->post_process_pass.sampler;
    view->overlay_display_bindings =
        ldk_rhi_bindings_create(renderer->rhi, &bindings_desc);
  }
  return view->overlay_display_bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_overlay_display_draw(
    LDKRenderer *renderer, LDKRendererView *view)
{
  if (!s_renderer_overlay_display_ensure(renderer, view))
  {
    return false;
  }

  LDKRendererPostProcessParams params = {0};
  params.blur_options[2] = (float)view->width / (float)view->height;
  if (!ldk_rhi_buffer_update(renderer->rhi,
          renderer->post_process_pass.params_buffer, 0, sizeof(params),
          &params))
  {
    return false;
  }

  LDKRHIPassDesc pass_desc = {0};
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = view->overlay_display_texture;
  pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.has_viewport = true;
  pass_desc.viewport.width = (float)view->width;
  pass_desc.viewport.height = (float)view->height;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  ldk_rhi_pipeline_bind(renderer->rhi, renderer->post_process_pass.pipeline);
  ldk_rhi_bindings_bind(renderer->rhi, view->overlay_display_bindings);
  ldk_rhi_vertex_buffer_bind(
      renderer->rhi, renderer->post_process_pass.vertex_buffer, 0);
  LDKRHIDrawDesc draw_desc = {0};
  draw_desc.vertex_count = 6;
  ldk_rhi_draw(renderer->rhi, &draw_desc);
  ldk_rhi_pass_end(renderer->rhi);

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  if (stats)
  {
    stats->draw_call_count += 1;
  }
  return true;
}

static bool s_renderer_post_process_texture_ensure(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL ||
      view->width == 0 || view->height == 0)
  {
    return false;
  }

  if (view->post_process_texture != LDK_RHI_INVALID_RESOURCE)
  {
    return true;
  }

  LDKRHITextureDesc desc = {0};
  ldk_rhi_texture_desc_defaults(&desc);
  desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.width = view->width;
  desc.height = view->height;
  desc.depth = 1;
  desc.mip_count = 1;
  desc.layer_count = 1;
  desc.usage =
      LDK_RHI_TEXTURE_USAGE_RENDER_TARGET | LDK_RHI_TEXTURE_USAGE_SAMPLED;

  view->post_process_texture = ldk_rhi_texture_create(renderer->rhi, &desc);
  return view->post_process_texture != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_post_process_blur_textures_ensure(
    LDKRenderer* renderer, LDKRendererView* view, LDKRHIFormat format)
{
  if (renderer == NULL || renderer->rhi == NULL || view == NULL ||
      view->width == 0 || view->height == 0 ||
      (format != LDK_RHI_FORMAT_RGBA8_UNORM &&
          format != LDK_RHI_FORMAT_RGBA16_FLOAT))
  {
    return false;
  }

  if (view->post_process_blur_textures[0] != LDK_RHI_INVALID_RESOURCE &&
      view->post_process_blur_textures[1] != LDK_RHI_INVALID_RESOURCE &&
      view->post_process_blur_format == format)
  {
    return true;
  }

  s_renderer_post_process_blur_textures_destroy(renderer, view);

  LDKRHITextureDesc desc = {0};
  ldk_rhi_texture_desc_defaults(&desc);
  desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  desc.format = format;
  desc.width = view->width;
  desc.height = view->height;
  desc.depth = 1;
  desc.mip_count = 1;
  desc.layer_count = 1;
  desc.usage =
      LDK_RHI_TEXTURE_USAGE_RENDER_TARGET | LDK_RHI_TEXTURE_USAGE_SAMPLED;

  for (u32 i = 0; i < 2; i++)
  {
    view->post_process_blur_textures[i] =
        ldk_rhi_texture_create(renderer->rhi, &desc);
    if (view->post_process_blur_textures[i] == LDK_RHI_INVALID_RESOURCE)
    {
      s_renderer_post_process_blur_textures_destroy(renderer, view);
      return false;
    }
  }

  view->post_process_blur_format = format;
  return true;
}

static bool s_renderer_post_process_bindings_ensure(
    LDKRenderer* renderer, LDKRendererView* view,
    LDKRHITexture blurred_texture)
{
  LDKRHITexture input_texture;

  if (renderer == NULL || view == NULL ||
      !renderer->post_process_pass.is_initialized)
  {
    return false;
  }

  input_texture = view->target.color_texture;
  if (input_texture == LDK_RHI_INVALID_RESOURCE ||
      blurred_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  if (view->post_process_bindings != LDK_RHI_INVALID_RESOURCE &&
      view->post_process_input_texture == input_texture &&
      view->post_process_blurred_texture == blurred_texture)
  {
    return true;
  }

  s_renderer_post_process_bindings_destroy(renderer, view);

  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = renderer->post_process_pass.bindings_layout;
  desc.binding_count = 3;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = renderer->post_process_pass.params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererPostProcessParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].texture = input_texture;
  desc.bindings[1].sampler = renderer->post_process_pass.sampler;
  desc.bindings[2].slot = 2;
  desc.bindings[2].texture = blurred_texture;
  desc.bindings[2].sampler = renderer->post_process_pass.sampler;

  view->post_process_bindings =
      ldk_rhi_bindings_create(renderer->rhi, &desc);
  if (view->post_process_bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  view->post_process_input_texture = input_texture;
  view->post_process_blurred_texture = blurred_texture;
  return true;
}

static bool s_renderer_post_process_blur_bindings_ensure(
    LDKRenderer* renderer, LDKRendererView* view, u32 index,
    LDKRHITexture input_texture)
{
  if (renderer == NULL || view == NULL || index >= 2 ||
      input_texture == LDK_RHI_INVALID_RESOURCE ||
      !renderer->blur_pass.is_initialized)
  {
    return false;
  }

  if (view->post_process_blur_bindings[index] != LDK_RHI_INVALID_RESOURCE &&
      view->post_process_blur_input_textures[index] == input_texture)
  {
    return true;
  }

  if (view->post_process_blur_bindings[index] != LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_bindings_destroy(
        renderer->rhi, view->post_process_blur_bindings[index]);
  }
  view->post_process_blur_bindings[index] = LDK_RHI_INVALID_RESOURCE;
  view->post_process_blur_input_textures[index] = LDK_RHI_INVALID_RESOURCE;

  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = renderer->blur_pass.bindings_layout;
  desc.binding_count = 2;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = renderer->blur_pass.params_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererBlurParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].texture = input_texture;
  desc.bindings[1].sampler = renderer->blur_pass.sampler;

  view->post_process_blur_bindings[index] =
      ldk_rhi_bindings_create(renderer->rhi, &desc);
  if (view->post_process_blur_bindings[index] == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  view->post_process_blur_input_textures[index] = input_texture;
  return true;
}

static bool s_renderer_blur_pipeline_create(LDKRendererBlurPass* pass,
    LDKRHIFormat color_format, LDKRHIPipeline* out_pipeline)
{
  if (pass == NULL || out_pipeline == NULL)
  {
    return false;
  }

  LDKRHIPipelineDesc pipeline = {0};
  ldk_rhi_pipeline_desc_defaults(&pipeline);
  pipeline.vertex_shader_module = pass->vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pipeline.bindings_layout = pass->bindings_layout;
  pipeline.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  pipeline.blend_state.enabled = false;
  pipeline.depth_state.test_enabled = false;
  pipeline.depth_state.write_enabled = false;
  pipeline.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pipeline.raster_state.scissor_enabled = false;
  pipeline.vertex_layout.stride = sizeof(LDKRendererPostProcessVertex);
  pipeline.vertex_layout.attribute_count = 2;
  pipeline.vertex_layout.attributes[0].location = 0;
  pipeline.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKRendererPostProcessVertex, x);
  pipeline.vertex_layout.attributes[1].location = 1;
  pipeline.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKRendererPostProcessVertex, u);
  pipeline.color_attachment_count = 1;
  pipeline.color_formats[0] = color_format;
  pipeline.depth_format = LDK_RHI_FORMAT_INVALID;

  *out_pipeline = ldk_rhi_pipeline_create(pass->rhi, &pipeline);
  return *out_pipeline != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_blur_pass_initialize(
    LDKRendererBlurPass* pass, LDKRendererConfig const* config)
{
  static const LDKRendererPostProcessVertex vertices[] = {
      {-1.0f, 1.0f, 0.0f, 1.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f, 0.0f},
      {-1.0f, 1.0f, 0.0f, 1.0f},
      {1.0f, -1.0f, 1.0f, 0.0f},
      {-1.0f, -1.0f, 0.0f, 0.0f},
  };

  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_POST_PROCESS_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_BLUR_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
      pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  LDKRHIBindingsLayoutDesc bindings_layout = {0};
  ldk_rhi_bindings_layout_desc_defaults(&bindings_layout);
  bindings_layout.entry_count = 2;
  bindings_layout.entries[0].slot = 0;
  bindings_layout.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  bindings_layout.entries[0].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  bindings_layout.entries[1].slot = 1;
  bindings_layout.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  bindings_layout.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &bindings_layout);
  if (pass->bindings_layout == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  if (!s_renderer_blur_pipeline_create(
          pass, LDK_RHI_FORMAT_RGBA8_UNORM, &pass->ldr_pipeline) ||
      !s_renderer_blur_pipeline_create(
          pass, LDK_RHI_FORMAT_RGBA16_FLOAT, &pass->hdr_pipeline))
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  LDKRHIBufferDesc vertex_buffer = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_buffer);
  vertex_buffer.size = (u32)sizeof(vertices);
  vertex_buffer.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_buffer.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_buffer.initial_data = vertices;
  pass->vertex_buffer = ldk_rhi_buffer_create(pass->rhi, &vertex_buffer);
  if (pass->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  LDKRHIBufferDesc params_buffer = {0};
  ldk_rhi_buffer_desc_defaults(&params_buffer);
  params_buffer.size = sizeof(LDKRendererBlurParams);
  params_buffer.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_buffer.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->params_buffer = ldk_rhi_buffer_create(pass->rhi, &params_buffer);
  if (pass->params_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  LDKRHISamplerDesc sampler = {0};
  ldk_rhi_sampler_desc_defaults(&sampler);
  pass->sampler = ldk_rhi_sampler_create(pass->rhi, &sampler);
  if (pass->sampler == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_blur_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_blur_pass_terminate(LDKRendererBlurPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    if (pass->sampler != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_sampler_destroy(pass->rhi, pass->sampler);
    }
    if (pass->params_buffer != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    }
    if (pass->vertex_buffer != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    }
    if (pass->ldr_pipeline != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_pipeline_destroy(pass->rhi, pass->ldr_pipeline);
    }
    if (pass->hdr_pipeline != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_pipeline_destroy(pass->rhi, pass->hdr_pipeline);
    }
    if (pass->bindings_layout != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    }
    if (pass->fragment_shader_module != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_shader_module_destroy(
          pass->rhi, pass->fragment_shader_module);
    }
    if (pass->vertex_shader_module != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_shader_module_destroy(
          pass->rhi, pass->vertex_shader_module);
    }
  }

  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_post_process_pass_initialize(
    LDKRendererPostProcessPass* pass, LDKRendererConfig const* config)
{
  static const LDKRendererPostProcessVertex vertices[] = {
      {-1.0f, 1.0f, 0.0f, 1.0f},
      {1.0f, 1.0f, 1.0f, 1.0f},
      {1.0f, -1.0f, 1.0f, 0.0f},
      {-1.0f, 1.0f, 0.0f, 1.0f},
      {1.0f, -1.0f, 1.0f, 0.0f},
      {-1.0f, -1.0f, 0.0f, 0.0f},
  };

  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_POST_PROCESS_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_POST_PROCESS_PASS,
      LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
      pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  LDKRHIBindingsLayoutDesc bindings_layout = {0};
  ldk_rhi_bindings_layout_desc_defaults(&bindings_layout);
  bindings_layout.entry_count = 3;
  bindings_layout.entries[0].slot = 0;
  bindings_layout.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  bindings_layout.entries[0].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  bindings_layout.entries[1].slot = 1;
  bindings_layout.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  bindings_layout.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  bindings_layout.entries[2].slot = 2;
  bindings_layout.entries[2].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  bindings_layout.entries[2].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &bindings_layout);
  if (pass->bindings_layout == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  LDKRHIPipelineDesc pipeline = {0};
  ldk_rhi_pipeline_desc_defaults(&pipeline);
  pipeline.vertex_shader_module = pass->vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pipeline.bindings_layout = pass->bindings_layout;
  pipeline.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  pipeline.blend_state.enabled = false;
  pipeline.depth_state.test_enabled = false;
  pipeline.depth_state.write_enabled = false;
  pipeline.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pipeline.raster_state.scissor_enabled = false;
  pipeline.vertex_layout.stride = sizeof(LDKRendererPostProcessVertex);
  pipeline.vertex_layout.attribute_count = 2;
  pipeline.vertex_layout.attributes[0].location = 0;
  pipeline.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKRendererPostProcessVertex, x);
  pipeline.vertex_layout.attributes[1].location = 1;
  pipeline.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKRendererPostProcessVertex, u);
  pipeline.color_attachment_count = 1;
  pipeline.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  pipeline.depth_format = LDK_RHI_FORMAT_INVALID;
  pass->pipeline = ldk_rhi_pipeline_create(pass->rhi, &pipeline);
  if (pass->pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  LDKRHIBufferDesc vertex_buffer = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_buffer);
  vertex_buffer.size = (u32)sizeof(vertices);
  vertex_buffer.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_buffer.memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
  vertex_buffer.initial_data = vertices;
  pass->vertex_buffer = ldk_rhi_buffer_create(pass->rhi, &vertex_buffer);
  if (pass->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  LDKRHIBufferDesc params_buffer = {0};
  ldk_rhi_buffer_desc_defaults(&params_buffer);
  params_buffer.size = sizeof(LDKRendererPostProcessParams);
  params_buffer.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_buffer.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->params_buffer = ldk_rhi_buffer_create(pass->rhi, &params_buffer);
  if (pass->params_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  LDKRHISamplerDesc sampler = {0};
  ldk_rhi_sampler_desc_defaults(&sampler);
  pass->sampler = ldk_rhi_sampler_create(pass->rhi, &sampler);
  if (pass->sampler == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_post_process_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_post_process_pass_terminate(
    LDKRendererPostProcessPass* pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    if (pass->sampler != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_sampler_destroy(pass->rhi, pass->sampler);
    }
    if (pass->params_buffer != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    }
    if (pass->vertex_buffer != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    }
    if (pass->pipeline != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    }
    if (pass->bindings_layout != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    }
    if (pass->fragment_shader_module != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_shader_module_destroy(
          pass->rhi, pass->fragment_shader_module);
    }
    if (pass->vertex_shader_module != LDK_RHI_INVALID_RESOURCE)
    {
      ldk_rhi_shader_module_destroy(
          pass->rhi, pass->vertex_shader_module);
    }
  }

  memset(pass, 0, sizeof(*pass));
}

static LDKRHITexture s_renderer_view_output_texture(
    LDKRendererView const* view)
{
  if (view == NULL)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  if (view->post_process_texture != LDK_RHI_INVALID_RESOURCE)
  {
    return view->post_process_texture;
  }

  return view->target.color_texture;
}

static bool s_renderer_blur_draw(LDKRenderer* renderer,
    LDKRendererView* view, LDKRHITexture* out_texture)
{
  if (renderer == NULL || view == NULL || out_texture == NULL)
  {
    return false;
  }

  *out_texture = view->target.color_texture;
  if (!s_renderer_blur_active(&view->post_processing))
  {
    return true;
  }

  if (!s_renderer_post_process_blur_textures_ensure(
          renderer, view, view->target.color_format) ||
      !s_renderer_post_process_blur_bindings_ensure(
          renderer, view, 0, view->target.color_texture) ||
      !s_renderer_post_process_blur_bindings_ensure(
          renderer, view, 1, view->post_process_blur_textures[0]))
  {
    return false;
  }

  LDKRHIPipeline pipeline = LDK_RHI_INVALID_RESOURCE;
  if (view->target.color_format == LDK_RHI_FORMAT_RGBA16_FLOAT)
  {
    pipeline = renderer->blur_pass.hdr_pipeline;
  }
  else if (view->target.color_format == LDK_RHI_FORMAT_RGBA8_UNORM)
  {
    pipeline = renderer->blur_pass.ldr_pipeline;
  }
  if (pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  float strength = s_renderer_saturate(view->post_processing.blur_strength);
  float sample_scale = strength * 4.0f;
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  for (u32 i = 0; i < 2; i++)
  {
    LDKRendererBlurParams params = {0};
    params.direction[0] = i == 0
        ? sample_scale / (float)view->width
        : 0.0f;
    params.direction[1] = i == 1
        ? sample_scale / (float)view->height
        : 0.0f;
    if (!ldk_rhi_buffer_update(renderer->rhi,
            renderer->blur_pass.params_buffer, 0, sizeof(params), &params))
    {
      return false;
    }

    LDKRHIPassDesc pass_desc = {0};
    ldk_rhi_pass_desc_defaults(&pass_desc);
    pass_desc.color_attachment_count = 1;
    pass_desc.color_attachments[0].texture =
        view->post_process_blur_textures[i];
    pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
    pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
    pass_desc.has_viewport = true;
    pass_desc.viewport.x = 0.0f;
    pass_desc.viewport.y = 0.0f;
    pass_desc.viewport.width = (float)view->width;
    pass_desc.viewport.height = (float)view->height;
    pass_desc.viewport.min_depth = 0.0f;
    pass_desc.viewport.max_depth = 1.0f;

    ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
    ldk_rhi_pipeline_bind(renderer->rhi, pipeline);
    ldk_rhi_bindings_bind(renderer->rhi,
        view->post_process_blur_bindings[i]);
    ldk_rhi_vertex_buffer_bind(
        renderer->rhi, renderer->blur_pass.vertex_buffer, 0);

    LDKRHIDrawDesc draw_desc = {0};
    draw_desc.vertex_count = 6;
    draw_desc.first_vertex = 0;
    ldk_rhi_draw(renderer->rhi, &draw_desc);
    ldk_rhi_pass_end(renderer->rhi);

    if (stats != NULL)
    {
      stats->draw_call_count += 1;
    }
  }

  *out_texture = view->post_process_blur_textures[1];
  return true;
}

static bool s_renderer_post_process_draw(
    LDKRenderer* renderer, LDKRendererView* view)
{
  if (renderer == NULL || view == NULL ||
      !renderer->post_process_pass.is_initialized)
  {
    return false;
  }

  LDKRHITexture blurred_texture = view->target.color_texture;
  if (!s_renderer_post_process_texture_ensure(renderer, view) ||
      !s_renderer_blur_draw(renderer, view, &blurred_texture) ||
      !s_renderer_post_process_bindings_ensure(
          renderer, view, blurred_texture))
  {
    return false;
  }

  LDKRHIPassDesc pass_desc = {0};
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = view->post_process_texture;
  pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)view->width;
  pass_desc.viewport.height = (float)view->height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  LDKRendererPostProcessing disabled = {0};
  const LDKRendererPostProcessing *post_processing =
      view->post_processing.enabled ? &view->post_processing : &disabled;
  bool blur_active = s_renderer_blur_active(post_processing);
  float focus_x = isfinite(post_processing->blur_focus_x)
      ? post_processing->blur_focus_x
      : 0.5f;
  float focus_y = isfinite(post_processing->blur_focus_y)
      ? post_processing->blur_focus_y
      : 0.5f;

  LDKRendererPostProcessParams params = {0};
  params.tonemapping[0] = post_processing->exposure;
  params.tonemapping[1] =
      post_processing->tonemapping_enabled ? 1.0f : 0.0f;
  params.color_adjustments[0] = post_processing->brightness;
  params.color_adjustments[1] = post_processing->contrast;
  params.color_adjustments[2] = post_processing->saturation;
  params.color_adjustments[3] =
      post_processing->color_enabled ? 1.0f : 0.0f;
  params.blur_focus[0] = focus_x;
  params.blur_focus[1] = focus_y;
  params.blur_focus[2] =
      s_renderer_saturate(post_processing->blur_focus_size);
  params.blur_focus[3] =
      s_renderer_saturate(post_processing->blur_focus_feather);
  params.blur_options[0] = blur_active ? 1.0f : 0.0f;
  params.blur_options[1] =
      post_processing->blur_inverted ? 1.0f : 0.0f;
  params.blur_options[2] =
      (float)view->width / (float)view->height;
  params.vignette[0] =
      s_renderer_saturate(post_processing->vignette_intensity);
  params.vignette[1] =
      s_renderer_saturate(post_processing->vignette_radius);
  params.vignette[2] =
      s_renderer_saturate(post_processing->vignette_softness);
  params.vignette[3] =
      post_processing->vignette_enabled ? 1.0f : 0.0f;
  float distortion_strength =
      isfinite(post_processing->screen_distortion_strength)
      ? post_processing->screen_distortion_strength
      : 0.0f;
  if (distortion_strength < -1.0f)
  {
    distortion_strength = -1.0f;
  }
  else if (distortion_strength > 1.0f)
  {
    distortion_strength = 1.0f;
  }
  params.lens[0] = distortion_strength;
  params.lens[1] =
      post_processing->screen_distortion_enabled ? 1.0f : 0.0f;
  params.lens[2] =
      s_renderer_saturate(post_processing->chromatic_aberration_strength);
  params.lens[3] =
      post_processing->chromatic_aberration_enabled ? 1.0f : 0.0f;
  params.heat[0] =
      s_renderer_saturate(post_processing->heat_strength);
  params.heat[1] = s_renderer_clamp_finite(
      post_processing->heat_amplitude, 0.0f, 32.0f, 4.0f);
  params.heat[2] = s_renderer_clamp_finite(
      post_processing->heat_scale, 0.25f, 4.0f, 1.0f);
  params.heat[3] = s_renderer_clamp_finite(
      post_processing->heat_speed, 0.0f, 4.0f, 1.0f);
  params.stylization_options[0] =
      post_processing->heat_enabled ? 1.0f : 0.0f;
  params.stylization_options[1] =
      post_processing->inverse_enabled ? 1.0f : 0.0f;
  params.stylization_options[2] =
      post_processing->black_and_white_enabled ? 1.0f : 0.0f;
  params.stylization_options[3] =
      post_processing->retro_enabled ? 1.0f : 0.0f;
  params.retro[0] = s_renderer_clamp_finite(
      post_processing->retro_pixel_size, 1.0f, 16.0f, 2.0f);
  params.retro[1] = s_renderer_clamp_finite(
      post_processing->retro_color_levels, 2.0f, 16.0f, 4.0f);
  params.retro[2] =
      s_renderer_saturate(post_processing->retro_dither_strength);
  params.retro[3] = s_renderer_clamp_finite(
      post_processing->retro_pattern_scale, 1.0f, 8.0f, 1.0f);
  params.drunk_primary[0] =
      s_renderer_saturate(post_processing->drunk_strength);
  params.drunk_primary[1] = s_renderer_clamp_finite(
      post_processing->drunk_speed, 0.0f, 4.0f, 1.0f);
  params.drunk_primary[2] = s_renderer_clamp_finite(
      post_processing->drunk_movement, 0.0f, 3.0f, 0.5f);
  params.drunk_primary[3] =
      post_processing->drunk_enabled ? 1.0f : 0.0f;
  params.drunk_secondary[0] =
      s_renderer_saturate(post_processing->drunk_ghosting);
  params.drunk_secondary[1] = s_renderer_saturate(
      post_processing->drunk_chromatic_aberration);
  params.animation[0] = renderer->animation_time_seconds;
  if (!ldk_rhi_buffer_update(renderer->rhi,
          renderer->post_process_pass.params_buffer, 0, sizeof(params),
          &params))
  {
    return false;
  }

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  ldk_rhi_pipeline_bind(renderer->rhi, renderer->post_process_pass.pipeline);
  ldk_rhi_bindings_bind(renderer->rhi, view->post_process_bindings);
  ldk_rhi_vertex_buffer_bind(
      renderer->rhi, renderer->post_process_pass.vertex_buffer, 0);

  LDKRHIDrawDesc draw_desc = {0};
  draw_desc.vertex_count = 6;
  draw_desc.first_vertex = 0;
  ldk_rhi_draw(renderer->rhi, &draw_desc);
  ldk_rhi_pass_end(renderer->rhi);

  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);
  if (stats != NULL)
  {
    stats->draw_call_count += 1;
  }

  return true;
}

bool ldk_renderer_view_post_processing_set(
    LDKRenderer* renderer, LDKRendererViewId view_id,
    LDKRendererPostProcessing const* post_processing)
{
  if (renderer == NULL || !renderer->is_initialized ||
      post_processing == NULL)
  {
    return false;
  }

  LDKRendererView* view = s_renderer_view_find(renderer, view_id);
  if (view == NULL || !view->submitted ||
      view->width == 0 || view->height == 0)
  {
    return false;
  }

  view->post_processing = *post_processing;

  LDKRHIFormat scene_color_format = s_renderer_view_requires_hdr(view)
      ? LDK_RHI_FORMAT_RGBA16_FLOAT
      : LDK_RHI_FORMAT_RGBA8_UNORM;

  if (view->target.color_texture != LDK_RHI_INVALID_RESOURCE &&
      (view->target.width != (i32)view->width ||
          view->target.height != (i32)view->height ||
          view->target.color_format != scene_color_format))
  {
    s_renderer_post_process_bindings_destroy(renderer, view);
    s_renderer_post_process_blur_bindings_destroy(renderer, view);
  }

  if (!s_renderer_target_ensure(renderer, &view->target,
          (i32)view->width, (i32)view->height, scene_color_format))
  {
    view->post_processing.enabled = false;
    return false;
  }

  if (!s_renderer_post_process_texture_ensure(renderer, view))
  {
    view->post_processing.enabled = false;
    return false;
  }

  return true;
}

static void s_renderer_present_view_pass(LDKRenderer* renderer,
    LDKRendererView const* view, const LDKRendererFrameDesc* frame_desc)
{
  if (renderer == NULL || view == NULL || frame_desc == NULL)
  {
    return;
  }

  LDKRHITexture color_texture = s_renderer_view_output_texture(view);
  if (color_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  i32 width = frame_desc->framebuffer_width;
  i32 height = frame_desc->framebuffer_height;

  if (width <= 0 || height <= 0)
  {
    return;
  }

  float game_aspect = (float)renderer->game_width / (float)renderer->game_height;
  float framebuffer_aspect = (float)width / (float)height;
  float present_width = (float)width;
  float present_height = (float)height;

  if (framebuffer_aspect > game_aspect)
  {
    present_width = present_height * game_aspect;
  }
  else
  {
    present_height = present_width / game_aspect;
  }

  float present_x = ((float)width - present_width) * 0.5f;
  float present_y = ((float)height - present_height) * 0.5f;

  LDKUIRect uv = ldk_renderer_view_texture_uv_get(renderer);
  LDKUIVertex vertices[4] =
  {
    {present_x, present_y, uv.x, uv.y, 0xffffffffu},
    {present_x + present_width, present_y,
        uv.x + uv.w, uv.y, 0xffffffffu},
    {present_x + present_width, present_y + present_height,
        uv.x + uv.w, uv.y + uv.h, 0xffffffffu},
    {present_x, present_y + present_height,
        uv.x, uv.y + uv.h, 0xffffffffu},
  };

  u32 indices[6] =
  {
    0, 1, 2,
    0, 2, 3,
  };

  LDKUIDrawCmd command = {0};
  command.texture = (LDKUITextureHandle)color_texture;
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
  render_data.viewport_size.w = (float)width;
  render_data.viewport_size.h = (float)height;

  LDKRendererFrameDesc present_desc = *frame_desc;
  present_desc.clear_color = 0x000000FFu;
  present_desc.clear_color_enabled = true;

  s_renderer_ui_pass(renderer, &renderer->ui_pass,
      &renderer->current_frame_stats.game, &render_data, &present_desc, true);
}

