/* Water rendering. */

typedef struct LDKRendererWaterParams
{
  Mat4 view_projection;
  Mat4 inverse_view_projection;
  float camera_time[4];
  float viewport[4];
  LDKRHIColor shallow_color;
  LDKRHIColor deep_color;
  LDKRHIColor foam_color;
  float waves[LDK_RENDERER_WATER_WAVE_COUNT][4];
  float speeds[4];
  float surface[4];
  float foam[4];
  float shore[4];
  float sun_direction[4];
  float sun_color[4];
  float ambient[4];
  float texture_flags[LDK_RENDERER_WATER_TEXTURE_COUNT];
  float noise[4];
  float foam_detail[4];
  float caustics[4];
} LDKRendererWaterParams;

LDK_STATIC_ASSERT(
    sizeof(LDKRendererWaterParams) == 432, water_params_std140_size);
LDK_STATIC_ASSERT(
    offsetof(LDKRendererWaterParams, waves) == 208, water_waves_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererWaterParams, ambient) == 352,
    water_ambient_std140_offset);
LDK_STATIC_ASSERT(offsetof(LDKRendererWaterParams, texture_flags) == 368,
    water_textures_std140_offset);

/* Match the normal-map and shader-defined material binding slots. */
static const u32 s_water_texture_slots[LDK_RENDERER_WATER_TEXTURE_COUNT] = {
    6u, 9u, 10u, 11u};

LDK_API void ldk_renderer_water_desc_defaults(LDKRendererWaterDesc *desc)
{
  if (!desc)
  {
    return;
  }
  memset(desc, 0, sizeof(*desc));
  desc->shallow_color = 0x73bdb35cu;
  desc->deep_color = 0x24546dffu;
  desc->depth_color_distance = 2.0f;
  desc->edge_fade_distance = 0.04f;
  desc->waves[0] = (LDKRendererWaterWave){0.06f, 16.0f, 1.2f, 25.0f};
  desc->waves[1] = (LDKRendererWaterWave){0.035f, 9.0f, 0.85f, 55.0f};
  desc->waves[2] = (LDKRendererWaterWave){0.015f, 4.0f, 0.6f, 110.0f};
  desc->detail_scale = 0.8f;
  desc->detail_strength = 0.045f;
  desc->detail_speed = 0.25f;
  desc->specular = 0.35f;
  desc->shininess = 64.0f;
  desc->foam_color = 0xe7f3e2e6u;
  desc->foam_width = 0.6f;
  desc->foam_strength = 0.8f;
  desc->foam_scale = 0.65f;
  desc->shore_range = 2.0f;
  desc->shore_wave_length = 1.4f;
  desc->shore_wave_speed = 0.35f;
  desc->shore_foam_strength = 0.6f;
  desc->noise_scale = 8.0f;
  desc->noise_speed = 0.12f;
  desc->distortion_strength = 0.08f;
  desc->foam_speed = 0.12f;
  desc->foam_cutoff = 0.45f;
  desc->caustics_scale = 4.0f;
  desc->caustics_strength = 0.3f;
  desc->caustics_speed = 0.2f;
  desc->caustics_depth = 2.0f;
}

