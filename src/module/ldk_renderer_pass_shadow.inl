// One map is reused sequentially: shadow(view), meshes(view), next view.
static void s_renderer_shadow_pass_terminate(LDKRendererShadowPass *pass)
{
  if (pass->rhi)
  {
    for (u32 i = 0; i < pass->cutout_bindings_cache_count; ++i)
    {
      ldk_rhi_bindings_destroy(
          pass->rhi, pass->cutout_bindings_cache[i].bindings);
    }

    ldk_rhi_bindings_destroy(pass->rhi, pass->vegetation_bindings);
    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->cutout_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->cutout_instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->vegetation_instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->cutout_vertex_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->vegetation_vertex_shader_module);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_uv_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->material_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_sampler_destroy(pass->rhi, pass->sampler);
    ldk_rhi_texture_destroy(pass->rhi, pass->depth_texture);
  }
  LDK_RENDERER_FREE(pass->cutout_bindings_cache);
  memset(pass, 0, sizeof(*pass));
}

static void s_renderer_shadow_pass_disable_instancing(
    LDKRendererShadowPass *pass)
{
  ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->cutout_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->instanced_pipeline);
  ldk_rhi_shader_module_destroy(
      pass->rhi, pass->cutout_instanced_vertex_shader_module);
  ldk_rhi_shader_module_destroy(
      pass->rhi, pass->vegetation_instanced_vertex_shader_module);
  ldk_rhi_shader_module_destroy(pass->rhi, pass->instanced_vertex_shader_module);
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_uv_buffer);

  pass->cutout_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->vegetation_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->cutout_instanced_vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
  pass->vegetation_instanced_vertex_shader_module =
      LDK_RHI_INVALID_RESOURCE;
  pass->instanced_vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
  pass->instance_buffer = LDK_RHI_INVALID_RESOURCE;
  pass->instance_uv_buffer = LDK_RHI_INVALID_RESOURCE;
  pass->instance_capacity = 0;
}

static void s_renderer_shadow_pass_instance_layout(
    LDKRHIPipelineDesc *desc)
{
  desc->vertex_buffer_layout_count = 3;
  desc->vertex_buffer_layouts[0] = desc->vertex_layout;
  desc->vertex_buffer_layouts[0].input_rate =
      LDK_RHI_VERTEX_INPUT_RATE_PER_VERTEX;

  LDKRHIVertexBufferLayoutDesc *instance = &desc->vertex_buffer_layouts[1];
  memset(instance, 0, sizeof(*instance));
  instance->stride = sizeof(Mat4);
  instance->attribute_count = 4;
  instance->input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE;
  for (u32 i = 0; i < 4; ++i)
  {
    instance->attributes[i].location = 4u + i;
    instance->attributes[i].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
    instance->attributes[i].offset = i * 4u * (u32)sizeof(float);
  }

  LDKRHIVertexBufferLayoutDesc *uv = &desc->vertex_buffer_layouts[2];
  memset(uv, 0, sizeof(*uv));
  uv->stride = sizeof(Vec4);
  uv->attribute_count = 1;
  uv->input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE;
  uv->attributes[0].location = 10u;
  uv->attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
}

static bool s_renderer_shadow_pass_ensure_instance_capacity(
    LDKRendererShadowPass *pass, u32 instance_count)
{
  if (instance_count <= pass->instance_capacity)
  {
    return true;
  }

  u32 capacity = pass->instance_capacity == 0 ? 256u : pass->instance_capacity;
  while (capacity < instance_count)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = instance_count;
      break;
    }
    capacity *= 2u;
  }

  if (capacity < instance_count || (capacity > UINT32_MAX / sizeof(Mat4) ||
          capacity > UINT32_MAX / sizeof(Vec4)))
  {
    return false;
  }

  size_t cpu_size = (size_t)capacity * sizeof(Mat4);
  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = (u32)cpu_size;
  desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  desc.stats_class = LDK_RHI_BUFFER_STATS_INSTANCE;

  LDKRHIBuffer buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.size = capacity * (u32)sizeof(Vec4);
  LDKRHIBuffer uv_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (uv_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_buffer_destroy(pass->rhi, buffer);
    return false;
  }
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_uv_buffer);
  pass->instance_buffer = buffer;
  pass->instance_uv_buffer = uv_buffer;
  pass->instance_capacity = capacity;
  return true;
}

