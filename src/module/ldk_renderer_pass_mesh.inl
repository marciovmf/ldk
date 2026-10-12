static bool s_renderer_mesh_pass_create_shaders(LDKRendererMeshPass* pass)
{
  pass->vertex_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  if (pass->vertex_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  pass->overlay_fragment_shader_module = ldk_rhi_create_builtin_shader_module(
      pass->rhi, LDK_SHADER_MESH_PASS_UNLIT,
      LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->overlay_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->fragment_shader_module = LDK_RHI_INVALID_RESOURCE;
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  pass->solid_unlit_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_SOLID_UNLIT, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->solid_unlit_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->overlay_fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    pass->overlay_fragment_shader_module = LDK_RHI_INVALID_RESOURCE;
    pass->fragment_shader_module = LDK_RHI_INVALID_RESOURCE;
    pass->vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  pass->textured_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_unlit_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_UNLIT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_unlit_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_cutout_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_CUTOUT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_cutout_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->textured_unlit_cutout_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_MESH_PASS_TEXTURED_UNLIT_CUTOUT,
          LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->textured_unlit_cutout_fragment_shader_module ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->terrain_vertex_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_TERRAIN_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->terrain_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_TERRAIN_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->terrain_vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
      pass->terrain_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  pass->vegetation_vertex_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_VEGETATION_PASS, LDK_RHI_SHADER_STAGE_VERTEX);
  pass->vegetation_fragment_shader_module =
      ldk_rhi_create_builtin_shader_module(pass->rhi,
          LDK_SHADER_VEGETATION_PASS, LDK_RHI_SHADER_STAGE_FRAGMENT);
  if (pass->vegetation_vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
      pass->vegetation_fragment_shader_module == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  if (pass->rhi->functions.draw_indexed_instanced != NULL)
  {
    pass->instanced_vertex_shader_module = ldk_rhi_create_builtin_shader_module(
        pass->rhi, LDK_SHADER_MESH_PASS_INSTANCED,
        LDK_RHI_SHADER_STAGE_VERTEX);
    pass->vegetation_instanced_vertex_shader_module =
        ldk_rhi_create_builtin_shader_module(pass->rhi,
            LDK_SHADER_VEGETATION_PASS_INSTANCED,
            LDK_RHI_SHADER_STAGE_VERTEX);
  }

  return true;
}

static const u32 s_renderer_material_texture_binding_slots[
    LDK_MATERIAL_TEXTURE_SLOT_COUNT] = {3u, 6u, 7u, 9u, 10u, 11u, 12u, 13u};

static bool s_renderer_mesh_pass_create_bindings_layout(
    LDKRendererMeshPass* pass)
{
  LDKRHIBindingsLayoutDesc desc = {0};
  ldk_rhi_bindings_layout_desc_defaults(&desc);
  desc.entry_count = 14;
  desc.entries[0].slot = 0;
  desc.entries[0].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[0].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[1].slot = 1;
  desc.entries[1].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[1].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  desc.entries[2].slot = 2;
  desc.entries[2].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[2].stages =
      LDK_RHI_SHADER_STAGE_VERTEX | LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[3].slot = 3;
  desc.entries[3].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[3].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[4].slot = 4;
  desc.entries[4].type = LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER;
  desc.entries[4].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[5].slot = 5;
  desc.entries[5].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[5].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[6].slot = 6;
  desc.entries[6].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[6].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[7].slot = 7;
  desc.entries[7].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[7].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  desc.entries[8].slot = 8;
  desc.entries[8].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
  desc.entries[8].stages = LDK_RHI_SHADER_STAGE_VERTEX;
  for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
  {
    u32 entry = 9u + i;
    desc.entries[entry].slot =
        s_renderer_material_texture_binding_slots[i + 3u];
    desc.entries[entry].type = LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
    desc.entries[entry].stages = LDK_RHI_SHADER_STAGE_FRAGMENT;
  }

  pass->bindings_layout =
      ldk_rhi_bindings_layout_create(pass->rhi, &desc);
  return pass->bindings_layout != LDK_RHI_INVALID_RESOURCE;
}

static void s_renderer_mesh_pass_instance_layout(
    LDKRHIPipelineDesc* desc)
{
  desc->vertex_buffer_layout_count = 4;
  desc->vertex_buffer_layouts[0] = desc->vertex_layout;
  desc->vertex_buffer_layouts[0].input_rate =
      LDK_RHI_VERTEX_INPUT_RATE_PER_VERTEX;

  LDKRHIVertexBufferLayoutDesc* instance = &desc->vertex_buffer_layouts[1];
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

  LDKRHIVertexBufferLayoutDesc* color = &desc->vertex_buffer_layouts[2];
  memset(color, 0, sizeof(*color));
  color->stride = sizeof(LDKRHIColor);
  color->attribute_count = 1;
  color->input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE;
  color->attributes[0].location = 9u;
  color->attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
  color->attributes[0].offset = 0u;

  LDKRHIVertexBufferLayoutDesc* uv = &desc->vertex_buffer_layouts[3];
  memset(uv, 0, sizeof(*uv));
  uv->stride = sizeof(Vec4);
  uv->attribute_count = 1;
  uv->input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE;
  uv->attributes[0].location = 10u;
  uv->attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
}

static void s_renderer_mesh_pass_disable_instancing(
    LDKRendererMeshPass* pass)
{
  ldk_rhi_pipeline_destroy(
      pass->rhi, pass->vertex_color_unlit_blend_instanced_pipeline);
  ldk_rhi_pipeline_destroy(
      pass->rhi, pass->vertex_color_blend_instanced_pipeline);
  ldk_rhi_pipeline_destroy(
      pass->rhi, pass->textured_unlit_blend_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_blend_instanced_pipeline);
  ldk_rhi_pipeline_destroy(
      pass->rhi, pass->textured_unlit_cutout_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_cutout_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_instanced_pipeline);
  ldk_rhi_pipeline_destroy(
      pass->rhi, pass->vertex_color_unlit_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_instanced_pipeline);
  ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_instanced_pipeline);
  ldk_rhi_shader_module_destroy(pass->rhi, pass->instanced_vertex_shader_module);
  ldk_rhi_shader_module_destroy(
      pass->rhi, pass->vegetation_instanced_vertex_shader_module);

  pass->vertex_color_unlit_blend_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->vertex_color_blend_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_unlit_blend_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_blend_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_unlit_cutout_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_cutout_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_unlit_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->textured_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->vertex_color_unlit_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->vertex_color_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->vegetation_instanced_pipeline = LDK_RHI_INVALID_RESOURCE;
  pass->instanced_vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
  pass->vegetation_instanced_vertex_shader_module = LDK_RHI_INVALID_RESOURCE;
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
  desc.vertex_layout.attribute_count = 5;
  desc.vertex_layout.attributes[0].location = 0;
  desc.vertex_layout.attributes[0].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[0].offset =
      (u32)offsetof(LDKMeshVertex, position);
  desc.vertex_layout.attributes[1].location = 1;
  desc.vertex_layout.attributes[1].format = LDK_RHI_VERTEX_FORMAT_FLOAT3;
  desc.vertex_layout.attributes[1].offset =
      (u32)offsetof(LDKMeshVertex, normal);
  desc.vertex_layout.attributes[2].location = 2;
  desc.vertex_layout.attributes[2].format = LDK_RHI_VERTEX_FORMAT_FLOAT2;
  desc.vertex_layout.attributes[2].offset =
      (u32)offsetof(LDKMeshVertex, uv);
  desc.vertex_layout.attributes[3].location = 3;
  desc.vertex_layout.attributes[3].format =
      LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
  desc.vertex_layout.attributes[3].offset =
      (u32)offsetof(LDKMeshVertex, color);
  desc.vertex_layout.attributes[4].location = 8;
  desc.vertex_layout.attributes[4].format = LDK_RHI_VERTEX_FORMAT_FLOAT4;
  desc.vertex_layout.attributes[4].offset =
      (u32)offsetof(LDKMeshVertex, tangent);
  desc.color_attachment_count = 1;
  desc.color_formats[0] = LDK_RHI_FORMAT_RGBA8_UNORM;
  desc.depth_format = LDK_RHI_FORMAT_D32_FLOAT;

  pass->vertex_color_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.vertex_shader_module = pass->terrain_vertex_shader_module;
  desc.fragment_shader_module = pass->terrain_fragment_shader_module;
  pass->terrain_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->terrain_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->fragment_shader_module;

  desc.vertex_shader_module = pass->vegetation_vertex_shader_module;
  desc.fragment_shader_module = pass->vegetation_fragment_shader_module;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pass->vegetation_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vegetation_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  desc.vertex_shader_module = pass->vertex_shader_module;
  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_BACK;

  pass->vertex_color_unlit_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    pass->vertex_color_pipeline = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->overlay_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->overlay_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    pass->vertex_color_unlit_pipeline = LDK_RHI_INVALID_RESOURCE;
    pass->vertex_color_pipeline = LDK_RHI_INVALID_RESOURCE;
    return false;
  }

  desc.fragment_shader_module = pass->solid_unlit_fragment_shader_module;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = false;
  desc.depth_state.compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.raster_state.polygon_mode = LDK_RHI_POLYGON_MODE_LINE;
  desc.raster_state.depth_bias_enabled = true;
  desc.raster_state.depth_bias_slope_factor = -1.0f;
  desc.raster_state.depth_bias_constant_factor = -1.0f;
  pass->wireframe_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->wireframe_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.blend_state.enabled = false;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.raster_state.polygon_mode = LDK_RHI_POLYGON_MODE_FILL;
  desc.raster_state.depth_bias_enabled = false;
  desc.raster_state.depth_bias_slope_factor = 0.0f;
  desc.raster_state.depth_bias_constant_factor = 0.0f;
  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->textured_overlay_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_overlay_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  desc.fragment_shader_module = pass->textured_cutout_fragment_shader_module;
  pass->textured_cutout_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_cutout_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module =
      pass->textured_unlit_cutout_fragment_shader_module;
  pass->textured_unlit_cutout_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_cutout_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = false;
  desc.depth_state.write_enabled = false;
  pass->textured_unlit_cutout_overlay_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_cutout_overlay_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = false;
  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;

  desc.fragment_shader_module = pass->fragment_shader_module;
  pass->vertex_color_blend_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_blend_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  pass->vertex_color_unlit_blend_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_blend_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_blend_pipeline = ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_blend_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_blend_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_blend_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  if (pass->instanced_vertex_shader_module == LDK_RHI_INVALID_RESOURCE ||
      pass->vegetation_instanced_vertex_shader_module ==
          LDK_RHI_INVALID_RESOURCE ||
      pass->rhi->functions.draw_indexed_instanced == NULL)
  {
    return true;
  }

  desc.vertex_shader_module = pass->instanced_vertex_shader_module;
  desc.blend_state.enabled = false;
  desc.depth_state.test_enabled = true;
  desc.depth_state.write_enabled = true;
  s_renderer_mesh_pass_instance_layout(&desc);

  desc.fragment_shader_module = pass->fragment_shader_module;
  pass->vertex_color_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.vertex_shader_module = pass->vegetation_instanced_vertex_shader_module;
  desc.fragment_shader_module = pass->vegetation_fragment_shader_module;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_NONE;
  pass->vegetation_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vegetation_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }
  desc.vertex_shader_module = pass->instanced_vertex_shader_module;
  desc.raster_state.cull_mode = LDK_RHI_CULL_MODE_BACK;

  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  pass->vertex_color_unlit_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_instanced_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->textured_cutout_fragment_shader_module;
  pass->textured_cutout_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_cutout_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module =
      pass->textured_unlit_cutout_fragment_shader_module;
  pass->textured_unlit_cutout_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_cutout_instanced_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.blend_state.enabled = true;
  desc.blend_state.src_color_factor = LDK_RHI_BLEND_FACTOR_SRC_ALPHA;
  desc.blend_state.dst_color_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.color_op = LDK_RHI_BLEND_OP_ADD;
  desc.blend_state.src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  desc.blend_state.dst_alpha_factor =
      LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  desc.blend_state.alpha_op = LDK_RHI_BLEND_OP_ADD;
  desc.depth_state.write_enabled = false;

  desc.fragment_shader_module = pass->fragment_shader_module;
  pass->vertex_color_blend_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_blend_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->overlay_fragment_shader_module;
  pass->vertex_color_unlit_blend_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->vertex_color_unlit_blend_instanced_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->textured_fragment_shader_module;
  pass->textured_blend_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_blend_instanced_pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
    return true;
  }

  desc.fragment_shader_module = pass->textured_unlit_fragment_shader_module;
  pass->textured_unlit_blend_instanced_pipeline =
      ldk_rhi_pipeline_create(pass->rhi, &desc);
  if (pass->textured_unlit_blend_instanced_pipeline ==
      LDK_RHI_INVALID_RESOURCE)
  {
    s_renderer_mesh_pass_disable_instancing(pass);
  }

  return true;
}