LDK_API bool ldk_renderer_water_desc_is_valid(const LDKRendererWaterDesc *desc)
{
  if (!desc || !isfinite(desc->water_level) ||
      !isfinite(desc->depth_color_distance) ||
      desc->depth_color_distance < 0.01f ||
      !isfinite(desc->edge_fade_distance) || desc->edge_fade_distance < 0.0f ||
      !isfinite(desc->detail_scale) || desc->detail_scale < 0.01f ||
      !isfinite(desc->detail_strength) || desc->detail_strength < 0.0f ||
      !isfinite(desc->detail_speed) || !isfinite(desc->specular) ||
      desc->specular < 0.0f || !isfinite(desc->shininess) ||
      desc->shininess < 1.0f || !isfinite(desc->foam_width) ||
      desc->foam_width < 0.0f || !isfinite(desc->foam_strength) ||
      desc->foam_strength < 0.0f || desc->foam_strength > 1.0f ||
      !isfinite(desc->foam_scale) || desc->foam_scale < 0.01f ||
      !isfinite(desc->shore_range) || desc->shore_range < 0.0f ||
      !isfinite(desc->shore_wave_length) || desc->shore_wave_length < 0.01f ||
      !isfinite(desc->shore_wave_speed) ||
      !isfinite(desc->shore_foam_strength) ||
      desc->shore_foam_strength < 0.0f || desc->shore_foam_strength > 1.0f ||
      !isfinite(desc->noise_scale) || desc->noise_scale < 0.01f ||
      !isfinite(desc->noise_speed) || !isfinite(desc->distortion_strength) ||
      desc->distortion_strength < 0.0f || desc->distortion_strength > 1.0f ||
      !isfinite(desc->foam_speed) || !isfinite(desc->foam_cutoff) ||
      desc->foam_cutoff < 0.0f || desc->foam_cutoff > 1.0f ||
      !isfinite(desc->surface_foam_strength) ||
      desc->surface_foam_strength < 0.0f ||
      desc->surface_foam_strength > 1.0f || !isfinite(desc->caustics_scale) ||
      desc->caustics_scale < 0.01f || !isfinite(desc->caustics_strength) ||
      desc->caustics_strength < 0.0f || !isfinite(desc->caustics_speed) ||
      !isfinite(desc->caustics_depth) || desc->caustics_depth < 0.01f ||
      !isfinite(desc->time_seconds) || desc->time_seconds < 0.0f)
  {
    return false;
  }
  float amplitude = 0.0f;
  for (u32 i = 0u; i < LDK_RENDERER_WATER_WAVE_COUNT; ++i)
  {
    const LDKRendererWaterWave *wave = &desc->waves[i];
    if (!isfinite(wave->height) || wave->height < 0.0f ||
        !isfinite(wave->length) || wave->length < 0.01f ||
        !isfinite(wave->speed) || !isfinite(wave->direction_degrees))
    {
      return false;
    }
    amplitude += wave->height;
  }
  return isfinite(desc->water_level + amplitude) &&
         isfinite(desc->water_level - amplitude);
}

LDK_API bool ldk_renderer_submit_water(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKResourceMesh mesh,
    const LDKRendererWaterDesc *desc)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      (view_id != LDK_RENDERER_VIEW_ALL &&
          !s_renderer_view_find(renderer, view_id)) ||
      !ldk_renderer_water_desc_is_valid(desc) ||
      !ldk_renderer_mesh_is_valid(renderer, mesh))
  {
    return false;
  }
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    if (desc->textures[i].id != LDK_RHI_INVALID_RESOURCE &&
        !ldk_renderer_texture_is_valid(renderer, desc->textures[i]))
    {
      return false;
    }
  }
  if (renderer->submitted_water_count == renderer->submitted_water_capacity)
  {
    u64 capacity = renderer->submitted_water_capacity
                       ? (u64)renderer->submitted_water_capacity * 2u
                       : 16u;
    if (capacity > UINT32_MAX ||
        capacity > SIZE_MAX / sizeof(*renderer->submitted_water))
    {
      return false;
    }
    LDKRendererWaterSubmit *submits =
        LDK_RENDERER_REALLOC(renderer->submitted_water,
            (size_t)capacity * sizeof(*renderer->submitted_water));
    if (!submits)
    {
      return false;
    }
    renderer->submitted_water = submits;
    renderer->submitted_water_capacity = (u32)capacity;
  }
  LDKRendererWaterSubmit *submit =
      &renderer->submitted_water[renderer->submitted_water_count++];
  submit->mesh = mesh;
  submit->view_id = view_id;
  submit->desc = *desc;
  return true;
}

