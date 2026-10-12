// ---------------------------------------------------------------------------
// Internal pass: world-space text
// ---------------------------------------------------------------------------

static bool s_renderer_text_pass_create_shaders(LDKRendererTextPass *pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_TEXT_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_TEXT_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_text_pass_create_layout(LDKRendererTextPass *pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 3;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[2].slot = 2;
  desc.entries[2].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[2].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  pass->bindings_layout = ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static LDKRHIPipeline s_renderer_text_pass_create_pipeline(
    LDKRendererTextPass *pass, LDKRHIFormat color_format, bool depth_test)
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
  desc.depth_state.test_enabled = depth_test;
  desc.depth_state.write_enabled = false;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  desc.vertex_layout.stride = sizeof(LDKRendererTextVertex);
  desc.vertex_layout.attribute_count = 3;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKRendererTextVertex, x);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKRendererTextVertex, u);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[2].offset =
      (u32)offsetof(LDKRendererTextVertex, color);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = color_format;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  return ldk_rhi_pipeline_create(pass->rhi, &desc);
}

static bool s_renderer_text_pass_create_buffers(LDKRendererTextPass *pass)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = pass->vertex_capacity * (u32)sizeof(LDKRendererTextVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->vertex_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (pass->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = pass->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->index_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (pass->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = sizeof(LDKRendererTextCameraParams);
  desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->camera_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (pass->camera_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = sizeof(LDKRendererTextObjectParams);
  desc.usage = LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->object_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  return pass->object_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_text_pass_initialize(
    LDKRendererTextPass *pass, LDKRendererConfig const *config)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;
  pass->vertex_capacity = config->initial_ui_vertex_capacity > 0
      ? config->initial_ui_vertex_capacity : 1024;
  pass->index_capacity = config->initial_ui_index_capacity > 0
      ? config->initial_ui_index_capacity : 2048;

  if (!s_renderer_text_pass_create_shaders(pass) ||
      !s_renderer_text_pass_create_layout(pass))
  {
    s_renderer_text_pass_terminate(pass);
    return false;
  }

  pass->ldr_pipeline = s_renderer_text_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA8_UNORM, true);
  pass->hdr_pipeline = s_renderer_text_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA16_FLOAT, true);
  pass->ldr_no_depth_pipeline = s_renderer_text_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA8_UNORM, false);
  pass->hdr_no_depth_pipeline = s_renderer_text_pass_create_pipeline(
      pass, LDK_RHI_FORMAT_RGBA16_FLOAT, false);
  if (pass->ldr_pipeline == LDK_RHI_INVALID_RESOURCE ||
      pass->hdr_pipeline == LDK_RHI_INVALID_RESOURCE ||
      pass->ldr_no_depth_pipeline == LDK_RHI_INVALID_RESOURCE ||
      pass->hdr_no_depth_pipeline == LDK_RHI_INVALID_RESOURCE ||
      !s_renderer_text_pass_create_buffers(pass))
  {
    s_renderer_text_pass_terminate(pass);
    return false;
  }

  LDKRHISamplerDesc sampler = {0};
  ldk_rhi_sampler_desc_defaults(&sampler);
  pass->sampler = ldk_rhi_sampler_create(pass->rhi, &sampler);
  if (pass->sampler == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_text_pass_terminate(pass);
    return false;
  }

  pass->is_initialized = true;
  return true;
}

static void s_renderer_text_pass_terminate(LDKRendererTextPass *pass)
{
  if (pass == NULL)
  {
    return;
  }

  if (pass->rhi != NULL)
  {
    for (u32 i = 0; i < pass->bindings_cache_count; ++i)
    {
      ldk_rhi_bindings_destroy(pass->rhi, pass->bindings_cache[i].bindings);
    }
    ldk_rhi_sampler_destroy(pass->rhi, pass->sampler);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->index_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->hdr_no_depth_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->ldr_no_depth_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->hdr_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->ldr_pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
  }

  LDK_RENDERER_FREE(pass->bindings_cache);
  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_text_pass_recreate_vertex_buffer(
    LDKRendererTextPass *pass)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = pass->vertex_capacity * (u32)sizeof(LDKRendererTextVertex);
  desc.usage = LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  LDKRHIBuffer buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  ldk_rhi_buffer_destroy(pass->rhi, pass->vertex_buffer);
  pass->vertex_buffer = buffer;
  return true;
}

static bool s_renderer_text_pass_recreate_index_buffer(
    LDKRendererTextPass *pass)
{
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = pass->index_capacity * (u32)sizeof(u32);
  desc.usage = LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  LDKRHIBuffer buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  ldk_rhi_buffer_destroy(pass->rhi, pass->index_buffer);
  pass->index_buffer = buffer;
  return true;
}

static bool s_renderer_text_pass_ensure_capacity(LDKRendererTextPass *pass,
    u32 vertex_count, u32 index_count)
{
  if (vertex_count > pass->vertex_capacity)
  {
    u32 capacity = pass->vertex_capacity;
    while (capacity < vertex_count && capacity <= UINT32_MAX / 2u)
    {
      capacity *= 2u;
    }
    if (capacity < vertex_count)
    {
      capacity = vertex_count;
    }
    pass->vertex_capacity = capacity;
    if (!s_renderer_text_pass_recreate_vertex_buffer(pass))
    {
      return false;
    }
  }

  if (index_count > pass->index_capacity)
  {
    u32 capacity = pass->index_capacity;
    while (capacity < index_count && capacity <= UINT32_MAX / 2u)
    {
      capacity *= 2u;
    }
    if (capacity < index_count)
    {
      capacity = index_count;
    }
    pass->index_capacity = capacity;
    if (!s_renderer_text_pass_recreate_index_buffer(pass))
    {
      return false;
    }
  }

  return true;
}