static bool s_renderer_shadow_pass_grow_cutout_bindings_cache(
    LDKRendererShadowPass *pass)
{
  u32 new_capacity = pass->cutout_bindings_cache_capacity == 0
      ? 16
      : pass->cutout_bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererBindingsCacheEntry);
  LDKRendererBindingsCacheEntry *new_cache =
      pass->cutout_bindings_cache == NULL
      ? (LDKRendererBindingsCacheEntry *)LDK_RENDERER_ALLOC(new_size)
      : (LDKRendererBindingsCacheEntry *)LDK_RENDERER_REALLOC(
            pass->cutout_bindings_cache, new_size);

  if (!new_cache)
  {
    return false;
  }

  pass->cutout_bindings_cache = new_cache;
  pass->cutout_bindings_cache_capacity = new_capacity;
  return true;
}

static LDKRHIBindings s_renderer_shadow_pass_create_cutout_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture, LDKRHISampler sampler)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 4;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_size = sizeof(LDKRendererShadowCameraParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_size = sizeof(Mat4) + sizeof(Vec4);
  desc.bindings[2].slot = 2;
  desc.bindings[2].buffer = pass->material_buffer;
  desc.bindings[2].buffer_size = sizeof(LDKRendererShadowMaterialParams);
  desc.bindings[3].slot = 3;
  desc.bindings[3].texture = texture;
  desc.bindings[3].sampler = sampler;
  return ldk_rhi_bindings_create(pass->rhi, &desc);
}

static LDKRHIBindings s_renderer_shadow_pass_get_cutout_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture, LDKRHISampler sampler)
{
  for (u32 i = 0; i < pass->cutout_bindings_cache_count; ++i)
  {
    LDKRendererBindingsCacheEntry *entry = &pass->cutout_bindings_cache[i];
    if (entry->texture == texture && entry->sampler == sampler)
    {
      return entry->bindings;
    }
  }

  if (pass->cutout_bindings_cache_count ==
          pass->cutout_bindings_cache_capacity &&
      !s_renderer_shadow_pass_grow_cutout_bindings_cache(pass))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIBindings bindings =
      s_renderer_shadow_pass_create_cutout_bindings(pass, texture, sampler);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererBindingsCacheEntry *entry =
      &pass->cutout_bindings_cache[pass->cutout_bindings_cache_count++];
  entry->texture = texture;
  entry->sampler = sampler;
  entry->bindings = bindings;
  return bindings;
}

static void s_renderer_shadow_pass_remove_texture_bindings(
    LDKRendererShadowPass *pass, LDKRHITexture texture)
{
  if (!pass || !pass->rhi || texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < pass->cutout_bindings_cache_count)
  {
    LDKRendererBindingsCacheEntry *entry = &pass->cutout_bindings_cache[index];
    if (entry->texture != texture)
    {
      ++index;
      continue;
    }

    ldk_rhi_bindings_destroy(pass->rhi, entry->bindings);
    --pass->cutout_bindings_cache_count;
    if (index != pass->cutout_bindings_cache_count)
    {
      *entry = pass->cutout_bindings_cache[pass->cutout_bindings_cache_count];
    }
    memset(&pass->cutout_bindings_cache[pass->cutout_bindings_cache_count], 0,
        sizeof(*entry));
  }
}