static void s_renderer_water_pass_terminate(LDKRendererWaterPass *pass)
{
  if (pass->rhi)
  {
    ldk_rhi_sampler_destroy(pass->rhi, pass->depth_sampler);
    ldk_rhi_sampler_destroy(pass->rhi, pass->texture_sampler);
    ldk_rhi_texture_destroy(pass->rhi, pass->fallback_texture);
    ldk_rhi_buffer_destroy(pass->rhi, pass->params_buffer);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->ldr_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->hdr_pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
  }
  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_water_pass_initialize(
    LDKRendererWaterPass *pass, LDKRHIContext *rhi)
{
  memset(pass, 0, sizeof(*pass));
  pass->rhi = rhi;
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_WATER_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_WATER_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);

  LDKRHIBindingsLayoutDesc layout = {0};
  ldk_rhi_bindings_layout_desc_defaults(&layout);
  layout.entry_count = 2u + LDK_RENDERER_WATER_TEXTURE_COUNT;
  layout.entries[0].slot = 0u;
  layout.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  layout.entries[1].slot = 1u;
  layout.entries[1].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  layout.entries[1].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    layout.entries[2u + i].slot = s_water_texture_slots[i];
    layout.entries[2u + i].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
    layout.entries[2u + i].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  }
  pass->bindings_layout = ldk_rhi_bindings_layout_create(rhi, &layout);

  LDKRHIBufferDesc buffer = {0};
  ldk_rhi_buffer_desc_defaults(&buffer);
  buffer.size = sizeof(LDKRendererWaterParams);
  buffer.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  buffer.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->params_buffer = ldk_rhi_buffer_create(rhi, &buffer);

  LDKRHISamplerDesc sampler = {0};
  ldk_rhi_sampler_desc_defaults(&sampler);
  sampler.min_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mag_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mip_filter = LDK_RHI_FILTER_NONE;
  sampler.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  pass->depth_sampler = ldk_rhi_sampler_create(rhi, &sampler);
  sampler.min_filter = LDK_RHI_FILTER_LINEAR;
  sampler.mag_filter = LDK_RHI_FILTER_LINEAR;
  sampler.mip_filter = LDK_RHI_FILTER_LINEAR;
  sampler.wrap_u = LDK_RHI_WRAP_REPEAT;
  sampler.wrap_v = LDK_RHI_WRAP_REPEAT;
  pass->texture_sampler = ldk_rhi_sampler_create(rhi, &sampler);

  const u8 neutral[4] = {128u, 128u, 255u, 255u};
  LDKRHITextureDesc texture = {0};
  ldk_rhi_texture_desc_defaults(&texture);
  texture.width = 1u;
  texture.height = 1u;
  texture.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  texture.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture.initial_data = neutral;
  texture.initial_data_size = sizeof(neutral);
  pass->fallback_texture = ldk_rhi_texture_create(rhi, &texture);

  if (!pass->vertex_shader_module || !pass->fragment_shader_module ||
      !pass->bindings_layout || !pass->params_buffer || !pass->depth_sampler ||
      !pass->texture_sampler || !pass->fallback_texture)
  {
    s_renderer_water_pass_terminate(pass);
    return false;
  }

  LDKRHIPipelineDesc pipeline = {0};
  ldk_rhi_pipeline_desc_defaults(&pipeline);
  pipeline.vertex_shader_module = pass->vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pipeline.bindings_layout = pass->bindings_layout;
  pipeline.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  pipeline.depth_state.test_enabled = false;
  pipeline.depth_state.write_enabled = false;
  pipeline.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pipeline.blend_state.enabled = true;
  pipeline.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_ONE;
  pipeline.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pipeline.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  pipeline.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pipeline.vertex_layout.stride = sizeof(LDKMeshVertex);
  pipeline.vertex_layout.attribute_count = 1u;
  pipeline.vertex_layout.attributes[0].location = 0u;
  pipeline.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  pipeline.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKMeshVertex, position);
  pipeline.color_attachment_count = 1u;
  pipeline.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  pass->ldr_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  pipeline.color_formats[0] = LDK_RHI_FORMAT_RGBA16_FLOAT;
  pass->hdr_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->ldr_pipeline || !pass->hdr_pipeline)
  {
    s_renderer_water_pass_terminate(pass);
    return false;
  }
  pass->is_initialized = true;
  return true;
}

