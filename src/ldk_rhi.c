#include "ldk_rhi.h"

#include <string.h>

static bool ldk_rhi_has_backend(const LDKRHIContext* context)
{
  if (context == NULL)
  {
    return false;
  }

  return context->backend_type != LDK_RHI_BACKEND_NONE;
}

void ldk_rhi_buffer_desc_defaults(LDKRHIBufferDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->memory_usage = LDK_RHI_MEMORY_USAGE_GPU;
}

void ldk_rhi_texture_desc_defaults(LDKRHITextureDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->type = LDK_RHI_TEXTURE_TYPE_2D;
  desc->depth = 1;
  desc->mip_count = 1;
  desc->layer_count = 1;
  desc->usage = LDK_RHI_TEXTURE_USAGE_SAMPLED;
  desc->swizzle_r = LDK_RHI_TEXTURE_SWIZZLE_R;
  desc->swizzle_g = LDK_RHI_TEXTURE_SWIZZLE_G;
  desc->swizzle_b = LDK_RHI_TEXTURE_SWIZZLE_B;
  desc->swizzle_a = LDK_RHI_TEXTURE_SWIZZLE_A;
}

void ldk_rhi_sampler_desc_defaults(LDKRHISamplerDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->min_filter = LDK_RHI_FILTER_LINEAR;
  desc->mag_filter = LDK_RHI_FILTER_LINEAR;
  desc->mip_filter = LDK_RHI_FILTER_LINEAR;
  desc->wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc->wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  desc->wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;
}

void ldk_rhi_shader_module_desc_defaults(LDKRHIShaderModuleDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
}

void ldk_rhi_shader_desc_defaults(LDKRHIShaderDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->source_type = LDK_RHI_SHADER_SOURCE_TEXT;
}

void ldk_rhi_blend_state_defaults(LDKRHIBlendState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->src_color_factor = LDK_RHI_BLEND_FACTOR_ONE;
  state->dst_color_factor = LDK_RHI_BLEND_FACTOR_ZERO;
  state->color_op = LDK_RHI_BLEND_OP_ADD;
  state->src_alpha_factor = LDK_RHI_BLEND_FACTOR_ONE;
  state->dst_alpha_factor = LDK_RHI_BLEND_FACTOR_ZERO;
  state->alpha_op = LDK_RHI_BLEND_OP_ADD;
}

void ldk_rhi_depth_state_defaults(LDKRHIDepthState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->test_enabled = true;
  state->write_enabled = true;
  state->compare_op = LDK_RHI_COMPARE_OP_LESS_EQUAL;
}

void ldk_rhi_raster_state_defaults(LDKRHIRasterState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->cull_mode = LDK_RHI_CULL_MODE_BACK;
  state->front_face = LDK_RHI_FRONT_FACE_CCW;
}

void ldk_rhi_pipeline_desc_defaults(LDKRHIPipelineDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->topology = LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES;
  ldk_rhi_blend_state_defaults(&desc->blend_state);
  ldk_rhi_depth_state_defaults(&desc->depth_state);
  ldk_rhi_raster_state_defaults(&desc->raster_state);
}

void ldk_rhi_bindings_layout_desc_defaults(LDKRHIBindingsLayoutDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
}

void ldk_rhi_bindings_desc_defaults(LDKRHIBindingsDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
}

void ldk_rhi_pass_desc_defaults(LDKRHIPassDesc* desc)
{
  if (desc == NULL)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  desc->depth_attachment.clear_depth = 1.0f;
  desc->viewport.min_depth = 0.0f;
  desc->viewport.max_depth = 1.0f;
}

bool ldk_rhi_initialize(LDKRHIContext* context, const LDKRHIContextDesc* desc, const LDKRHIFunctions* functions)
{
  if (context == NULL || desc == NULL || functions == NULL)
  {
    return false;
  }

  memset(context, 0, sizeof(*context));
  context->backend_type = desc->backend_type;
  context->backend_user_data = desc->backend_user_data;
  context->functions = *functions;
  return true;
}

void ldk_rhi_terminate(LDKRHIContext* context)
{
  if (context == NULL)
  {
    return;
  }

  if (context->functions.shutdown != NULL)
  {
    context->functions.shutdown(context->backend_user_data);
  }

  memset(context, 0, sizeof(*context));
}

bool ldk_rhi_is_valid_buffer(LDKRHIBuffer buffer)
{
  return buffer != LDK_RHI_INVALID_BUFFER;
}