static bool s_renderer_shadow_pass_initialize(LDKRendererShadowPass *pass,
    LDKRHIContext *rhi, u32 resolution, float distance)
{
  memset(pass, 0, sizeof(*pass));
  if (!s_renderer_shadow_settings_valid(resolution, distance))
  {
    return false;
  }
  pass->resolution = resolution;
  pass->distance = distance;
  pass->rhi = rhi;

  LDKRHITextureDesc texture;
  ldk_rhi_texture_desc_defaults(&texture);
  texture.width = pass->resolution;
  texture.height = pass->resolution;
  texture.format = LDK_RHI_FORMAT_D32_FLOAT;
  texture.usage =
      LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL | LDK_RHI_TEXTURE_USAGE_SAMPLED;
  pass->depth_texture = ldk_rhi_texture_create(rhi, &texture);

  LDKRHISamplerDesc sampler;
  ldk_rhi_sampler_desc_defaults(&sampler);
  sampler.min_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mag_filter = LDK_RHI_FILTER_NEAREST;
  sampler.mip_filter = LDK_RHI_FILTER_NEAREST;
  sampler.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  sampler.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  pass->sampler = ldk_rhi_sampler_create(rhi, &sampler);

  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  pass->cutout_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS_CUTOUT, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->cutout_fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS_CUTOUT, LDK_RHI_SHADER_STAGE_FRAGMENT);
  pass->vegetation_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      rhi, LDK_SHADER_SHADOW_PASS_VEGETATION,
      LDK_RHI_SHADER_STAGE_VERTEX);

  if (rhi->functions.draw_indexed_instanced != NULL)
  {
    pass->instanced_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
        rhi, LDK_SHADER_SHADOW_PASS_INSTANCED, LDK_RHI_SHADER_STAGE_VERTEX);
    pass->cutout_instanced_vertex_shader_module =
        ldk_rhi_create_builtin_shader_module(rhi,
            LDK_SHADER_SHADOW_PASS_CUTOUT_INSTANCED,
            LDK_RHI_SHADER_STAGE_VERTEX);
    pass->vegetation_instanced_vertex_shader_module =
        ldk_rhi_create_builtin_shader_module(rhi,
            LDK_SHADER_SHADOW_PASS_VEGETATION_INSTANCED,
            LDK_RHI_SHADER_STAGE_VERTEX);
    if (pass->instanced_vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
        pass->cutout_instanced_vertex_shader_module ==
            LDK_RHI_INVALID_RESOURCE ||
        pass->vegetation_instanced_vertex_shader_module ==
            LDK_RHI_INVALID_RESOURCE)
    {
      s_renderer_shadow_pass_disable_instancing(pass);
    }
  }

  LDKRHIBufferDesc buffer;
  ldk_rhi_buffer_desc_defaults(&buffer);
  buffer.size = sizeof(LDKRendererShadowCameraParams);
  buffer.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  buffer.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  pass->camera_buffer = ldk_rhi_buffer_create(rhi, &buffer);
  buffer.size = sizeof(Mat4) + sizeof(Vec4);
  pass->object_buffer = ldk_rhi_buffer_create(rhi, &buffer);
  buffer.size = sizeof(LDKRendererShadowMaterialParams);
  pass->material_buffer = ldk_rhi_buffer_create(rhi, &buffer);

  if (!pass->depth_texture || !pass->sampler || !pass->vertex_shader_module ||
      !pass->fragment_shader_module || !pass->cutout_vertex_shader_module ||
      !pass->cutout_fragment_shader_module ||
      !pass->vegetation_vertex_shader_module || !pass->camera_buffer ||
      !pass->object_buffer || !pass->material_buffer)
  {
    goto fail;
  }

  LDKRHIBindingsLayoutDesc layout;
  ldk_rhi_bindings_layout_desc_defaults(&layout);
  layout.entry_count = 5;
  layout.entries[0].slot = 0;
  layout.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[0].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  layout.entries[1].slot = 1;
  layout.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  layout.entries[2].slot = 2;
  layout.entries[2].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  layout.entries[2].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  layout.entries[3].slot = 3;
  layout.entries[3].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  layout.entries[3].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  layout.entries[4].slot = 4;
  layout.entries[4].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  layout.entries[4].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  pass->bindings_layout = ldk_rhi_bindings_layout_create(rhi, &layout);
  if (!pass->bindings_layout)
  {
    goto fail;
  }

  LDKRHIPipelineDesc pipeline;
  ldk_rhi_pipeline_desc_defaults(&pipeline);
  pipeline.vertex_shader_module = pass->vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pipeline.bindings_layout = pass->bindings_layout;
  pipeline.vertex_layout.stride = sizeof(LDKMeshVertex);
  pipeline.vertex_layout.attribute_count = 3;
  pipeline.vertex_layout.attributes[0].location = 0;
  pipeline.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  pipeline.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKMeshVertex, position);
  pipeline.vertex_layout.attributes[1].location = 2;
  pipeline.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  pipeline.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKMeshVertex, uv);
  pipeline.vertex_layout.attributes[2].location = 8;
  pipeline.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
  pipeline.vertex_layout.attributes[2].offset =
      (u32)offsetof(LDKMeshVertex, tangent);
  pipeline.topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  pipeline.color_attachment_count = 0;
  pipeline.depth_format = LDK_RHI_FORMAT_D32_FLOAT;
  pipeline.depth_state.test_enabled = true;
  pipeline.depth_state.write_enabled = true;
  pipeline.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
  pipeline.blend_state.enabled = false;
  // Two-sided casting also handles thin meshes and mirrored transforms.
  pipeline.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pipeline.raster_state.scissor_enabled = false;
  // Use the rasterized triangle's depth slope, including grazing surfaces.
  pipeline.raster_state.depth_bias_enabled = true;
  pipeline.raster_state.depth_bias_slope_factor = 1.0f;
  pipeline.raster_state.depth_bias_constant_factor = 2.0f;
  pass->pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->pipeline)
  {
    goto fail;
  }

  pipeline.vertex_shader_module = pass->vegetation_vertex_shader_module;
  pipeline.fragment_shader_module = pass->fragment_shader_module;
  pass->vegetation_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->vegetation_pipeline)
  {
    goto fail;
  }

  pipeline.vertex_shader_module = pass->cutout_vertex_shader_module;
  pipeline.fragment_shader_module = pass->cutout_fragment_shader_module;
  pass->cutout_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);
  if (!pass->cutout_pipeline)
  {
    goto fail;
  }

  if (pass->instanced_vertex_shader_module != LDK_RHI_INVALID_RESOURCE &&
      pass->cutout_instanced_vertex_shader_module !=
          LDK_RHI_INVALID_RESOURCE &&
      pass->vegetation_instanced_vertex_shader_module !=
          LDK_RHI_INVALID_RESOURCE)
  {
    pipeline.vertex_shader_module = pass->instanced_vertex_shader_module;
    pipeline.fragment_shader_module = pass->fragment_shader_module;
    s_renderer_shadow_pass_instance_layout(&pipeline);
    pass->instanced_pipeline = ldk_rhi_pipeline_create(rhi, &pipeline);

    pipeline.vertex_shader_module =
        pass->vegetation_instanced_vertex_shader_module;
    pipeline.fragment_shader_module = pass->fragment_shader_module;
    s_renderer_shadow_pass_instance_layout(&pipeline);
    pass->vegetation_instanced_pipeline =
        ldk_rhi_pipeline_create(rhi, &pipeline);

    pipeline.vertex_shader_module =
        pass->cutout_instanced_vertex_shader_module;
    pipeline.fragment_shader_module = pass->cutout_fragment_shader_module;
    s_renderer_shadow_pass_instance_layout(&pipeline);
    pass->cutout_instanced_pipeline =
        ldk_rhi_pipeline_create(rhi, &pipeline);

    if (pass->instanced_pipeline == LDK_RHI_INVALID_RESOURCE ||
        pass->vegetation_instanced_pipeline == LDK_RHI_INVALID_RESOURCE ||
        pass->cutout_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
    {
      s_renderer_shadow_pass_disable_instancing(pass);
    }
  }

  LDKRHIBindingsDesc bindings;
  ldk_rhi_bindings_desc_defaults(&bindings);
  bindings.layout = pass->bindings_layout;
  bindings.binding_count = 3;
  bindings.bindings[0].slot = 0;
  bindings.bindings[0].buffer = pass->camera_buffer;
  bindings.bindings[0].buffer_size = sizeof(LDKRendererShadowCameraParams);
  bindings.bindings[1].slot = 1;
  bindings.bindings[1].buffer = pass->object_buffer;
  bindings.bindings[1].buffer_size = sizeof(Mat4);
  bindings.bindings[2].slot = 2;
  bindings.bindings[2].buffer = pass->material_buffer;
  bindings.bindings[2].buffer_size = sizeof(LDKRendererShadowMaterialParams);
  pass->bindings = ldk_rhi_bindings_create(rhi, &bindings);
  if (!pass->bindings)
  {
    goto fail;
  }
  return true;

fail:
  s_renderer_shadow_pass_terminate(pass);
  return false;
}