static void s_renderer_water_params(LDKRendererWaterParams *params,
    const LDKRendererView *view, const LDKRendererWaterDesc *desc,
    const LDKRendererViewLighting *lighting)
{
  params->camera_time[3] = desc->time_seconds;
  params->viewport[0] = 1.0f / (float)view->width;
  params->viewport[1] = 1.0f / (float)view->height;
  params->viewport[2] = desc->water_level;
  params->viewport[3] = desc->depth_color_distance;
  params->shallow_color = ldk_renderer_color_from_rgba32(desc->shallow_color);
  params->deep_color = ldk_renderer_color_from_rgba32(desc->deep_color);
  params->foam_color = ldk_renderer_color_from_rgba32(desc->foam_color);
  for (u32 i = 0u; i < LDK_RENDERER_WATER_WAVE_COUNT; ++i)
  {
    const LDKRendererWaterWave *wave = &desc->waves[i];
    float angle = wave->direction_degrees * 0.017453292519943295f;
    params->waves[i][0] = cosf(angle);
    params->waves[i][1] = sinf(angle);
    params->waves[i][2] = wave->height;
    params->waves[i][3] = wave->length;
    params->speeds[i] = wave->speed;
  }
  params->speeds[3] = desc->detail_speed;
  params->surface[0] = desc->detail_scale;
  params->surface[1] = desc->detail_strength;
  params->surface[2] = desc->specular;
  params->surface[3] = desc->shininess;
  params->foam[0] = desc->foam_width;
  params->foam[1] = desc->foam_strength;
  params->foam[2] = desc->foam_scale;
  params->foam[3] = desc->edge_fade_distance;
  params->shore[0] = desc->shore_range;
  params->shore[1] = desc->shore_wave_length;
  params->shore[2] = desc->shore_wave_speed;
  params->shore[3] = desc->shore_foam_strength;
  params->noise[0] = desc->noise_scale;
  params->noise[1] = desc->noise_speed;
  params->noise[2] = desc->distortion_strength;
  params->noise[3] = desc->foam_speed;
  params->foam_detail[0] = desc->foam_cutoff;
  params->foam_detail[1] = desc->surface_foam_strength;
  params->caustics[0] = desc->caustics_scale;
  params->caustics[1] = desc->caustics_strength;
  params->caustics[2] = desc->caustics_speed;
  params->caustics[3] = desc->caustics_depth;
  memcpy(params->ambient, lighting->params.ambient, sizeof(params->ambient));
  for (i32 i = 0; i < lighting->params.count[0]; ++i)
  {
    const LDKRendererLightParams *light = &lighting->params.lights[i];
    if ((i32)light->position_type[3] == LDK_RENDERER_LIGHT_DIRECTIONAL)
    {
      memcpy(params->sun_direction, light->direction_range,
          sizeof(params->sun_direction));
      memcpy(
          params->sun_color, light->color_intensity, sizeof(params->sun_color));
      return;
    }
  }
}

