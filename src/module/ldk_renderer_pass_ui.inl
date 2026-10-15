// ---------------------------------------------------------------------------
// Internal pass: UI / present
// ---------------------------------------------------------------------------

static bool s_renderer_ui_pass_create_shaders(LDKRendererUIPass* renderer)
{
  renderer->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (renderer->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  renderer->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      renderer->rhi, LDK_SHADER_UI_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (renderer->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->vertex_shader_module);
    renderer->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  return true;
}

static bool s_renderer_ui_pass_bindings_create_layout(
    LDKRendererUIPass* renderer)
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

  renderer->bindings_layout =
      ldk_rhi_bindings_layout_create(renderer->rhi, &desc);
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
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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

  renderer->white_texture =
      ldk_rhi_texture_create(renderer->rhi, &texture_desc);
  return renderer->white_texture != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_create_sampler(LDKRendererUIPass* renderer)
{
  LDKRHISamplerDesc desc = {0};
  ldk_rhi_sampler_desc_defaults(&desc);

  renderer->sampler = ldk_rhi_sampler_create(renderer->rhi, &desc);
  return renderer->sampler != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_buffer_creates(LDKRendererUIPass* renderer)
{
  LDKRHIBufferDesc vertex_desc = {0};
  ldk_rhi_buffer_desc_defaults(&vertex_desc);
  vertex_desc.size = renderer->vertex_capacity * (u32)sizeof(LDKUIVertex);
  vertex_desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  vertex_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->vertex_buffer = ldk_rhi_buffer_create(renderer->rhi, &vertex_desc);
  if (renderer->vertex_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc index_desc = {0};
  ldk_rhi_buffer_desc_defaults(&index_desc);
  index_desc.size = renderer->index_capacity * (u32)sizeof(u32);
  index_desc.usage =
      LDK_RHI_BUFFER_USAGE_INDEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  index_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->index_buffer = ldk_rhi_buffer_create(renderer->rhi, &index_desc);
  if (renderer->index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc params_desc = {0};
  ldk_rhi_buffer_desc_defaults(&params_desc);
  params_desc.size = sizeof(LDKRendererUIParams);
  params_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  params_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  renderer->params_buffer = ldk_rhi_buffer_create(renderer->rhi, &params_desc);
  return renderer->params_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_ui_pass_recreate_vertex_buffer(
    LDKRendererUIPass* renderer)
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

static bool s_renderer_ui_pass_recreate_index_buffer(
    LDKRendererUIPass* renderer)
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

static bool s_renderer_ui_pass_ensure_vertex_capacity(
    LDKRendererUIPass* renderer, u32 vertex_count)
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

static bool s_renderer_ui_pass_ensure_index_capacity(
    LDKRendererUIPass* renderer, u32 index_count)
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

static bool s_renderer_ui_pass_initialize(
    LDKRendererUIPass* renderer, LDKRendererConfig const* config)
{
  if (renderer == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->rhi = config->rhi;
  renderer->vertex_capacity = config->initial_ui_vertex_capacity > 0
      ? config->initial_ui_vertex_capacity : 1024;
  renderer->index_capacity = config->initial_ui_index_capacity > 0
      ? config->initial_ui_index_capacity : 2048;

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
      ldk_rhi_bindings_destroy(
          renderer->rhi, renderer->bindings_cache[i].bindings);
    }

    ldk_rhi_buffer_destroy(renderer->rhi, renderer->params_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->index_buffer);
    ldk_rhi_buffer_destroy(renderer->rhi, renderer->vertex_buffer);
    ldk_rhi_sampler_destroy(renderer->rhi, renderer->sampler);
    ldk_rhi_texture_destroy(renderer->rhi, renderer->white_texture);
    ldk_rhi_pipeline_destroy(renderer->rhi, renderer->pipeline);
    ldk_rhi_bindings_layout_destroy(renderer->rhi, renderer->bindings_layout);
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        renderer->rhi, renderer->vertex_shader_module);
  }

  LDK_RENDERER_FREE(renderer->bindings_cache);
  memset(renderer, 0, sizeof(*renderer));
}

static LDKRHIBindings s_renderer_ui_pass_create_draw_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
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
  desc.bindings[1].sampler = sampler;
  return ldk_rhi_bindings_create(renderer->rhi, &desc);
}

static LDKRHIBindings s_renderer_ui_pass_find_cached_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
{
  for (u32 i = 0; i < renderer->bindings_cache_count; ++i)
  {
    if (renderer->bindings_cache[i].texture == texture &&
        renderer->bindings_cache[i].sampler == sampler)
    {
      return renderer->bindings_cache[i].bindings;
    }
  }

  return LDK_RHI_INVALID_RESOURCE;
}

static void s_renderer_ui_pass_remove_texture_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture)
{
  if (renderer == NULL || renderer->rhi == NULL ||
      texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < renderer->bindings_cache_count)
  {
    LDKRendererBindingsCacheEntry* entry = &renderer->bindings_cache[index];
    if (entry->texture != texture)
    {
      index += 1;
      continue;
    }

    ldk_rhi_bindings_destroy(renderer->rhi, entry->bindings);
    renderer->bindings_cache_count -= 1;

    if (index != renderer->bindings_cache_count)
    {
      *entry = renderer->bindings_cache[renderer->bindings_cache_count];
    }

    memset(&renderer->bindings_cache[renderer->bindings_cache_count],
        0, sizeof(*entry));
  }
}

static bool s_renderer_ui_pass_grow_bindings_cache(
    LDKRendererUIPass* renderer)
{
  u32 new_capacity = renderer->bindings_cache_capacity == 0
      ? 16 : renderer->bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererBindingsCacheEntry);
  LDKRendererBindingsCacheEntry* new_cache = renderer->bindings_cache == NULL
      ? (LDKRendererBindingsCacheEntry*)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererBindingsCacheEntry*)LDK_RENDERER_REALLOC(
            renderer->bindings_cache, new_size);

  if (new_cache == NULL)
  {
    return false;
  }

  renderer->bindings_cache = new_cache;
  renderer->bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_ui_pass_get_draw_bindings(
    LDKRendererUIPass* renderer, LDKRHITexture texture, LDKRHISampler sampler)
{
  LDKRHIBindings cached_bindings =
      s_renderer_ui_pass_find_cached_bindings(renderer, texture, sampler);
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

  LDKRHIBindings bindings =
      s_renderer_ui_pass_create_draw_bindings(renderer, texture, sampler);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererBindingsCacheEntry* entry =
      &renderer->bindings_cache[renderer->bindings_cache_count];
  entry->texture = texture;
  entry->sampler = sampler;
  entry->bindings = bindings;
  renderer->bindings_cache_count += 1;
  return bindings;
}