bool ldk_rhi_is_valid_texture(LDKRHITexture texture)
{
  return texture != LDK_RHI_INVALID_TEXTURE;
}

bool ldk_rhi_is_valid_sampler(LDKRHISampler sampler)
{
  return sampler != LDK_RHI_INVALID_SAMPLER;
}

bool ldk_rhi_is_valid_shader_module(LDKRHIShaderModule shader_module)
{
  return shader_module != LDK_RHI_INVALID_SHADER_MODULE;
}

bool ldk_rhi_is_valid_shader(LDKRHIShader shader)
{
  return ldk_rhi_is_valid_shader_module(shader);
}

bool ldk_rhi_is_valid_bindings_layout(LDKRHIBindingsLayout bindings_layout)
{
  return bindings_layout != LDK_RHI_INVALID_BINDINGS_LAYOUT;
}

bool ldk_rhi_is_valid_pipeline(LDKRHIPipeline pipeline)
{
  return pipeline != LDK_RHI_INVALID_PIPELINE;
}

bool ldk_rhi_is_valid_bindings(LDKRHIBindings bindings)
{
  return bindings != LDK_RHI_INVALID_BINDINGS;
}

bool ldk_rhi_is_valid_buffer_desc(const LDKRHIBufferDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->size == 0)
  {
    return false;
  }

  if (desc->usage == LDK_RHI_BUFFER_USAGE_NONE)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_texture_desc(const LDKRHITextureDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->format == LDK_RHI_FORMAT_INVALID)
  {
    return false;
  }

  if (desc->width == 0 || desc->height == 0 || desc->depth == 0)
  {
    return false;
  }

  if (desc->mip_count == 0 || desc->layer_count == 0)
  {
    return false;
  }

  if (desc->usage == LDK_RHI_TEXTURE_USAGE_NONE)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_sampler_desc(const LDKRHISamplerDesc* desc)
{
  return desc != NULL;
}

bool ldk_rhi_is_valid_shader_module_desc(const LDKRHIShaderModuleDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->stage == LDK_RHI_SHADER_STAGE_NONE)
  {
    return false;
  }

  if (desc->code == NULL || desc->code_size == 0)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_shader_desc(const LDKRHIShaderDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->stage == LDK_RHI_SHADER_STAGE_NONE)
  {
    return false;
  }

  if (desc->data == NULL || desc->size == 0)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_bindings_layout_desc(const LDKRHIBindingsLayoutDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->entry_count > LDK_RHI_BINDING_MAX)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_pipeline_desc(const LDKRHIPipelineDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (!ldk_rhi_is_valid_shader_module(desc->vertex_shader_module))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_shader_module(desc->fragment_shader_module))
  {
    return false;
  }

  if (desc->vertex_layout.attribute_count > LDK_RHI_VERTEX_ATTRIBUTE_MAX)
  {
    return false;
  }

  if (desc->color_attachment_count > LDK_RHI_COLOR_ATTACHMENT_MAX)
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_bindings_desc(const LDKRHIBindingsDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (!ldk_rhi_is_valid_bindings_layout(desc->layout))
  {
    return false;
  }

  if (desc->binding_count > LDK_RHI_BINDING_MAX)
  {
    return false;
  }

  for (uint32_t i = 0; i < desc->binding_count; i++)
  {
    const LDKRHIBindingDesc* binding = &desc->bindings[i];

    if (binding->slot >= LDK_RHI_BINDING_MAX)
    {
      return false;
    }

    if (!ldk_rhi_is_valid_buffer(binding->buffer) &&
        !ldk_rhi_is_valid_texture(binding->texture) &&
        !ldk_rhi_is_valid_sampler(binding->sampler))
    {
      return false;
    }
  }

  return true;
}

bool ldk_rhi_is_valid_pass_desc(const LDKRHIPassDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (desc->color_attachment_count > LDK_RHI_COLOR_ATTACHMENT_MAX)
  {
    return false;
  }

  if (desc->color_attachment_count == 0 && !desc->depth_attachment.valid)
  {
    return false;
  }

  for (uint32_t i = 0; i < desc->color_attachment_count; i++)
  {
    LDKRHITexture texture = desc->color_attachments[i].texture;
    bool default_backbuffer = desc->color_attachment_count == 1 && texture == LDK_RHI_INVALID_TEXTURE;
    if (!default_backbuffer && !ldk_rhi_is_valid_texture(texture))
    {
      return false;
    }
  }

  if (desc->depth_attachment.valid && !ldk_rhi_is_valid_texture(desc->depth_attachment.texture))
  {
    return false;
  }

  return true;
}