static bool s_renderer_mesh_pass_create_buffers(LDKRendererMeshPass* pass)
{
  LDKRHIBufferDesc camera_desc = {0};
  ldk_rhi_buffer_desc_defaults(&camera_desc);
  camera_desc.size = sizeof(LDKRendererMeshCameraParams);
  camera_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  camera_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->camera_buffer = ldk_rhi_buffer_create(pass->rhi, &camera_desc);
  if (pass->camera_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc object_desc = {0};
  ldk_rhi_buffer_desc_defaults(&object_desc);
  object_desc.size = sizeof(LDKRendererMeshObjectParams);
  object_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  object_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->object_buffer = ldk_rhi_buffer_create(pass->rhi, &object_desc);
  if (pass->object_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  LDKRHIBufferDesc material_desc = {0};
  ldk_rhi_buffer_desc_defaults(&material_desc);
  material_desc.size = sizeof(LDKRendererMeshMaterialParams);
  material_desc.usage =
      LDK_RHI_BUFFER_USAGE_UNIFORM | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  material_desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;

  pass->material_buffer = ldk_rhi_buffer_create(pass->rhi, &material_desc);
  if (pass->material_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }
  LDKRHIBufferDesc lighting_desc = material_desc;
  lighting_desc.size = sizeof(LDKRendererLightingParams);
  pass->lighting_buffer = ldk_rhi_buffer_create(pass->rhi, &lighting_desc);
  return pass->lighting_buffer != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_ensure_instance_capacity(
    LDKRendererMeshPass* pass, u32 instance_count)
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

  if (capacity < instance_count || capacity > UINT32_MAX / sizeof(Mat4) ||
      capacity > UINT32_MAX / sizeof(LDKRHIColor))
  {
    return false;
  }

  LDKRHIBufferDesc desc = {0};
  ldk_rhi_buffer_desc_defaults(&desc);
  desc.size = capacity * (u32)sizeof(Mat4);
  desc.usage =
      LDK_RHI_BUFFER_USAGE_VERTEX | LDK_RHI_BUFFER_USAGE_TRANSFER_DST;
  desc.memory_usage = LDK_RHI_MEMORY_USAGE_CPU_TO_GPU;
  desc.stats_class = LDK_RHI_BUFFER_STATS_INSTANCE;

  LDKRHIBuffer transform_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (transform_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  desc.size = capacity * (u32)sizeof(LDKRHIColor);
  LDKRHIBuffer color_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (color_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_buffer_destroy(pass->rhi, transform_buffer);
    return false;
  }

  desc.size = capacity * (u32)sizeof(Vec4);
  LDKRHIBuffer uv_buffer = ldk_rhi_buffer_create(pass->rhi, &desc);
  if (uv_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    ldk_rhi_buffer_destroy(pass->rhi, transform_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, color_buffer);
    return false;
  }

  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_color_buffer);
  ldk_rhi_buffer_destroy(pass->rhi, pass->instance_uv_buffer);
  pass->instance_buffer = transform_buffer;
  pass->instance_color_buffer = color_buffer;
  pass->instance_uv_buffer = uv_buffer;
  pass->instance_capacity = capacity;
  return true;
}

static bool s_renderer_mesh_pass_create_bindings(LDKRendererMeshPass* pass)
{
  LDKRHIBindingsDesc desc = {0};
  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;
  desc.binding_count = 5;
  desc.bindings[0].slot = 0;
  desc.bindings[0].buffer = pass->camera_buffer;
  desc.bindings[0].buffer_offset = 0;
  desc.bindings[0].buffer_size = sizeof(LDKRendererMeshCameraParams);
  desc.bindings[1].slot = 1;
  desc.bindings[1].buffer = pass->object_buffer;
  desc.bindings[1].buffer_offset = 0;
  desc.bindings[1].buffer_size = sizeof(LDKRendererMeshObjectParams);
  desc.bindings[2].slot = 2;
  desc.bindings[2].buffer = pass->material_buffer;
  desc.bindings[2].buffer_offset = 0;
  desc.bindings[2].buffer_size = sizeof(LDKRendererMeshMaterialParams);
  desc.bindings[3].slot = 4;
  desc.bindings[3].buffer = pass->lighting_buffer;
  desc.bindings[3].buffer_size = sizeof(LDKRendererLightingParams);
  desc.bindings[4].slot = 5;
  desc.bindings[4].texture = pass->shadow_texture;
  desc.bindings[4].sampler = pass->shadow_sampler;

  pass->bindings = ldk_rhi_bindings_create(pass->rhi, &desc);
  return pass->bindings != LDK_RHI_INVALID_RESOURCE;
}

static bool s_renderer_mesh_pass_create_fallback_resources(
    LDKRendererMeshPass *pass)
{
  const float flat_normal[4] = {0.5f, 0.5f, 1.0f, 1.0f};
  const u8 white_specular[4] = {255u, 255u, 255u, 255u};
  LDKRHITextureDesc texture_desc = {0};
  LDKRHISamplerDesc sampler_desc = {0};

  ldk_rhi_texture_desc_defaults(&texture_desc);
  texture_desc.type = LDK_RHI_TEXTURE_TYPE_2D;
  texture_desc.format = LDK_RHI_FORMAT_RGBA32_FLOAT;
  texture_desc.width = 1;
  texture_desc.height = 1;
  texture_desc.depth = 1;
  texture_desc.mip_count = 1;
  texture_desc.layer_count = 1;
  texture_desc.usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  texture_desc.initial_data = flat_normal;
  texture_desc.initial_data_size = sizeof(flat_normal);
  pass->flat_normal_texture = ldk_rhi_texture_create(pass->rhi, &texture_desc);
  if (pass->flat_normal_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  texture_desc.format = LDK_RHI_FORMAT_RGBA8_UNORM;
  texture_desc.initial_data = white_specular;
  texture_desc.initial_data_size = sizeof(white_specular);
  pass->white_specular_texture =
      ldk_rhi_texture_create(pass->rhi, &texture_desc);
  if (pass->white_specular_texture == LDK_RHI_INVALID_RESOURCE)
  {
    return false;
  }

  ldk_rhi_sampler_desc_defaults(&sampler_desc);
  sampler_desc.min_filter = LDK_RHI_FILTER_NEAREST;
  sampler_desc.mag_filter = LDK_RHI_FILTER_NEAREST;
  sampler_desc.mip_filter = LDK_RHI_FILTER_NEAREST;
  pass->fallback_sampler = ldk_rhi_sampler_create(pass->rhi, &sampler_desc);
  return pass->fallback_sampler != LDK_RHI_INVALID_RESOURCE;
}

static LDKRHIBindings s_renderer_mesh_pass_create_material_bindings(
    LDKRendererMeshPass *pass,
    const LDKRHITexture textures[LDK_MATERIAL_TEXTURE_SLOT_COUNT],
    const LDKRHISampler samplers[LDK_MATERIAL_TEXTURE_SLOT_COUNT])
{
  LDKRHIBindingsDesc desc = {0};
  u32 binding = 0;

  ldk_rhi_bindings_desc_defaults(&desc);
  desc.layout = pass->bindings_layout;

  desc.bindings[binding].slot = 0;
  desc.bindings[binding].buffer = pass->camera_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshCameraParams);
  binding++;
  desc.bindings[binding].slot = 1;
  desc.bindings[binding].buffer = pass->object_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshObjectParams);
  binding++;
  desc.bindings[binding].slot = 2;
  desc.bindings[binding].buffer = pass->material_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererMeshMaterialParams);
  binding++;

  desc.bindings[binding].slot = 4;
  desc.bindings[binding].buffer = pass->lighting_buffer;
  desc.bindings[binding].buffer_size = sizeof(LDKRendererLightingParams);
  binding++;
  desc.bindings[binding].slot = 5;
  desc.bindings[binding].texture = pass->shadow_texture;
  desc.bindings[binding].sampler = pass->shadow_sampler;
  binding++;

  for (u32 i = 0; i < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
  {
    if (textures[i] == LDK_RHI_INVALID_RESOURCE ||
        samplers[i] == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }
    desc.bindings[binding].slot = s_renderer_material_texture_binding_slots[i];
    desc.bindings[binding].texture = textures[i];
    desc.bindings[binding].sampler = samplers[i];
    binding++;
  }
  desc.binding_count = binding;

  return ldk_rhi_bindings_create(pass->rhi, &desc);
}

static bool s_renderer_mesh_pass_grow_material_bindings_cache(
    LDKRendererMeshPass *pass)
{
  u32 new_capacity = pass->material_bindings_cache_capacity == 0
                         ? 16
                         : pass->material_bindings_cache_capacity * 2;
  size_t new_size =
      (size_t)new_capacity * sizeof(LDKRendererMeshBindingsCacheEntry);
  LDKRendererMeshBindingsCacheEntry *new_cache =
      pass->material_bindings_cache == NULL
          ? (LDKRendererMeshBindingsCacheEntry *)LDK_RENDERER_ALLOC(new_size)
          : (LDKRendererMeshBindingsCacheEntry *)LDK_RENDERER_REALLOC(
                pass->material_bindings_cache, new_size);

  if (!new_cache)
  {
    return false;
  }

  pass->material_bindings_cache = new_cache;
  pass->material_bindings_cache_capacity = new_capacity;
  return true;
}

static bool s_renderer_mesh_material_bindings_equal(
    const LDKRendererMeshBindingsCacheEntry *entry,
    const LDKRHITexture textures[LDK_MATERIAL_TEXTURE_SLOT_COUNT],
    const LDKRHISampler samplers[LDK_MATERIAL_TEXTURE_SLOT_COUNT])
{
  for (u32 i = 0; i < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
  {
    if (entry->textures[i] != textures[i] || entry->samplers[i] != samplers[i])
    {
      return false;
    }
  }
  return true;
}

static LDKRHIBindings s_renderer_mesh_pass_get_material_bindings(
    LDKRendererMeshPass *pass,
    const LDKRHITexture textures[LDK_MATERIAL_TEXTURE_SLOT_COUNT],
    const LDKRHISampler samplers[LDK_MATERIAL_TEXTURE_SLOT_COUNT])
{
  for (u32 i = 0; i < pass->material_bindings_cache_count; i++)
  {
    LDKRendererMeshBindingsCacheEntry *entry =
        &pass->material_bindings_cache[i];
    if (s_renderer_mesh_material_bindings_equal(entry, textures, samplers))
    {
      return entry->bindings;
    }
  }

  if (pass->material_bindings_cache_count ==
          pass->material_bindings_cache_capacity &&
      !s_renderer_mesh_pass_grow_material_bindings_cache(pass))
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIBindings bindings =
      s_renderer_mesh_pass_create_material_bindings(pass, textures, samplers);
  if (bindings == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRendererMeshBindingsCacheEntry *entry =
      &pass->material_bindings_cache[pass->material_bindings_cache_count++];
  memcpy(entry->textures, textures, sizeof(entry->textures));
  memcpy(entry->samplers, samplers, sizeof(entry->samplers));
  entry->bindings = bindings;
  return bindings;
}

static void s_renderer_mesh_pass_remove_texture_bindings(
    LDKRendererMeshPass* pass, LDKRHITexture texture)
{
  if (!pass || !pass->rhi || texture == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  u32 index = 0;
  while (index < pass->material_bindings_cache_count)
  {
    LDKRendererMeshBindingsCacheEntry *entry =
        &pass->material_bindings_cache[index];
    bool found = false;
    for (u32 i = 0; i < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
    {
      if (entry->textures[i] == texture)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      index++;
      continue;
    }

    ldk_rhi_bindings_destroy(pass->rhi, entry->bindings);
    pass->material_bindings_cache_count--;
    if (index != pass->material_bindings_cache_count)
    {
      *entry =
          pass->material_bindings_cache[pass->material_bindings_cache_count];
    }
    memset(&pass->material_bindings_cache[pass->material_bindings_cache_count],
        0, sizeof(*entry));
  }
}

static bool s_renderer_mesh_pass_initialize(LDKRendererMeshPass *pass,
    LDKRendererConfig const *config, const LDKRendererShadowPass *shadow)
{
  if (pass == NULL || config == NULL || config->rhi == NULL)
  {
    return false;
  }
  memset(pass, 0, sizeof(*pass));
  pass->rhi = config->rhi;

  pass->shadow_texture = shadow->depth_texture;
  pass->shadow_sampler = shadow->sampler;

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

  if (!s_renderer_mesh_pass_create_fallback_resources(pass))
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
    for (u32 i = 0; i < pass->material_bindings_cache_count; i++)
    {
      ldk_rhi_bindings_destroy(
          pass->rhi, pass->material_bindings_cache[i].bindings);
    }

    ldk_rhi_bindings_destroy(pass->rhi, pass->vegetation_bindings);
    ldk_rhi_bindings_destroy(pass->rhi, pass->bindings);
    ldk_rhi_sampler_destroy(pass->rhi, pass->fallback_sampler);
    ldk_rhi_texture_destroy(pass->rhi, pass->white_specular_texture);
    ldk_rhi_texture_destroy(pass->rhi, pass->flat_normal_texture);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_color_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->instance_uv_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->lighting_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->material_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->object_buffer);
    ldk_rhi_buffer_destroy(pass->rhi, pass->camera_buffer);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_blend_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_blend_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_cutout_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_cutout_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->vertex_color_unlit_blend_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->vertex_color_blend_instanced_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->vertex_color_unlit_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_instanced_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_blend_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_blend_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->textured_unlit_cutout_overlay_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_cutout_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_overlay_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->textured_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->wireframe_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->overlay_pipeline);
    ldk_rhi_pipeline_destroy(
        pass->rhi, pass->vertex_color_unlit_blend_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_blend_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_unlit_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vertex_color_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->terrain_pipeline);
    ldk_rhi_pipeline_destroy(pass->rhi, pass->vegetation_pipeline);
    ldk_rhi_bindings_layout_destroy(pass->rhi, pass->bindings_layout);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_unlit_cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_cutout_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_unlit_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->textured_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->solid_unlit_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->overlay_fragment_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(pass->rhi, pass->vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->vegetation_instanced_vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->terrain_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->terrain_vertex_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->vegetation_fragment_shader_module);
    ldk_rhi_shader_module_destroy(
        pass->rhi, pass->vegetation_vertex_shader_module);
  }

  LDK_RENDERER_FREE(pass->staging_worlds);
  LDK_RENDERER_FREE(pass->staging_colors);
  LDK_RENDERER_FREE(pass->staging_uvs);
  LDK_RENDERER_FREE(pass->material_bindings_cache);
  memset(pass, 0, sizeof(*pass));
}

static bool s_renderer_mesh_pass_material_is_textured(
    const LDKRendererMaterialResource* material)
{
  return material != NULL &&
         (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND ||
             material->selection == LDK_RENDERER_MATERIAL_SELECTION_TERRAIN);
}

static bool s_renderer_mesh_pass_material_is_lit(
    const LDKRendererMaterialResource *material)
{
  return material != NULL &&
         (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR ||
             material->selection ==
                 LDK_RENDERER_MATERIAL_SELECTION_VEGETATION ||
             material->selection == LDK_RENDERER_MATERIAL_SELECTION_TERRAIN);
}

static LDKRHIPipeline s_renderer_mesh_pass_pipeline(
    const LDKRendererMeshPass* pass,
    LDKRendererMaterialSelection selection, u32 flags, bool instanced)
{
  bool overlay = (flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0;
  if (overlay)
  {
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT)
    {
      return pass->textured_unlit_cutout_overlay_pipeline;
    }
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND)
    {
      return pass->textured_overlay_pipeline;
    }
    if (selection == LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT ||
        selection == LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_BLEND ||
        selection ==
            LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT_BLEND)
    {
      return pass->overlay_pipeline;
    }
    return LDK_RHI_INVALID_RESOURCE;
  }

  switch (selection)
  {
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR:
      return instanced ? pass->vertex_color_instanced_pipeline
                       : pass->vertex_color_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT:
      return instanced ? pass->vertex_color_unlit_instanced_pipeline
                       : pass->vertex_color_unlit_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_BLEND:
      return instanced ? pass->vertex_color_blend_instanced_pipeline
                       : pass->vertex_color_blend_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT_BLEND:
      return instanced ? pass->vertex_color_unlit_blend_instanced_pipeline
                       : pass->vertex_color_unlit_blend_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED:
      return instanced ? pass->textured_instanced_pipeline
                       : pass->textured_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT:
      return instanced ? pass->textured_unlit_instanced_pipeline
                       : pass->textured_unlit_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT:
      return instanced ? pass->textured_cutout_instanced_pipeline
                       : pass->textured_cutout_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT:
      return instanced ? pass->textured_unlit_cutout_instanced_pipeline
                       : pass->textured_unlit_cutout_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND:
      return instanced ? pass->textured_blend_instanced_pipeline
                       : pass->textured_blend_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND:
      return instanced ? pass->textured_unlit_blend_instanced_pipeline
                       : pass->textured_unlit_blend_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_VEGETATION:
      return instanced ? pass->vegetation_instanced_pipeline
                       : pass->vegetation_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_TERRAIN:
      return instanced ? LDK_RHI_INVALID_RESOURCE : pass->terrain_pipeline;
    case LDK_RENDERER_MATERIAL_SELECTION_INVALID:
    default:
      return LDK_RHI_INVALID_RESOURCE;
  }
}

static LDKRendererMeshSubmit* s_renderer_mesh_pass_run_submit(
    LDKRenderer* renderer, LDKRendererMeshSubmit* first_submit,
    const u64* sort_items, u32 index)
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

static bool s_renderer_mesh_pass_staging_ensure(
    LDKRendererMeshPass *pass, u32 count)
{
  if (count <= pass->staging_capacity)
    return true;
  if (count > SIZE_MAX / sizeof(Mat4) ||
      count > SIZE_MAX / sizeof(LDKRHIColor) ||
      count > SIZE_MAX / sizeof(Vec4))
    return false;
  u32 capacity = pass->staging_capacity ? pass->staging_capacity : 256u;
  while (capacity < count)
    capacity = capacity > UINT32_MAX / 2u ? count : capacity * 2u;
  Mat4 *worlds = LDK_RENDERER_REALLOC(
      pass->staging_worlds, (size_t)capacity * sizeof(Mat4));
  if (!worlds)
    return false;
  pass->staging_worlds = worlds;
  LDKRHIColor *colors = LDK_RENDERER_REALLOC(
      pass->staging_colors, (size_t)capacity * sizeof(LDKRHIColor));
  if (!colors)
    return false;
  pass->staging_colors = colors;
  Vec4 *uvs = LDK_RENDERER_REALLOC(
      pass->staging_uvs, (size_t)capacity * sizeof(Vec4));
  if (!uvs)
    return false;
  pass->staging_uvs = uvs;
  pass->staging_capacity = capacity;
  return true;
}

static void s_renderer_mesh_pass_draw_run(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererFrameDomainStats* stats,
    LDKRendererMeshSubmit* first_submit, const u64* sort_items,
    u32 submit_count, bool translucent)
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
  const LDKRHIColor *colors = !instance_set && first_submit->instance_count
      ? renderer->submitted_instance_colors + first_submit->instance_offset
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

  /* The sorted queue contains ordinary submissions, not explicit instance
   * arrays. Pack consecutive compatible draws into reusable CPU staging. */
  if (!instance_set && !worlds && submit_count > 1u &&
      s_renderer_mesh_pass_staging_ensure(pass, submit_count))
  {
    bool complete = true;
    for (u32 i = 0; i < submit_count; ++i)
    {
      LDKRendererMeshSubmit *submit =
          s_renderer_mesh_pass_run_submit(renderer, first_submit, sort_items, i);
      if (!submit)
      {
        complete = false;
        break;
      }
      pass->staging_worlds[i] = submit->world;
      pass->staging_colors[i] = ldk_renderer_color_from_rgba32(0xffffffffu);
      pass->staging_uvs[i] = submit->uv_rect;
    }
    if (complete)
    {
      worlds = pass->staging_worlds;
      colors = pass->staging_colors;
      uv_rects = pass->staging_uvs;
    }
  }
  if (!instance_set && worlds && !uv_rects &&
      s_renderer_mesh_pass_staging_ensure(pass, submit_count))
  {
    for (u32 i = 0; i < submit_count; ++i)
      pass->staging_uvs[i] = first_submit->uv_rect;
    uv_rects = pass->staging_uvs;
  }

  LDKRendererMeshSubmit* first = first_submit;
  LDKRendererMeshResource* mesh =
      s_renderer_mesh_get_resource(renderer, first->mesh);
  LDKRendererMaterialResource* material =
      s_renderer_material_get_resource(renderer, first->material);
  if (mesh == NULL || mesh->index_count == 0 || material == NULL)
  {
    return;
  }

  bool overlay =
      (first->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0;
  bool textured = s_renderer_mesh_pass_material_is_textured(material);
  bool lit = !overlay && s_renderer_mesh_pass_material_is_lit(material);
  bool vegetation = material->selection ==
      LDK_RENDERER_MATERIAL_SELECTION_VEGETATION;

  LDKRHIBindings bindings = pass->bindings;
  if (vegetation && renderer->vegetation_interaction_enabled &&
      pass->vegetation_bindings != LDK_RHI_INVALID_RESOURCE)
  {
    bindings = pass->vegetation_bindings;
  }
  else if (textured || lit)
  {
    LDKRHITexture textures[LDK_MATERIAL_TEXTURE_SLOT_COUNT];
    LDKRHISampler samplers[LDK_MATERIAL_TEXTURE_SLOT_COUNT];

    for (u32 i = 0; i < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
    {
      textures[i] = pass->white_specular_texture;
      samplers[i] = pass->fallback_sampler;
    }
    textures[LDK_MATERIAL_TEXTURE_SLOT_NORMAL] = pass->flat_normal_texture;

    if (textured)
    {
      LDKRendererTextureResource* texture =
          s_renderer_texture_get_resource(renderer, material->desc.texture);
      if (texture == NULL)
      {
        if (material->selection != LDK_RENDERER_MATERIAL_SELECTION_TERRAIN)
        {
          return;
        }
      }
      else
      {
        textures[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO] = texture->texture;
        samplers[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO] =
            material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO] !=
            LDK_RHI_INVALID_RESOURCE
            ? material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_ALBEDO]
            : texture->sampler;
      }

      for (u32 i = 0; i < LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT; ++i)
      {
        if (material->desc.additional_textures[i].id ==
            LDK_RHI_INVALID_RESOURCE)
        {
          continue;
        }
        texture = s_renderer_texture_get_resource(
            renderer, material->desc.additional_textures[i]);
        if (texture == NULL)
        {
          return;
        }
        u32 slot = i + 3u;
        textures[slot] = texture->texture;
        samplers[slot] =
            material->texture_samplers[slot] != LDK_RHI_INVALID_RESOURCE
            ? material->texture_samplers[slot]
            : texture->sampler;
      }
    }

    if (lit)
    {
      if (material->desc.normal_map.id != LDK_RHI_INVALID_RESOURCE)
      {
        LDKRendererTextureResource* texture = s_renderer_texture_get_resource(
            renderer, material->desc.normal_map);
        if (texture != NULL)
        {
          textures[LDK_MATERIAL_TEXTURE_SLOT_NORMAL] = texture->texture;
          samplers[LDK_MATERIAL_TEXTURE_SLOT_NORMAL] =
              material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_NORMAL] !=
              LDK_RHI_INVALID_RESOURCE
              ? material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_NORMAL]
              : texture->sampler;
        }
      }
      if (material->desc.specular_map.id != LDK_RHI_INVALID_RESOURCE)
      {
        LDKRendererTextureResource* texture = s_renderer_texture_get_resource(
            renderer, material->desc.specular_map);
        if (texture != NULL)
        {
          textures[LDK_MATERIAL_TEXTURE_SLOT_SPECULAR] = texture->texture;
          samplers[LDK_MATERIAL_TEXTURE_SLOT_SPECULAR] =
              material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_SPECULAR] !=
              LDK_RHI_INVALID_RESOURCE
              ? material->texture_samplers[LDK_MATERIAL_TEXTURE_SLOT_SPECULAR]
              : texture->sampler;
        }
      }
    }

    bindings =
        s_renderer_mesh_pass_get_material_bindings(pass, textures, samplers);
    if (bindings == LDK_RHI_INVALID_RESOURCE)
    {
      return;
    }
  }

  LDKRendererMeshMaterialParams material_params = {0};
  material_params.color =
      ldk_renderer_color_from_rgba32(material->desc.color);
  material_params.alpha_cutoff = material->desc.alpha_cutoff;
  material_params.vegetation[0] = material->desc.vegetation_curvature;
  material_params.vegetation[1] = material->desc.vegetation_wind_strength;
  material_params.vegetation[2] = material->desc.vegetation_wind_speed;
  material_params.vegetation[3] = material->desc.vegetation_wind_direction;
  for (u32 slot = 0; slot < LDK_MATERIAL_TEXTURE_SLOT_COUNT; ++slot)
  {
    material_params.uv_transform[slot][0] = material->texture_uv_scales[slot].x;
    material_params.uv_transform[slot][1] = material->texture_uv_scales[slot].y;
    material_params.uv_transform[slot][2] = material->texture_uv_offsets[slot].x;
    material_params.uv_transform[slot][3] = material->texture_uv_offsets[slot].y;
  }
  if (material->selection == LDK_RENDERER_MATERIAL_SELECTION_TERRAIN)
  {
    material_params.terrain_info[0] =
        (float)material->desc.terrain_surface_count;
    for (u32 i = 0u; i < material->desc.terrain_surface_count; ++i)
    {
      const LDKRendererTerrainSurfaceDesc *surface =
          &material->desc.terrain_surfaces[i];
      LDKRHIColor color = ldk_renderer_color_from_rgba32(surface->color);
      material_params.terrain_range[i][0] = surface->min_height;
      material_params.terrain_range[i][1] = surface->blend_width;
      material_params.terrain_range[i][2] = surface->texture_weight;
      material_params.terrain_color[i][0] = color.r;
      material_params.terrain_color[i][1] = color.g;
      material_params.terrain_color[i][2] = color.b;
      material_params.terrain_color[i][3] = color.a;
      material_params.terrain_atlas_rect[i][0] = surface->atlas_rect.x;
      material_params.terrain_atlas_rect[i][1] = surface->atlas_rect.y;
      material_params.terrain_atlas_rect[i][2] = surface->atlas_rect.z;
      material_params.terrain_atlas_rect[i][3] = surface->atlas_rect.w;
      material_params.terrain_uv_scale[i][0] = surface->uv_scale.x;
      material_params.terrain_uv_scale[i][1] = surface->uv_scale.y;
    }
  }
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
  if (lit)
  {
    material_params.specular = material->desc.specular;
    material_params.shininess = material->desc.shininess;
    material_params.emission = material->desc.emission;
  }
  ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
      sizeof(material_params), &material_params);

  LDKRHIBuffer transform_buffer = LDK_RHI_INVALID_RESOURCE;
  LDKRHIBuffer color_buffer = LDK_RHI_INVALID_RESOURCE;
  if (instance_set && s_renderer_mesh_pass_staging_ensure(pass, submit_count))
  {
    for (u32 i = 0; i < submit_count; ++i)
      pass->staging_uvs[i] = first_submit->uv_rect;
    uv_rects = pass->staging_uvs;
  }
  if (instance_set)
  {
    transform_buffer = instance_set->transform_buffer;
    color_buffer = instance_set->color_buffer;
  }
  else if (worlds &&
      s_renderer_mesh_pass_ensure_instance_capacity(pass, submit_count) &&
      ldk_rhi_buffer_update(pass->rhi, pass->instance_buffer, 0,
          submit_count * (u32)sizeof(Mat4), worlds) &&
      ldk_rhi_buffer_update(pass->rhi, pass->instance_color_buffer, 0,
          submit_count * (u32)sizeof(LDKRHIColor), colors))
  {
    transform_buffer = pass->instance_buffer;
    color_buffer = pass->instance_color_buffer;
  }

  bool valid_uv = uv_rects &&
      s_renderer_mesh_pass_ensure_instance_capacity(pass, submit_count) &&
      ldk_rhi_buffer_update(pass->rhi, pass->instance_uv_buffer, 0,
          submit_count * (u32)sizeof(Vec4), uv_rects);

  if (!overlay && valid_uv &&
      transform_buffer != LDK_RHI_INVALID_RESOURCE &&
      color_buffer != LDK_RHI_INVALID_RESOURCE &&
      pass->rhi->functions.draw_indexed_instanced != NULL)
  {
    LDKRHIPipeline pipeline = s_renderer_mesh_pass_pipeline(
        pass, material->selection, first->flags, true);
    if (pipeline != LDK_RHI_INVALID_RESOURCE)
    {
      LDKRendererMeshObjectParams object = {0};
      object.world = mat4_identity();
      object.instance_color = ldk_renderer_color_from_rgba32(0xffffffffu);
      object.uv_rect[0] = 0.0f;
      object.uv_rect[1] = 0.0f;
      object.uv_rect[2] = 1.0f;
      object.uv_rect[3] = 1.0f;
      object.options[0] =
          (first->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD)
          ? 1.0f : 0.0f;
      ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
          sizeof(object), &object);

      ldk_rhi_pipeline_bind(pass->rhi, pipeline);
      ldk_rhi_bindings_bind(pass->rhi, bindings);
      ldk_rhi_vertex_buffer_bind_at(
          pass->rhi, 0, mesh->vertex_buffer, 0);
      ldk_rhi_vertex_buffer_bind_at(
          pass->rhi, 1, transform_buffer,
          instance_buffer_offset * (u32)sizeof(Mat4));
      ldk_rhi_vertex_buffer_bind_at(
          pass->rhi, 2, color_buffer,
          instance_buffer_offset * (u32)sizeof(LDKRHIColor));
      ldk_rhi_vertex_buffer_bind_at(
          pass->rhi, 3, pass->instance_uv_buffer, 0);
      ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
          LDK_RHI_INDEX_TYPE_UINT32);

      LDKRHIDrawIndexedInstancedDesc draw = {0};
      draw.index_count = first->index_count;
      draw.instance_count = submit_count;
      draw.first_index = first->first_index;
      draw.vertex_offset = 0;
      draw.first_instance = 0;
      ldk_rhi_draw_indexed_instanced(pass->rhi, &draw);
      stats->draw_call_count += 1;
      if (!translucent)
      {
        stats->opaque_mesh_draw_call_count += 1;
      }
      stats->instanced_batch_count += 1;
      stats->instanced_instance_count += submit_count;
      return;
    }
  }

  if (instance_set)
  {
    return;
  }

  LDKRHIPipeline pipeline = s_renderer_mesh_pass_pipeline(
      pass, material->selection, first->flags, false);
  if (pipeline == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  ldk_rhi_pipeline_bind(pass->rhi, pipeline);
  ldk_rhi_bindings_bind(pass->rhi, bindings);
  ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
  ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
      LDK_RHI_INDEX_TYPE_UINT32);

  for (u32 i = 0; i < submit_count; ++i)
  {
    LDKRendererMeshSubmit* submit =
        worlds ? first :
        s_renderer_mesh_pass_run_submit(renderer, first, sort_items, i);
    if (submit == NULL)
    {
      return;
    }

    LDKRendererMeshObjectParams object = {0};
    object.world = worlds ? worlds[i] : submit->world;
    object.instance_color = colors
        ? colors[i]
        : ldk_renderer_color_from_rgba32(0xffffffffu);
    object.uv_rect[0] = submit->uv_rect.x;
    object.uv_rect[1] = submit->uv_rect.y;
    object.uv_rect[2] = submit->uv_rect.z;
    object.uv_rect[3] = submit->uv_rect.w;
    object.options[0] =
        (submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD)
        ? 1.0f : 0.0f;
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);

    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = first->index_count;
    draw.first_index = first->first_index;
    draw.vertex_offset = 0;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    if (overlay)
    {
      stats->overlay_mesh_draw_call_count += 1;
    }
    else if (!translucent)
    {
      stats->opaque_mesh_draw_call_count += 1;
    }
  }
}