static bool s_renderer_shadow_camera(const LDKRendererShadowPass *pass,
    const LDKRendererView *view, Vec3 direction,
    LDKRendererLightingParams *params)
{
  const float receiver_constant_bias_texels = 0.5f;
  const float receiver_slope_bias_texels = 5.0f;

  bool invertible = false;
  Mat4 inverse =
      mat4_inverse_full(mat4_mul(view->projection, view->view), &invertible);
  if (!invertible)
  {
    return false;
  }

  Vec3 corners[8];
  Vec3 center = vec3_make(0, 0, 0);
  for (u32 i = 0; i < 4; ++i)
  {
    float x = (i & 1) ? 1.0f : -1.0f;
    float y = (i & 2) ? 1.0f : -1.0f;
    Vec3 near_corner = mat4_mul_point(inverse, vec3_make(x, y, -1));
    Vec3 far_corner = mat4_mul_point(inverse, vec3_make(x, y, 1));
    float near_depth = -mat4_mul_point(view->view, near_corner).z;
    float far_depth = -mat4_mul_point(view->view, far_corner).z;

    if (!isfinite(near_depth) || !isfinite(far_depth) ||
        near_depth >= pass->distance || far_depth <= near_depth)
    {
      return false;
    }

    float t =
        fminf(1.0f, (pass->distance - near_depth) / (far_depth - near_depth));
    corners[i] = near_corner;
    corners[i + 4] =
        vec3_add(near_corner, vec3_mul(vec3_sub(far_corner, near_corner), t));
    center = vec3_add(center, vec3_add(corners[i], corners[i + 4]));
  }

  center = vec3_mul(center, 0.125f);

  float radius = 0.0f;
  for (u32 i = 0; i < 8; ++i)
  {
    Vec3 delta = vec3_sub(corners[i], center);
    radius = fmaxf(radius, sqrtf(vec3_dot(delta, delta)));
  }

  if (!isfinite(radius) || radius < 1e-4f)
  {
    return false;
  }

  // Rounded sphere fit keeps the map extent stable under camera rotation.
  radius = ceilf(radius * 16.0f) / 16.0f;
  radius *= (float)pass->resolution / (float)(pass->resolution - 2u);

  float texel = 2.0f * radius / (float)pass->resolution;

  Vec3 up =
      fabsf(direction.y) > 0.99f ? vec3_make(0, 0, 1) : vec3_make(0, 1, 0);

  Mat4 light_view = mat4_look_at_rh(vec3_make(0, 0, 0), direction, up);
  Vec3 light_center = mat4_mul_point(light_view, center);

  light_center.x = floorf(light_center.x / texel + 0.5f) * texel;
  light_center.y = floorf(light_center.y / texel + 0.5f) * texel;

  float min_z = light_center.z - radius;
  float max_z =
      light_center.z + radius + LDK_RENDERER_SHADOW_CASTER_MARGIN;

  // Include upstream, off-camera casters. This bounded first pass has no CSM.
  Mat4 projection =
      mat4_orthographic_rh_no(light_center.x - radius,
          light_center.x + radius,
          light_center.y - radius,
          light_center.y + radius,
          -max_z,
          -min_z);

  params->shadow_view_projection = mat4_mul(projection, light_view);

  float depth_range = max_z - min_z;

  // Bias is expressed in shadow-map texels and converted to depth space.
  params->shadow_params[0] =
      receiver_constant_bias_texels * texel / depth_range;
  params->shadow_params[1] =
      receiver_slope_bias_texels * texel / depth_range;

  return true;
}