static void s_renderer_ui_pass(LDKRenderer* owner,
    LDKRendererUIPass* renderer, LDKRendererFrameDomainStats* stats,
    LDKUIRenderData const* render_data,
    const LDKRendererFrameDesc* frame_desc, bool present)
{
  if (renderer == NULL || !renderer->is_initialized || render_data == NULL ||
      frame_desc == NULL)
  {
    return;
  }

  i32 framebuffer_width = frame_desc->framebuffer_width;
  i32 framebuffer_height = frame_desc->framebuffer_height;
  float ui_width = render_data->viewport_size.w;
  float ui_height = render_data->viewport_size.h;

  if (framebuffer_width <= 0 || framebuffer_height <= 0)
  {
    return;
  }

  /* Older/manual render-data producers may not provide a logical viewport.
   * In that case UI coordinates keep their historical one-unit-per-pixel
   * behavior. */
  if (!isfinite(ui_width) || ui_width <= 0.0f)
  {
    ui_width = (float)framebuffer_width;
  }
  if (!isfinite(ui_height) || ui_height <= 0.0f)
  {
    ui_height = (float)framebuffer_height;
  }

  if (render_data->vertex_count == 0 || render_data->index_count == 0 ||
      render_data->command_count == 0)
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_vertex_capacity(
          renderer, render_data->vertex_count))
  {
    return;
  }

  if (!s_renderer_ui_pass_ensure_index_capacity(
          renderer, render_data->index_count))
  {
    return;
  }

  LDKRHIPassDesc pass_desc;
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1;
  pass_desc.color_attachments[0].texture = LDK_RHI_INVALID_RESOURCE;
  pass_desc.color_attachments[0].load_op = frame_desc->clear_color_enabled
      ? LDK_RHI_LOAD_OP_CLEAR : LDK_RHI_LOAD_OP_LOAD;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.color_attachments[0].clear_color =
      s_renderer_color_from_rgba32_unorm(frame_desc->clear_color);
  pass_desc.has_viewport = true;
  pass_desc.viewport.x = 0.0f;
  pass_desc.viewport.y = 0.0f;
  pass_desc.viewport.width = (float)framebuffer_width;
  pass_desc.viewport.height = (float)framebuffer_height;
  pass_desc.viewport.min_depth = 0.0f;
  pass_desc.viewport.max_depth = 1.0f;

  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);

  LDKRendererUIParams params = {0};
  params.viewport_size[0] = ui_width;
  params.viewport_size[1] = ui_height;

  ldk_rhi_buffer_update(renderer->rhi, renderer->params_buffer, 0,
      sizeof(params), &params);
  ldk_rhi_buffer_update(renderer->rhi, renderer->vertex_buffer, 0,
      render_data->vertex_count * (u32)sizeof(LDKUIVertex),
      render_data->vertices);
  ldk_rhi_buffer_update(renderer->rhi, renderer->index_buffer, 0,
      render_data->index_count * (u32)sizeof(u32), render_data->indices);

  ldk_rhi_pipeline_bind(renderer->rhi, renderer->pipeline);
  ldk_rhi_vertex_buffer_bind(renderer->rhi, renderer->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(renderer->rhi, renderer->index_buffer, 0,
      LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < render_data->command_count; i++)
  {
    const LDKUIDrawCmd* cmd = &render_data->commands[i];
    LDKRHITexture texture = (LDKRHITexture)cmd->texture;
    if (texture == LDK_RHI_INVALID_RESOURCE)
    {
      texture = renderer->white_texture;
    }

    LDKRHISampler sampler =
        s_renderer_texture_sampler_from_rhi_texture(owner, texture);
    if (sampler == LDK_RHI_INVALID_RESOURCE)
    {
      sampler = renderer->sampler;
    }

    LDKRHIBindings bindings =
        s_renderer_ui_pass_get_draw_bindings(renderer, texture, sampler);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }

    float scale_x = (float)framebuffer_width / ui_width;
    float scale_y = (float)framebuffer_height / ui_height;
    LDKRHIRect scissor = {0};
    float x0 = floorf(cmd->clip_rect.x * scale_x);
    float y0 = floorf(cmd->clip_rect.y * scale_y);
    float x1 = ceilf((cmd->clip_rect.x + cmd->clip_rect.w) * scale_x);
    float y1 = ceilf((cmd->clip_rect.y + cmd->clip_rect.h) * scale_y);

    x0 = fmaxf(0.0f, fminf(x0, (float)framebuffer_width));
    y0 = fmaxf(0.0f, fminf(y0, (float)framebuffer_height));
    x1 = fmaxf(0.0f, fminf(x1, (float)framebuffer_width));
    y1 = fmaxf(0.0f, fminf(y1, (float)framebuffer_height));

    scissor.x = (i32)x0;
    scissor.y = (i32)y0;
    scissor.width = (i32)(x1 - x0);
    scissor.height = (i32)(y1 - y0);

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
    stats->draw_call_count += 1;
    if (present)
    {
      stats->present_draw_call_count += 1;
    }
    else
    {
      stats->ui_draw_call_count += 1;
    }
  }

  ldk_rhi_pass_end(renderer->rhi);
}