LDKRHIBuffer ldk_rhi_create_buffer(LDKRHIContext* context, const LDKRHIBufferDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_buffer_desc(desc) || context->functions.create_buffer == NULL)
  {
    return LDK_RHI_INVALID_BUFFER;
  }

  return context->functions.create_buffer(context->backend_user_data, desc);
}

void ldk_rhi_destroy_buffer(LDKRHIContext* context, LDKRHIBuffer buffer)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_buffer(buffer) || context->functions.destroy_buffer == NULL)
  {
    return;
  }

  context->functions.destroy_buffer(context->backend_user_data, buffer);
}

bool ldk_rhi_update_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_buffer(buffer) || size == 0 || data == NULL || context->functions.update_buffer == NULL)
  {
    return false;
  }

  return context->functions.update_buffer(context->backend_user_data, buffer, offset, size, data);
}

LDKRHITexture ldk_rhi_create_texture(LDKRHIContext* context, const LDKRHITextureDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_texture_desc(desc) || context->functions.create_texture == NULL)
  {
    return LDK_RHI_INVALID_TEXTURE;
  }

  return context->functions.create_texture(context->backend_user_data, desc);
}

void ldk_rhi_destroy_texture(LDKRHIContext* context, LDKRHITexture texture)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_texture(texture) || context->functions.destroy_texture == NULL)
  {
    return;
  }

  context->functions.destroy_texture(context->backend_user_data, texture);
}

bool ldk_rhi_update_texture(LDKRHIContext* context, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_texture(texture) || size == 0 || data == NULL || context->functions.update_texture == NULL)
  {
    return false;
  }

  return context->functions.update_texture(context->backend_user_data, texture, mip_level, layer, data, size);
}

LDKRHISampler ldk_rhi_create_sampler(LDKRHIContext* context, const LDKRHISamplerDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_sampler_desc(desc) || context->functions.create_sampler == NULL)
  {
    return LDK_RHI_INVALID_SAMPLER;
  }

  return context->functions.create_sampler(context->backend_user_data, desc);
}

void ldk_rhi_destroy_sampler(LDKRHIContext* context, LDKRHISampler sampler)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_sampler(sampler) || context->functions.destroy_sampler == NULL)
  {
    return;
  }

  context->functions.destroy_sampler(context->backend_user_data, sampler);
}

LDKRHIShaderModule ldk_rhi_create_shader_module(LDKRHIContext* context, const LDKRHIShaderModuleDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_shader_module_desc(desc) || context->functions.create_shader_module == NULL)
  {
    return LDK_RHI_INVALID_SHADER_MODULE;
  }

  return context->functions.create_shader_module(context->backend_user_data, desc);
}

void ldk_rhi_destroy_shader_module(LDKRHIContext* context, LDKRHIShaderModule shader_module)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_shader_module(shader_module) || context->functions.destroy_shader_module == NULL)
  {
    return;
  }

  context->functions.destroy_shader_module(context->backend_user_data, shader_module);
}

LDKRHIShader ldk_rhi_create_shader(LDKRHIContext* context, const LDKRHIShaderDesc* desc)
{
  LDKRHIShaderModuleDesc module_desc = {0};

  if (!ldk_rhi_is_valid_shader_desc(desc))
  {
    return LDK_RHI_INVALID_SHADER;
  }

  ldk_rhi_shader_module_desc_defaults(&module_desc);
  module_desc.stage = desc->stage;
  module_desc.code_format = (LDKRHIShaderCodeFormat)desc->source_type;
  module_desc.code = desc->data;
  module_desc.code_size = desc->size;
  return ldk_rhi_create_shader_module(context, &module_desc);
}

void ldk_rhi_destroy_shader(LDKRHIContext* context, LDKRHIShader shader)
{
  ldk_rhi_destroy_shader_module(context, shader);
}

LDKRHIBindingsLayout ldk_rhi_create_bindings_layout(LDKRHIContext* context, const LDKRHIBindingsLayoutDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings_layout_desc(desc) || context->functions.create_bindings_layout == NULL)
  {
    return LDK_RHI_INVALID_BINDINGS_LAYOUT;
  }

  return context->functions.create_bindings_layout(context->backend_user_data, desc);
}

void ldk_rhi_destroy_bindings_layout(LDKRHIContext* context, LDKRHIBindingsLayout bindings_layout)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings_layout(bindings_layout) || context->functions.destroy_bindings_layout == NULL)
  {
    return;
  }

  context->functions.destroy_bindings_layout(context->backend_user_data, bindings_layout);
}