static LDKRendererMeshSubmit *s_renderer_shadow_pass_run_submit(
    LDKRenderer *renderer, LDKRendererMeshSubmit *first_submit,
    const u64 *sort_items, u32 index)
{
  if (sort_items == NULL)
  {
    return index == 0 ? first_submit : NULL;
  }

  u32 submit_index = s_renderer_mesh_sort_item_submit_index(sort_items[index]);
  if (submit_index >= renderer->submitted_mesh_count)
  {
    return NULL;
  }

  return &renderer->submitted_meshes[submit_index];
}

static void s_renderer_shadow_pass_bind(LDKRendererShadowPass *pass,
    LDKRHIPipeline pipeline, LDKRHIBindings bindings,
    LDKRHIPipeline *bound_pipeline, LDKRHIBindings *bound_bindings)
{
  if (pipeline != *bound_pipeline)
  {
    ldk_rhi_pipeline_bind(pass->rhi, pipeline);
    *bound_pipeline = pipeline;
    *bound_bindings = LDK_RHI_INVALID_RESOURCE;
  }
  if (bindings != *bound_bindings)
  {
    ldk_rhi_bindings_bind(pass->rhi, bindings);
    *bound_bindings = bindings;
  }
}

static bool s_renderer_mesh_pass_staging_ensure(
    LDKRendererMeshPass *pass, u32 count);