static void s_renderer_mesh_pass_draw_unsorted_submissions(
    LDKRenderer* renderer, LDKRendererMeshPass* pass,
    LDKRendererView const* view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[i];
    if ((submit->flags & (LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY |
                              LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE)) !=
            flags ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    LDKRendererMaterialResource *material =
        s_renderer_material_get_resource(renderer, submit->material);
    if ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) == 0 &&
        s_renderer_material_is_blended(material))
    {
      continue;
    }

    if ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0)
    {
      u32 instance_count =
          s_renderer_mesh_submit_instance_count(renderer, submit);
      stats->overlay_mesh_render_count +=
          instance_count ? instance_count : 1u;
    }
    else
    {
      u32 instance_count =
          s_renderer_mesh_submit_instance_count(renderer, submit);
      stats->opaque_mesh_render_count +=
          instance_count ? instance_count : 1u;
      stats->batch_count += 1;
      if (stats->max_batch_size < 1)
      {
        stats->max_batch_size = 1;
      }
    }

    s_renderer_mesh_pass_draw_run(
        renderer, pass, stats, submit, NULL, 1, false);
  }
}

static void s_renderer_mesh_pass_draw_submissions(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererView const* view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) != 0)
  {
    s_renderer_mesh_pass_draw_unsorted_submissions(
        renderer, pass, view, flags);
    return;
  }

  u64* sort_items = NULL;
  u32 sort_item_count = 0;
  if (!s_renderer_mesh_sort_queue_build(
          renderer, view, LDK_RENDERER_MESH_SUBMIT_FLAG_NONE, &sort_items,
          &sort_item_count))
  {
    s_renderer_mesh_pass_draw_unsorted_submissions(
        renderer, pass, view, flags);
    return;
  }

  stats->opaque_mesh_render_count += sort_item_count;

  u32 i = 0;
  while (i < sort_item_count)
  {
    u32 submit_index = s_renderer_mesh_sort_item_submit_index(sort_items[i]);
    if (submit_index >= renderer->submitted_mesh_count)
    {
      ++i;
      continue;
    }

    LDKRendererMeshSubmit* submit = &renderer->submitted_meshes[submit_index];
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

    u32 batch_size = end - i;
    u32 instance_count =
        s_renderer_mesh_submit_instance_count(renderer, submit);
    if (instance_count)
    {
      stats->opaque_mesh_render_count += instance_count - 1u;
    }
    stats->batch_count += 1;
    if (batch_size > stats->max_batch_size)
    {
      stats->max_batch_size = batch_size;
    }

    s_renderer_mesh_pass_draw_run(
        renderer, pass, stats, submit, &sort_items[i], batch_size, false);
    i = end;
  }
}