LDKRHIPipeline ldk_rhi_create_pipeline(LDKRHIContext* context, const LDKRHIPipelineDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_pipeline_desc(desc) || context->functions.create_pipeline == NULL)
  {
    return LDK_RHI_INVALID_PIPELINE;
  }

  return context->functions.create_pipeline(context->backend_user_data, desc);
}

void ldk_rhi_destroy_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_pipeline(pipeline) || context->functions.destroy_pipeline == NULL)
  {
    return;
  }

  context->functions.destroy_pipeline(context->backend_user_data, pipeline);
}

LDKRHIBindings ldk_rhi_create_bindings(LDKRHIContext* context, const LDKRHIBindingsDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings_desc(desc) || context->functions.create_bindings == NULL)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  return context->functions.create_bindings(context->backend_user_data, desc);
}

void ldk_rhi_destroy_bindings(LDKRHIContext* context, LDKRHIBindings bindings)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings(bindings) || context->functions.destroy_bindings == NULL)
  {
    return;
  }

  context->functions.destroy_bindings(context->backend_user_data, bindings);
}

void ldk_rhi_begin_frame(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && context->functions.begin_frame != NULL)
  {
    context->functions.begin_frame(context->backend_user_data);
  }
}

void ldk_rhi_end_frame(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && context->functions.end_frame != NULL)
  {
    context->functions.end_frame(context->backend_user_data);
  }
}

void ldk_rhi_begin_pass(LDKRHIContext* context, const LDKRHIPassDesc* desc)
{
  if (ldk_rhi_has_backend(context) && ldk_rhi_is_valid_pass_desc(desc) && context->functions.begin_pass != NULL)
  {
    context->functions.begin_pass(context->backend_user_data, desc);
  }
}

void ldk_rhi_end_pass(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && context->functions.end_pass != NULL)
  {
    context->functions.end_pass(context->backend_user_data);
  }
}

void ldk_rhi_bind_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline)
{
  if (ldk_rhi_has_backend(context) && ldk_rhi_is_valid_pipeline(pipeline) && context->functions.bind_pipeline != NULL)
  {
    context->functions.bind_pipeline(context->backend_user_data, pipeline);
  }
}

void ldk_rhi_bind_bindings(LDKRHIContext* context, LDKRHIBindings bindings)
{
  if (ldk_rhi_has_backend(context) && ldk_rhi_is_valid_bindings(bindings) && context->functions.bind_bindings != NULL)
  {
    context->functions.bind_bindings(context->backend_user_data, bindings);
  }
}

void ldk_rhi_bind_vertex_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset)
{
  if (ldk_rhi_has_backend(context) && ldk_rhi_is_valid_buffer(buffer) && context->functions.bind_vertex_buffer != NULL)
  {
    context->functions.bind_vertex_buffer(context->backend_user_data, buffer, offset);
  }
}

void ldk_rhi_bind_index_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type)
{
  if (ldk_rhi_has_backend(context) && ldk_rhi_is_valid_buffer(buffer) && context->functions.bind_index_buffer != NULL)
  {
    context->functions.bind_index_buffer(context->backend_user_data, buffer, offset, index_type);
  }
}

void ldk_rhi_set_viewport(LDKRHIContext* context, const LDKRHIViewport* viewport)
{
  if (ldk_rhi_has_backend(context) && viewport != NULL && context->functions.set_viewport != NULL)
  {
    context->functions.set_viewport(context->backend_user_data, viewport);
  }
}

void ldk_rhi_set_scissor(LDKRHIContext* context, const LDKRHIRect* scissor)
{
  if (ldk_rhi_has_backend(context) && scissor != NULL && context->functions.set_scissor != NULL)
  {
    context->functions.set_scissor(context->backend_user_data, scissor);
  }
}

void ldk_rhi_draw(LDKRHIContext* context, const LDKRHIDrawDesc* desc)
{
  if (ldk_rhi_has_backend(context) && desc != NULL && context->functions.draw != NULL)
  {
    context->functions.draw(context->backend_user_data, desc);
  }
}

void ldk_rhi_draw_indexed(LDKRHIContext* context, const LDKRHIDrawIndexedDesc* desc)
{
  if (ldk_rhi_has_backend(context) && desc != NULL && context->functions.draw_indexed != NULL)
  {
    context->functions.draw_indexed(context->backend_user_data, desc);
  }
}