static void s_renderer_shadow_pass_draw_run(LDKRenderer *renderer,
    LDKRendererShadowPass *pass, LDKRendererFrameDomainStats *stats,
    LDKRendererMeshSubmit *first_submit, const u64 *sort_items,
    u32 submit_count, LDKRHIPipeline *bound_pipeline,
    LDKRHIBindings *bound_bindings)
{
  if (submit_count == 0 || first_submit == NULL)
  {
    return;
  }

  LDKRendererInstanceSetResource *instance_set =
      s_renderer_instance_set_get_resource(
          renderer, first_submit->instance_set);
  const Mat4 *worlds = !instance_set && first_submit->instance_count
      ? renderer->submitted_instance_worlds + first_submit->instance_offset
      : NULL;
  const Vec4 *uv_rects = NULL;
  u32 instance_buffer_offset = 0u;
  if (first_submit->instance_set.id != LDK_RHI_INVALID_RESOURCE &&
      instance_set == NULL)
  {
    return;
  }
  if (worlds)
  {
    submit_count = first_submit->instance_count;
  }
  else if (instance_set)
  {
    if (first_submit->instance_count != 0u)
    {
      if (first_submit->instance_offset > instance_set->instance_count ||
          first_submit->instance_count >
              instance_set->instance_count - first_submit->instance_offset)
      {
        return;
      }
      submit_count = first_submit->instance_count;
      instance_buffer_offset = first_submit->instance_offset;
    }
    else
    {
      submit_count = instance_set->instance_count;
    }
  }

  if (!instance_set && !worlds && submit_count > 1u &&
      s_renderer_mesh_pass_staging_ensure(&renderer->mesh_pass, submit_count))
  {
    bool complete = true;
    for (u32 i = 0; i < submit_count; ++i)
    {
      LDKRendererMeshSubmit *submit = s_renderer_shadow_pass_run_submit(
          renderer, first_submit, sort_items, i);
      if (!submit)
      {
        complete = false;
        break;
      }
      renderer->mesh_pass.staging_worlds[i] = submit->world;
      renderer->mesh_pass.staging_uvs[i] = submit->uv_rect;
    }
    if (complete)
    {
      worlds = renderer->mesh_pass.staging_worlds;
      uv_rects = renderer->mesh_pass.staging_uvs;
    }
  }
  if (!instance_set && worlds && !uv_rects &&
      s_renderer_mesh_pass_staging_ensure(&renderer->mesh_pass, submit_count))
  {
    for (u32 i = 0; i < submit_count; ++i)
      renderer->mesh_pass.staging_uvs[i] = first_submit->uv_rect;
    uv_rects = renderer->mesh_pass.staging_uvs;
  }
  if (instance_set &&
      s_renderer_mesh_pass_staging_ensure(&renderer->mesh_pass, submit_count))
  {
    for (u32 i = 0; i < submit_count; ++i)
      renderer->mesh_pass.staging_uvs[i] = first_submit->uv_rect;
    uv_rects = renderer->mesh_pass.staging_uvs;
  }

  LDKRendererMeshResource *mesh =
      s_renderer_mesh_get_resource(renderer, first_submit->mesh);
  LDKRendererMaterialResource *material =
      s_renderer_material_get_resource(renderer, first_submit->material);
  if (!mesh || !mesh->index_count || !material)
  {
    return;
  }

  bool cutout = s_renderer_shadow_material_uses_alpha_test(material);
  bool vegetation = material->selection ==
          LDK_RENDERER_MATERIAL_SELECTION_VEGETATION ||
      material->selection == LDK_RENDERER_MATERIAL_SELECTION_VEGETATION_UNLIT;
  LDKRHIPipeline pipeline = vegetation
      ? pass->vegetation_pipeline
      : (cutout ? pass->cutout_pipeline : pass->pipeline);
  LDKRHIPipeline instanced_pipeline = vegetation
      ? pass->vegetation_instanced_pipeline
      : (cutout ? pass->cutout_instanced_pipeline : pass->instanced_pipeline);
  LDKRHIBindings bindings = pass->bindings;
  LDKRendererShadowMaterialParams material_params = {0};

  if (vegetation && renderer->vegetation_interaction_enabled &&
      pass->vegetation_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    bindings = pass->vegetation_bindings;
  }

  material_params.vegetation[0] = material->desc.vegetation_curvature;
  material_params.vegetation[1] = material->desc.vegetation_wind_strength;
  material_params.vegetation[2] = material->desc.vegetation_wind_speed;
  material_params.vegetation[3] = material->desc.vegetation_wind_direction;
  material_params.uv_transform[0] = material->uv_scale.x;
  material_params.uv_transform[1] = material->uv_scale.y;
  material_params.uv_transform[2] = material->uv_offset.x;
  material_params.uv_transform[3] = material->uv_offset.y;
  if (vegetation && renderer->vegetation_interaction_enabled)
  {
    material_params.vegetation_interaction[0] =
        renderer->vegetation_interaction_origin.x;
    material_params.vegetation_interaction[1] =
        renderer->vegetation_interaction_origin.y;
    material_params.vegetation_interaction[2] =
        renderer->vegetation_interaction_inv_world_size;
    material_params.vegetation_interaction[3] =
        material->desc.vegetation_interaction_recovery_time;
  }

  if (cutout)
  {
    LDKRendererTextureResource *texture =
        s_renderer_texture_get_resource(renderer, material->desc.texture);
    if (!texture)
    {
      return;
    }

    LDKRHISampler sampler =
        material->texture_sampler != LDK_RHI_INVALID_RESOURCE
        ? material->texture_sampler
        : texture->sampler;
    bindings = s_renderer_shadow_pass_get_cutout_bindings(
        pass, texture->texture, sampler);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      return;
    }

    material_params.color_alpha =
        ldk_renderer_color_from_rgba32(material->desc.color).a;
    material_params.alpha_cutoff = material->desc.alpha_cutoff;
  }
  ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
      sizeof(material_params), &material_params);

  LDKRHIBuffer instance_buffer = LDK_RHI_INVALID_RESOURCE;
  if (instance_set)
  {
    instance_buffer = instance_set->transform_buffer;
  }
  else if (worlds &&
      s_renderer_shadow_pass_ensure_instance_capacity(pass, submit_count) &&
      ldk_rhi_buffer_update(pass->rhi, pass->instance_buffer, 0,
          submit_count * (u32)sizeof(Mat4), worlds))
  {
    instance_buffer = pass->instance_buffer;
  }

  bool valid_uv = uv_rects &&
      s_renderer_shadow_pass_ensure_instance_capacity(pass, submit_count) &&
      ldk_rhi_buffer_update(pass->rhi, pass->instance_uv_buffer, 0,
          submit_count * (u32)sizeof(Vec4), uv_rects);
  if (valid_uv && instance_buffer != LDK_RHI_INVALID_RESOURCE &&
      pass->rhi->functions.draw_indexed_instanced != NULL &&
      instanced_pipeline != LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_shadow_pass_bind(pass, instanced_pipeline, bindings,
        bound_pipeline, bound_bindings);
    ldk_rhi_vertex_buffer_bind_at(pass->rhi, 0, mesh->vertex_buffer, 0);
    ldk_rhi_vertex_buffer_bind_at(pass->rhi, 1, instance_buffer,
        instance_buffer_offset * (u32)sizeof(Mat4));
    ldk_rhi_vertex_buffer_bind_at(
        pass->rhi, 2, pass->instance_uv_buffer, 0);
    ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
        LDK_RHI_INDEX_TYPE_UINT32);

    LDKRHIDrawIndexedInstancedDesc draw = {0};
    draw.first_index = first_submit->first_index;
    draw.index_count = first_submit->index_count;
    draw.instance_count = submit_count;
    draw.vertex_offset = 0;
    draw.first_instance = 0;
    ldk_rhi_draw_indexed_instanced(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->shadow_draw_call_count += 1;
    return;
  }

  if (instance_set)
  {
    return;
  }

  if (pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  s_renderer_shadow_pass_bind(
      pass, pipeline, bindings, bound_pipeline, bound_bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(
      pass->rhi, mesh->index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < submit_count; ++i)
  {
    LDKRendererMeshSubmit *submit = worlds ? first_submit
        : s_renderer_shadow_pass_run_submit(
              renderer, first_submit, sort_items, i);
    if (submit == NULL)
    {
      return;
    }

    struct {
      Mat4 world;
      Vec4 uv_rect;
    } object = {
      worlds ? worlds[i] : submit->world,
      uv_rects ? uv_rects[i] : submit->uv_rect
    };
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);

    LDKRHIDrawIndexedDesc draw = {0};
    draw.first_index = first_submit->first_index;
    draw.index_count = first_submit->index_count;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->shadow_draw_call_count += 1;
  }
}