static float s_renderer_mesh_submit_depth(const LDKRenderer *renderer,
    const LDKRendererView *view, const LDKRendererMeshSubmit *submit)
{
  Vec3 center = vec3_make(
      submit->world.m[12], submit->world.m[13], submit->world.m[14]);
  if (submit->instance_count &&
      submit->instance_set.id == LDK_RHI_INVALID_RESOURCE)
  {
    const Mat4 *worlds =
        renderer->submitted_instance_worlds + submit->instance_offset;
    center = vec3_make(0.0f, 0.0f, 0.0f);
    for (u32 i = 0; i < submit->instance_count; ++i)
    {
      center.x += worlds[i].m[12];
      center.y += worlds[i].m[13];
      center.z += worlds[i].m[14];
    }
    float inv_count = 1.0f / (float)submit->instance_count;
    center.x *= inv_count;
    center.y *= inv_count;
    center.z *= inv_count;
  }

  Vec3 view_position = mat4_mul_point(view->view, center);
  return -view_position.z;
}

static void s_renderer_mesh_pass_draw_translucent(LDKRenderer *renderer,
    LDKRendererMeshPass *pass, const LDKRendererView *view)
{
  if (!renderer || !pass || !pass->is_initialized || !view ||
      !view->submitted || !renderer->submitted_mesh_count)
  {
    return;
  }

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  if (!s_renderer_mesh_sort_storage_ensure(
          renderer, renderer->submitted_mesh_count))
  {
    for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
    {
      LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[i];
      if ((submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) ||
          (submit->view_id != LDK_RENDERER_VIEW_ALL &&
              submit->view_id != view->id))
      {
        continue;
      }

      LDKRendererMaterialResource *material =
          s_renderer_material_get_resource(renderer, submit->material);
      if (s_renderer_material_is_blended(material))
      {
        s_renderer_mesh_pass_draw_run(
            renderer, pass, stats, submit, NULL, 1, true);
      }
    }
    return;
  }

  u32 item_count = 0;
  for (u32 i = 0; i < renderer->submitted_mesh_count; ++i)
  {
    LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[i];
    if ((submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY) ||
        (submit->view_id != LDK_RENDERER_VIEW_ALL &&
            submit->view_id != view->id))
    {
      continue;
    }

    LDKRendererMaterialResource *material =
        s_renderer_material_get_resource(renderer, submit->material);
    if (!s_renderer_material_is_blended(material))
    {
      continue;
    }

    float depth = s_renderer_mesh_submit_depth(renderer, view, submit);
    if (!isfinite(depth))
    {
      continue;
    }
    u32 depth_bits;
    memcpy(&depth_bits, &depth, sizeof(depth_bits));
    renderer->mesh_sort_items[item_count++] =
        ((u64)depth_bits << 32u) | (u64)i;
  }

  for (u32 i = 1; i < item_count; ++i)
  {
    u64 item = renderer->mesh_sort_items[i];
    u32 item_bits = (u32)(item >> 32u);
    float item_depth;
    memcpy(&item_depth, &item_bits, sizeof(item_depth));
    u32 j = i;
    while (j > 0)
    {
      u32 previous_bits =
          (u32)(renderer->mesh_sort_items[j - 1u] >> 32u);
      float previous_depth;
      memcpy(&previous_depth, &previous_bits, sizeof(previous_depth));
      if (previous_depth >= item_depth)
      {
        break;
      }
      renderer->mesh_sort_items[j] = renderer->mesh_sort_items[j - 1u];
      --j;
    }
    renderer->mesh_sort_items[j] = item;
  }

  for (u32 i = 0; i < item_count; ++i)
  {
    u32 submit_index = (u32)renderer->mesh_sort_items[i];
    if (submit_index >= renderer->submitted_mesh_count)
    {
      continue;
    }
    LDKRendererMeshSubmit *submit = &renderer->submitted_meshes[submit_index];
    s_renderer_mesh_pass_draw_run(
        renderer, pass, stats, submit, NULL, 1, true);
  }
}