static bool s_renderer_water_pass_draw(LDKRenderer *renderer,
    const LDKRendererView *view, const LDKRendererViewLighting *lighting)
{
  bool has_water = false;
  for (u32 i = 0u; i < renderer->submitted_water_count; ++i)
  {
    LDKRendererViewId id = renderer->submitted_water[i].view_id;
    if (id == LDK_RENDERER_VIEW_ALL || id == view->id)
    {
      has_water = true;
      break;
    }
  }
  if (!has_water)
  {
    return true;
  }

  LDKRendererWaterPass *pass = &renderer->water_pass;
  if (!pass->is_initialized &&
      !s_renderer_water_pass_initialize(pass, renderer->rhi))
  {
    return false;
  }
  LDKRendererWaterParams params = {0};
  params.view_projection = mat4_mul(view->projection, view->view);
  bool invertible = false;
  params.inverse_view_projection =
      mat4_inverse_full(params.view_projection, &invertible);
  if (!invertible)
  {
    return false;
  }
  Vec3 camera = mat4_mul_point(
      mat4_inverse_affine(view->view), vec3_make(0.0f, 0.0f, 0.0f));
  params.camera_time[0] = camera.x;
  params.camera_time[1] = camera.y;
  params.camera_time[2] = camera.z;

  LDKRHIBindingsDesc bindings_desc = {0};
  ldk_rhi_bindings_desc_defaults(&bindings_desc);
  bindings_desc.layout = pass->bindings_layout;
  bindings_desc.binding_count = 2u + LDK_RENDERER_WATER_TEXTURE_COUNT;
  bindings_desc.bindings[0].slot = 0u;
  bindings_desc.bindings[0].buffer = pass->params_buffer;
  bindings_desc.bindings[0].buffer_size = sizeof(params);
  bindings_desc.bindings[1].slot = 1u;
  bindings_desc.bindings[1].texture = view->target.depth_texture;
  bindings_desc.bindings[1].sampler = pass->depth_sampler;
  for (u32 i = 0u; i < LDK_RENDERER_WATER_TEXTURE_COUNT; ++i)
  {
    bindings_desc.bindings[2u + i].slot = s_water_texture_slots[i];
    bindings_desc.bindings[2u + i].sampler = pass->texture_sampler;
  }
  LDKRHIBindings bindings = LDK_RHI_INVALID_RESOURCE;
  LDKRHITexture bound_textures[LDK_RENDERER_WATER_TEXTURE_COUNT] = {0};

  /* Sample opaque depth without attaching it to this pass. The shader does
   * its own opaque occlusion test; later translucent draws reuse this depth. */
  LDKRHIPassDesc pass_desc = {0};
  ldk_rhi_pass_desc_defaults(&pass_desc);
  pass_desc.color_attachment_count = 1u;
  pass_desc.color_attachments[0].texture = view->target.color_texture;
  pass_desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_LOAD;
  pass_desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
  pass_desc.has_viewport = true;
  pass_desc.viewport.width = (float)view->width;
  pass_desc.viewport.height = (float)view->height;
  pass_desc.viewport.max_depth = 1.0f;
  ldk_rhi_pass_begin(renderer->rhi, &pass_desc);
  ldk_rhi_pipeline_bind(
      renderer->rhi, view->target.color_format == LDK_RHI_FORMAT_RGBA16_FLOAT
                         ? pass->hdr_pipeline
                         : pass->ldr_pipeline);

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  bool success = true;
  for (u32 i = 0u; i < renderer->submitted_water_count; ++i)
  {
    const LDKRendererWaterSubmit *submit = &renderer->submitted_water[i];
    if (submit->view_id != LDK_RENDERER_VIEW_ALL && submit->view_id != view->id)
    {
      continue;
    }
    LDKRendererMeshResource *mesh =
        s_renderer_mesh_get_resource(renderer, submit->mesh);
    if (!mesh)
    {
      continue;
    }
    s_renderer_water_params(&params, view, &submit->desc, lighting);
    bool textures_changed = bindings == LDK_RHI_INVALID_RESOURCE;
    for (u32 t = 0u; t < LDK_RENDERER_WATER_TEXTURE_COUNT; ++t)
    {
      LDKRendererTextureResource *texture =
          s_renderer_texture_get_resource(renderer, submit->desc.textures[t]);
      LDKRHITexture handle =
          texture ? texture->texture : pass->fallback_texture;
      params.texture_flags[t] = texture ? 1.0f : 0.0f;
      bindings_desc.bindings[2u + t].texture = handle;
      textures_changed = textures_changed || bound_textures[t] != handle;
    }
    if (textures_changed)
    {
      LDKRHIBindings next =
          ldk_rhi_bindings_create(renderer->rhi, &bindings_desc);
      if (!next)
      {
        success = false;
        break;
      }
      ldk_rhi_bindings_destroy(renderer->rhi, bindings);
      bindings = next;
      for (u32 t = 0u; t < LDK_RENDERER_WATER_TEXTURE_COUNT; ++t)
      {
        bound_textures[t] = bindings_desc.bindings[2u + t].texture;
      }
      ldk_rhi_bindings_bind(renderer->rhi, bindings);
    }
    if (!ldk_rhi_buffer_update(
            renderer->rhi, pass->params_buffer, 0u, sizeof(params), &params))
    {
      success = false;
      break;
    }
    ldk_rhi_vertex_buffer_bind(renderer->rhi, mesh->vertex_buffer, 0u);
    ldk_rhi_index_buffer_bind(
        renderer->rhi, mesh->index_buffer, 0u, LDK_RHI_INDEX_TYPE_UINT32);
    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = mesh->index_count;
    ldk_rhi_draw_indexed(renderer->rhi, &draw);
    if (stats)
    {
      stats->draw_call_count += 1u;
      stats->batch_count += 1u;
      if (stats->max_batch_size < 1u)
      {
        stats->max_batch_size = 1u;
      }
    }
  }
  ldk_rhi_pass_end(renderer->rhi);
  ldk_rhi_bindings_destroy(renderer->rhi, bindings);
  return success;
}