static void s_renderer_shadow_pass_draw_unsorted(LDKRenderer *renderer,
    LDKRendererShadowPass *pass, const LDKRendererView *view,
    LDKRendererFrameDomainStats *stats, LDKRHIPipeline *bound_pipeline,
    LDKRHIBindings *bound_bindings)
{
  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[i];
    if (!(submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS) ||
        (submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    LDKRendererMaterialResource *material =
        s_renderer_material_get_resource(renderer, submit->material);
    if (s_renderer_mesh_material_excluded_from_queue(material, true))
    {
      continue;
    }

    s_renderer_shadow_pass_draw_run(renderer, pass, stats, submit, NULL, 1,
        bound_pipeline, bound_bindings);
  }
}

static void s_renderer_shadow_pass_draw(LDKRenderer *renderer,
    const LDKRendererView *view, LDKRendererViewLighting *lighting)
{
  LDKRendererShadowPass *pass = &renderer->shadow_pass;
  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  if (!lighting->shadow_light ||
      !s_renderer_shadow_camera(
          pass, view, lighting->shadow_light->direction, &lighting->params))
  {
    lighting->params.count[1] = -1;
    return;
  }

  LDKRendererShadowCameraParams camera = {0};
  camera.view_projection = lighting->params.shadow_view_projection;
  camera.animation[0] = renderer->animation_time_seconds;
  ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0, sizeof(camera),
      &camera);

  LDKRHIPassDesc desc;
  ldk_rhi_pass_desc_defaults(&desc);
  desc.color_attachment_count = 0;
  desc.depth_attachment.valid = true;
  desc.depth_attachment.texture = pass->depth_texture;
  desc.depth_attachment.depth_load_op = LDK_RHI_LOAD_OP_CLEAR;
  desc.depth_attachment.depth_store_op = LDK_RHI_STORE_OP_STORE;
  desc.depth_attachment.clear_depth = 1.0f;
  desc.has_viewport = true;
  desc.viewport.width = (float)pass->resolution;
  desc.viewport.height = (float)pass->resolution;
  desc.viewport.min_depth = 0.0f;
  desc.viewport.max_depth = 1.0f;
  ldk_rhi_pass_begin(pass->rhi, &desc);

  LDKRHIPipeline bound_pipeline = LDK_RHI_INVALID_RESOURCE;
  LDKRHIBindings bound_bindings = LDK_RHI_INVALID_RESOURCE;
  u64 *sort_items = NULL;
  u32 sort_item_count = 0;

  if (!s_renderer_mesh_sort_queue_build(renderer, view,
          LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS, &sort_items,
          &sort_item_count))
  {
    s_renderer_shadow_pass_draw_unsorted(renderer, pass, view, stats,
        &bound_pipeline, &bound_bindings);
    ldk_rhi_pass_end(pass->rhi);
    return;
  }

  u32 i = 0;
  while (i < sort_item_count)
  {
    u32 submit_index = s_renderer_mesh_sort_item_submit_index(sort_items[i]);
    if (submit_index >= renderer->submitted_mesh_count)
    {
      ++i;
      continue;
    }

    LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[submit_index];
    u32 end = i + 1;
    while (end < sort_item_count)
    {
      u32 next_index =
          s_renderer_mesh_sort_item_submit_index(sort_items[end]);
      if (next_index >= renderer->submitted_mesh_count ||
          !s_renderer_mesh_submit_same_batch(
              submit, &renderer->submitted_meshes[next_index]))
      {
        break;
      }
      ++end;
    }

    s_renderer_shadow_pass_draw_run(renderer, pass, stats, submit,
        &sort_items[i], end - i, &bound_pipeline, &bound_bindings);
    i = end;
  }

  ldk_rhi_pass_end(pass->rhi);
}