static bool s_renderer_line_mesh_create(LDKRenderer *renderer)
{
  if (ldk_renderer_mesh_is_valid(renderer, renderer->line_mesh))
  {
    return true;
  }
  // Radius 1/2, along +Z from 0 to 1. Outward CCW faces and end caps.
  LDKMeshVertex vertices[6] = {0};
  const u32 indices[] = {0, 2, 1, 3, 4, 5,
      0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 2, 0, 3, 2, 3, 5};
  for (u32 i = 0; i < 6; ++i)
  {
    float angle = (float)(i % 3) * (2.0f * STDXM_PI / 3.0f);
    vertices[i].position = vec3_make(0.5f * cosf(angle),
        0.5f * sinf(angle), i < 3 ? 0.0f : 1.0f);
    vertices[i].normal = vec3_make(cosf(angle), sinf(angle), 0.0f);
    vertices[i].color = 0xffffffffu;
  }
  LDKRendererMeshDesc desc = {0};
  desc.vertices = vertices;
  desc.vertex_count = 6;
  desc.indices = indices;
  desc.index_count = 24;
  renderer->line_mesh = ldk_renderer_mesh_create(renderer, &desc);
  return ldk_renderer_mesh_is_valid(renderer, renderer->line_mesh);
}

bool ldk_renderer_draw_line(LDKRenderer *renderer, LDKRendererViewId view_id,
    Vec3 start, Vec3 end, float thickness, u32 color, bool depth_test)
{
  if (!renderer || !renderer->is_initialized ||
      view_id == LDK_RENDERER_VIEW_INVALID ||
      !isfinite(thickness) || thickness <= 0.0f ||
      !isfinite(start.x) || !isfinite(start.y) || !isfinite(start.z) ||
      !isfinite(end.x) || !isfinite(end.y) || !isfinite(end.z))
  {
    return false;
  }
  Vec3 delta = vec3_sub(end, start);
  float length = hypotf(hypotf(delta.x, delta.y), delta.z);
  if (!isfinite(length))
  {
    return false;
  }
  if (length == 0.0f)
  {
    return true;
  }
  Vec3 forward = vec3_make(delta.x / length, delta.y / length,
      delta.z / length);
  Vec3 reference = fabsf(forward.y) < 0.9f
      ? vec3_make(0, 1, 0) : vec3_make(1, 0, 0);
  Vec3 right = vec3_norm(vec3_cross(reference, forward));
  Vec3 up = vec3_cross(forward, right);
  Mat4 world = mat4_identity();
  for (u32 i = 0; i < 3; ++i)
  {
    const float x[] = {right.x, right.y, right.z};
    const float y[] = {up.x, up.y, up.z};
    const float z[] = {delta.x, delta.y, delta.z};
    world.m[i] = x[i] * thickness;
    world.m[4 + i] = y[i] * thickness;
    world.m[8 + i] = z[i];
  }
  world.m[12] = start.x;
  world.m[13] = start.y;
  world.m[14] = start.z;
  if (renderer->submitted_line_count == renderer->submitted_line_capacity)
  {
    u32 capacity = renderer->submitted_line_capacity == 0
        ? 128 : renderer->submitted_line_capacity * 2;
    size_t size = (size_t)capacity * sizeof(LDKRendererLineSubmit);
    if (capacity <= renderer->submitted_line_capacity ||
        size / sizeof(LDKRendererLineSubmit) != capacity)
    {
      return false;
    }
    LDKRendererLineSubmit *lines = LDK_RENDERER_REALLOC(
        renderer->submitted_lines, size);
    if (!lines)
    {
      return false;
    }
    renderer->submitted_lines = lines;
    renderer->submitted_line_capacity = capacity;
  }
  if (!s_renderer_line_mesh_create(renderer))
  {
    return false;
  }
  LDKRendererLineSubmit *line =
      &renderer->submitted_lines[renderer->submitted_line_count++];
  line->world = world;
  line->view_id = view_id;
  line->color = color;
  line->depth_test = depth_test;
  return true;
}