static LDKRHIBindings s_renderer_text_pass_get_bindings(
    LDKRendererTextPass *pass, LDKRHITexture texture)
{
  for (u32 i = 0; i < pass->bindings_cache_count; ++i)
  {
    if (pass->bindings_cache[i].texture == texture &&
        pass->bindings_cache[i].sampler == pass->sampler)
    {
      return pass->bindings_cache[i].bindings;
    }
  }

  if (pass->bindings_cache_count == pass->bindings_cache_capacity)
  {
    u32 capacity = pass->bindings_cache_capacity == 0
        ? 16 : pass->bindings_cache_capacity * 2;
    size_t size = (size_t)capacity * sizeof(LDKRendererBindingsCacheEntry);
    LDKRendererBindingsCacheEntry *cache = LDK_RENDERER_REALLOC(
        pass->bindings_cache, size);
    if (cache == NULL)
    {
      return LDK_RHI_INVALID_RESOURCE;
    }
    pass->bindings_cache = cache;
    pass->bindings_cache_capacity = capacity;
  }

  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 3;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_size = sizeof(LDKRendererTextCameraParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_size = sizeof(LDKRendererTextObjectParams);
  desc.bindings[2].slot = 2;
  desc.bindings[2].texture = texture;
  desc.bindings[2].sampler = pass->sampler;
  LDKRHIBindings bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return bindings;
  }

  LDKRendererBindingsCacheEntry *entry =
      &pass->bindings_cache[pass->bindings_cache_count++];
  entry->texture = texture;
  entry->sampler = pass->sampler;
  entry->bindings = bindings;
  return bindings;
}

static void s_renderer_text_pass_remove_texture_bindings(
    LDKRendererTextPass *pass, LDKRHITexture texture)
{
  if (pass == NULL || pass->rhi == NULL ||
      texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 i = 0;
  while (i < pass->bindings_cache_count)
  {
    LDKRendererBindingsCacheEntry *entry = &pass->bindings_cache[i];
    if (entry->texture != texture)
    {
      ++i;
      continue;
    }

    ldk_rhi_bindings_destroy(pass->rhi, entry->bindings);
    --pass->bindings_cache_count;
    if (i != pass->bindings_cache_count)
    {
      *entry = pass->bindings_cache[pass->bindings_cache_count];
    }
  }
}

static void s_renderer_text_pass_draw(LDKRenderer *renderer,
    LDKRendererTextPass *pass, LDKRendererView const *view)
{
  if (renderer == NULL || pass == NULL || !pass->is_initialized ||
      view == NULL || !view->submitted || renderer->submitted_text_count == 0 ||
      renderer->submitted_text_vertex_count == 0 ||
      renderer->submitted_text_index_count == 0)
  {
    return;
  }

  if (!s_renderer_text_pass_ensure_capacity(pass,
          renderer->submitted_text_vertex_count,
          renderer->submitted_text_index_count))
  {
    return;
  }

  LDKRendererTextCameraParams camera = {0};
  camera.view = view->view;
  camera.projection = view->projection;
  if (!ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0,
          sizeof(camera), &camera) ||
      !ldk_rhi_buffer_update(pass->rhi, pass->vertex_buffer, 0,
          renderer->submitted_text_vertex_count *
              (u32)sizeof(LDKRendererTextVertex),
          renderer->submitted_text_vertices) ||
      !ldk_rhi_buffer_update(pass->rhi, pass->index_buffer, 0,
          renderer->submitted_text_index_count * (u32)sizeof(u32),
          renderer->submitted_text_indices))
  {
    return;
  }

  LDKRHIPipeline bound_pipeline = LDK_RHI_INVALID_RESOURCE;
  ldk_rhi_vertex_buffer_bind(pass->rhi, pass->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(
      pass->rhi, pass->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);

  for (u32 i = 0; i < renderer->submitted_text_count; ++i)
  {
    LDKRendererTextSubmit const *submit = &renderer->submitted_texts[i];
    if (submit->view_id != LDK_RENDERER_VIEW_ALL &&
        submit->view_id != view->id)
    {
      continue;
    }

    bool depth_test =
        (submit->flags & LDK_RENDERER_TEXT_SUBMIT_FLAG_NO_DEPTH_TEST) == 0;
    LDKRHIPipeline pipeline;
    if (view->target.color_format == LDK_RHI_FORMAT_RGBA16_FLOAT)
    {
      pipeline = depth_test
          ? pass->hdr_pipeline : pass->hdr_no_depth_pipeline;
    }
    else
    {
      pipeline = depth_test
          ? pass->ldr_pipeline : pass->ldr_no_depth_pipeline;
    }
    if (pipeline != bound_pipeline)
    {
      ldk_rhi_pipeline_bind(pass->rhi, pipeline);
      bound_pipeline = pipeline;
    }

    LDKRHIBindings bindings =
        s_renderer_text_pass_get_bindings(pass, submit->texture);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }

    LDKRendererTextObjectParams object = {0};
    object.world = submit->world;
    object.options[0] =
        (submit->flags & LDK_RENDERER_TEXT_SUBMIT_FLAG_BILLBOARD)
        ? 1.0f : 0.0f;
    if (!ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
            sizeof(object), &object))
    {
      continue;
    }

    ldk_rhi_bindings_bind(pass->rhi, bindings);
    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = submit->index_count;
    draw.first_index = submit->index_offset;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    if (stats != NULL)
    {
      stats->draw_call_count += 1;
    }
  }
}