static bool s_renderer_text_vertices_reserve(
    LDKRenderer *renderer, u32 additional_count)
{
  if (additional_count > UINT32_MAX - renderer->submitted_text_vertex_count)
  {
    return false;
  }

  u32 required = renderer->submitted_text_vertex_count + additional_count;
  if (required <= renderer->submitted_text_vertex_capacity)
  {
    return true;
  }

  u32 capacity = renderer->submitted_text_vertex_capacity == 0
      ? 256 : renderer->submitted_text_vertex_capacity;
  while (capacity < required && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  if (capacity < required)
  {
    capacity = required;
  }

  size_t size = (size_t)capacity * sizeof(LDKRendererTextVertex);
  if (size / sizeof(LDKRendererTextVertex) != capacity)
  {
    return false;
  }

  LDKRendererTextVertex *vertices = LDK_RENDERER_REALLOC(
      renderer->submitted_text_vertices, size);
  if (vertices == NULL)
  {
    return false;
  }

  renderer->submitted_text_vertices = vertices;
  renderer->submitted_text_vertex_capacity = capacity;
  return true;
}

static bool s_renderer_text_indices_reserve(
    LDKRenderer *renderer, u32 additional_count)
{
  if (additional_count > UINT32_MAX - renderer->submitted_text_index_count)
  {
    return false;
  }

  u32 required = renderer->submitted_text_index_count + additional_count;
  if (required <= renderer->submitted_text_index_capacity)
  {
    return true;
  }

  u32 capacity = renderer->submitted_text_index_capacity == 0
      ? 384 : renderer->submitted_text_index_capacity;
  while (capacity < required && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  if (capacity < required)
  {
    capacity = required;
  }

  size_t size = (size_t)capacity * sizeof(u32);
  if (size / sizeof(u32) != capacity)
  {
    return false;
  }

  u32 *indices = LDK_RENDERER_REALLOC(renderer->submitted_text_indices, size);
  if (indices == NULL)
  {
    return false;
  }

  renderer->submitted_text_indices = indices;
  renderer->submitted_text_index_capacity = capacity;
  return true;
}

static bool s_renderer_text_submits_reserve(
    LDKRenderer *renderer, u32 additional_count)
{
  if (additional_count > UINT32_MAX - renderer->submitted_text_count)
  {
    return false;
  }

  u32 required = renderer->submitted_text_count + additional_count;
  if (required <= renderer->submitted_text_capacity)
  {
    return true;
  }

  u32 capacity = renderer->submitted_text_capacity == 0
      ? 32 : renderer->submitted_text_capacity;
  while (capacity < required && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  if (capacity < required)
  {
    capacity = required;
  }

  size_t size = (size_t)capacity * sizeof(LDKRendererTextSubmit);
  if (size / sizeof(LDKRendererTextSubmit) != capacity)
  {
    return false;
  }

  LDKRendererTextSubmit *submits = LDK_RENDERER_REALLOC(
      renderer->submitted_texts, size);
  if (submits == NULL)
  {
    return false;
  }

  renderer->submitted_texts = submits;
  renderer->submitted_text_capacity = capacity;
  return true;
}

bool ldk_renderer_submit_text(LDKRenderer *renderer,
    LDKRendererViewId view_id, LDKFontInstance *font, char const *text,
    Mat4 world, u32 color, u32 flags)
{
  if (renderer == NULL || !renderer->is_initialized || font == NULL ||
      text == NULL || view_id == LDK_RENDERER_VIEW_INVALID ||
      (flags & ~(LDK_RENDERER_TEXT_SUBMIT_FLAG_BILLBOARD |
                   LDK_RENDERER_TEXT_SUBMIT_FLAG_NO_DEPTH_TEST)) != 0)
  {
    return false;
  }

  for (u32 i = 0; i < 16; ++i)
  {
    if (!isfinite(world.m[i]))
    {
      return false;
    }
  }

  if (*text == '\0')
  {
    return true;
  }

  float pixel_height = ldk_ttf_get_pixel_height(font);
  if (!isfinite(pixel_height) || pixel_height <= 0.0f)
  {
    return false;
  }

  u32 vertex_start = renderer->submitted_text_vertex_count;
  u32 index_start = renderer->submitted_text_index_count;
  u32 submit_start = renderer->submitted_text_count;
  LDKRHITexture current_texture = LDK_RHI_INVALID_RESOURCE;
  LDKRendererTextSubmit *current_submit = NULL;
  float scale = 1.0f / pixel_height;
  u32 packed_color = LDK_RGBA32(color);

  LDKTextLayoutIterator layout;
  if (!ldk_text_layout_begin(&layout, font, text, 0.0f, 0.0f, true))
  {
    return false;
  }

  LDKTextGlyphQuad quad;
  while (ldk_text_layout_next(&layout, &quad))
  {
    LDKRHITexture texture = (LDKRHITexture)ldk_renderer_get_font_page_texture(
        renderer, font, quad.page_index);
    if (texture == LDK_RHI_INVALID_RESOURCE ||
        !s_renderer_text_vertices_reserve(renderer, 4) ||
        !s_renderer_text_indices_reserve(renderer, 6))
    {
      renderer->submitted_text_vertex_count = vertex_start;
      renderer->submitted_text_index_count = index_start;
      renderer->submitted_text_count = submit_start;
      return false;
    }

    if (current_submit == NULL || texture != current_texture)
    {
      if (!s_renderer_text_submits_reserve(renderer, 1))
      {
        renderer->submitted_text_vertex_count = vertex_start;
        renderer->submitted_text_index_count = index_start;
        renderer->submitted_text_count = submit_start;
        return false;
      }

      current_submit =
          &renderer->submitted_texts[renderer->submitted_text_count++];
      *current_submit = (LDKRendererTextSubmit){0};
      current_submit->world = world;
      current_submit->view_id = view_id;
      current_submit->texture = texture;
      current_submit->index_offset = renderer->submitted_text_index_count;
      current_submit->flags = flags;
      current_texture = texture;
    }

    u32 base_index = renderer->submitted_text_vertex_count;
    LDKRendererTextVertex *vertices =
        &renderer->submitted_text_vertices[base_index];
    vertices[0] = (LDKRendererTextVertex){
        quad.x0 * scale, -quad.y0 * scale, quad.u0, quad.v0, packed_color};
    vertices[1] = (LDKRendererTextVertex){
        quad.x1 * scale, -quad.y0 * scale, quad.u1, quad.v0, packed_color};
    vertices[2] = (LDKRendererTextVertex){
        quad.x1 * scale, -quad.y1 * scale, quad.u1, quad.v1, packed_color};
    vertices[3] = (LDKRendererTextVertex){
        quad.x0 * scale, -quad.y1 * scale, quad.u0, quad.v1, packed_color};
    renderer->submitted_text_vertex_count += 4;

    u32 *indices =
        &renderer->submitted_text_indices[renderer->submitted_text_index_count];
    indices[0] = base_index + 0;
    indices[1] = base_index + 1;
    indices[2] = base_index + 2;
    indices[3] = base_index + 2;
    indices[4] = base_index + 3;
    indices[5] = base_index + 0;
    renderer->submitted_text_index_count += 6;
    current_submit->index_count += 6;
  }

  return true;
}

static void s_renderer_lines_draw(LDKRenderer *renderer,
    LDKRendererMeshPass *pass, const LDKRendererView *view, u32 flags)
{
  LDKRendererFrameDomainStats* stats =
      s_renderer_frame_stats_for_view(renderer, view);

  if (!renderer->submitted_line_count)
  {
    return;
  }
  LDKRendererMeshResource *mesh =
      s_renderer_mesh_get_resource(renderer, renderer->line_mesh);
  if (!mesh)
  {
    return;
  }
  bool overlay = flags == LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY;
  bool bound = false;
  for (u32 i = 0; i < renderer->submitted_line_count; ++i)
  {
    const LDKRendererLineSubmit *line = &renderer->submitted_lines[i];
    if (line->depth_test == overlay ||
        (line->view_id != LDK_RENDERER_VIEW_ALL && line->view_id != view->id))
    {
      continue;
    }
    if (!bound)
    {
      ldk_rhi_pipeline_bind(pass->rhi, overlay
          ? pass->overlay_pipeline : pass->vertex_color_unlit_pipeline);
      ldk_rhi_bindings_bind(pass->rhi, pass->bindings);
      ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
      ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
          LDK_RHI_INDEX_TYPE_UINT32);
      bound = true;
    }
    LDKRendererMeshObjectParams object = {0};
    object.world = line->world;
    object.instance_color = ldk_renderer_color_from_rgba32(0xffffffffu);
    LDKRendererMeshMaterialParams material = {0};
    material.color = ldk_renderer_color_from_rgba32(line->color);
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);
    ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
        sizeof(material), &material);
    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = mesh->index_count;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->line_draw_call_count += 1;
  }
}

static void s_renderer_wireframes_draw(LDKRenderer *renderer,
    LDKRendererMeshPass *pass, const LDKRendererView *view)
{
  if (!renderer || !pass || !view ||
      pass->wireframe_pipeline == LDK_RHI_INVALID_RESOURCE ||
      renderer->submitted_wireframe_count == 0)
  {
    return;
  }

  LDKRendererFrameDomainStats *stats =
      s_renderer_frame_stats_for_view(renderer, view);
  bool bound = false;

  for (u32 i = 0; i < renderer->submitted_wireframe_count; ++i)
  {
    const LDKRendererWireframeSubmit *submit =
        &renderer->submitted_wireframes[i];
    if (submit->view_id != view->id)
    {
      continue;
    }

    LDKRendererMeshResource *mesh =
        s_renderer_mesh_get_resource(renderer, submit->mesh);
    if (!mesh || mesh->index_count == 0)
    {
      continue;
    }

    if (!bound)
    {
      ldk_rhi_pipeline_bind(pass->rhi, pass->wireframe_pipeline);
      ldk_rhi_bindings_bind(pass->rhi, pass->bindings);
      bound = true;
    }

    LDKRendererMeshObjectParams object = {0};
    object.world = submit->world;
    object.instance_color = ldk_renderer_color_from_rgba32(0xffffffffu);
    object.options[0] =
        (submit->flags & LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD)
        ? 1.0f : 0.0f;
    LDKRendererMeshMaterialParams material = {0};
    material.color = ldk_renderer_color_from_rgba32(submit->color);
    ldk_rhi_buffer_update(pass->rhi, pass->object_buffer, 0,
        sizeof(object), &object);
    ldk_rhi_buffer_update(pass->rhi, pass->material_buffer, 0,
        sizeof(material), &material);
    ldk_rhi_vertex_buffer_bind(pass->rhi, mesh->vertex_buffer, 0);
    ldk_rhi_index_buffer_bind(pass->rhi, mesh->index_buffer, 0,
        LDK_RHI_INDEX_TYPE_UINT32);

    ldk_rhi_line_width_set(pass->rhi, submit->line_width);

    LDKRHIDrawIndexedDesc draw = {0};
    draw.index_count = mesh->index_count;
    ldk_rhi_draw_indexed(pass->rhi, &draw);
    stats->draw_call_count += 1;
    stats->line_draw_call_count += 1;
  }

  if (bound)
  {
    ldk_rhi_line_width_set(pass->rhi, 1.0f);
  }
}

static void s_renderer_mesh_pass_draw(LDKRenderer* renderer,
    LDKRendererMeshPass* pass, LDKRendererView const* view, u32 flags)
{
  if (renderer == NULL || pass == NULL || !pass->is_initialized ||
      view == NULL || !view->submitted)
  {
    return;
  }

  LDKRendererMeshCameraParams camera_params = {0};
  Mat4 camera_world = mat4_inverse_affine(view->view);
  Vec3 camera_position =
      mat4_mul_point(camera_world, vec3_make(0.0f, 0.0f, 0.0f));
  /* This second overlay camera is strictly orthographic. Editor handle
   * coordinates are viewport pixels, so neither scene projection nor zoom
   * can change their on-screen size. */
  if ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE) != 0)
  {
    camera_params.view = mat4_identity();
    camera_params.projection = mat4_orthographic_rh_no(
        0.0f, (float)view->width, 0.0f, (float)view->height,
        -4096.0f, 4096.0f);
    camera_position = vec3_make(0.0f, 0.0f, 4096.0f);
  }
  else
  {
    camera_params.view = view->view;
    camera_params.projection = view->projection;
  }
  camera_params.camera_position[0] = camera_position.x;
  camera_params.camera_position[1] = camera_position.y;
  camera_params.camera_position[2] = camera_position.z;
  camera_params.camera_position[3] = renderer->animation_time_seconds;
  ldk_rhi_buffer_update(pass->rhi, pass->camera_buffer, 0,
      sizeof(camera_params), &camera_params);

  s_renderer_mesh_pass_draw_submissions(renderer, pass, view, flags);
  if ((flags & LDK_RENDERER_MESH_SUBMIT_FLAG_SCREEN_SPACE) == 0)
  {
    s_renderer_lines_draw(renderer, pass, view, flags);
  }
}
