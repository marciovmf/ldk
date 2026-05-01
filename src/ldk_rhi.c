#include <module/ldk_rhi.h>
#include <stdlib.h>
#include <string.h>

typedef enum LDKRHIDeferredDeleteType
{
  LDK_RHI_DEFERRED_DELETE_BUFFER,
  LDK_RHI_DEFERRED_DELETE_TEXTURE,
  LDK_RHI_DEFERRED_DELETE_SAMPLER,
  LDK_RHI_DEFERRED_DELETE_SHADER_MODULE,
  LDK_RHI_DEFERRED_DELETE_BINDINGS_LAYOUT,
  LDK_RHI_DEFERRED_DELETE_PIPELINE,
  LDK_RHI_DEFERRED_DELETE_BINDINGS
}
LDKRHIDeferredDeleteType;

typedef struct LDKRHIDeferredDelete
{
  LDKRHIDeferredDeleteType type;
  uint32_t handle;
  uint64_t frame_index;
}
LDKRHIDeferredDelete;

#ifndef LDK_RHI_DEFERRED_DELETE_FRAME_DELAY 
#define LDK_RHI_DEFERRED_DELETE_FRAME_DELAY 3
#endif

static bool ldk_rhi_is_deferred_delete_ready(const LDKRHIContext* context, const LDKRHIDeferredDelete* deferred_delete)
{
  if (context == NULL || deferred_delete == NULL)
  {
    return false;
  }

  return context->frame_index >= deferred_delete->frame_index + LDK_RHI_DEFERRED_DELETE_FRAME_DELAY;
}

static void ldk_rhi_destroy_deferred_now(LDKRHIContext* context, const LDKRHIDeferredDelete* deferred_delete)
{
  if (context == NULL || deferred_delete == NULL)
  {
    return;
  }

  switch (deferred_delete->type)
  {
    case LDK_RHI_DEFERRED_DELETE_BUFFER:
      {
        if (context->functions.destroy_buffer != NULL)
        {
          context->functions.destroy_buffer(context->backend_user_data, (LDKRHIBuffer)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_TEXTURE:
      {
        if (context->functions.destroy_texture != NULL)
        {
          context->functions.destroy_texture(context->backend_user_data, (LDKRHITexture)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_SAMPLER:
      {
        if (context->functions.destroy_sampler != NULL)
        {
          context->functions.destroy_sampler(context->backend_user_data, (LDKRHISampler)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_SHADER_MODULE:
      {
        if (context->functions.destroy_shader_module != NULL)
        {
          context->functions.destroy_shader_module(context->backend_user_data, (LDKRHIShaderModule)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_BINDINGS_LAYOUT:
      {
        if (context->functions.destroy_bindings_layout != NULL)
        {
          context->functions.destroy_bindings_layout(context->backend_user_data, (LDKRHIBindingsLayout)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_PIPELINE:
      {
        if (context->functions.destroy_pipeline != NULL)
        {
          context->functions.destroy_pipeline(context->backend_user_data, (LDKRHIPipeline)deferred_delete->handle);
        }
      } break;

    case LDK_RHI_DEFERRED_DELETE_BINDINGS:
      {
        if (context->functions.destroy_bindings != NULL)
        {
          context->functions.destroy_bindings(context->backend_user_data, (LDKRHIBindings)deferred_delete->handle);
        }
      } break;
  }
}

static bool ldk_rhi_is_deferred_delete_pending(const LDKRHIContext* context, LDKRHIDeferredDeleteType type, uint32_t handle)
{
  if (context == NULL || handle == 0)
  {
    return false;
  }

  LDKRHIDeferredDelete* deferred_deletes = (LDKRHIDeferredDelete*)context->deferred_deletes;

  for (uint32_t i = 0; i < context->deferred_delete_count; i++)
  {
    if (deferred_deletes[i].type == type && deferred_deletes[i].handle == handle)
    {
      return true;
    }
  }

  return false;
}

static bool ldk_rhi_enqueue_deferred_delete(LDKRHIContext* context, LDKRHIDeferredDeleteType type, uint32_t handle)
{
  if (context == NULL || handle == 0)
  {
    return false;
  }

  if (ldk_rhi_is_deferred_delete_pending(context, type, handle))
  {
    return true;
  }

  if (context->deferred_delete_count == context->deferred_delete_capacity)
  {
    uint32_t new_capacity = context->deferred_delete_capacity == 0 ? 64 : context->deferred_delete_capacity * 2;
    void* new_deferred_deletes = realloc(context->deferred_deletes, sizeof(LDKRHIDeferredDelete) * new_capacity);

    if (new_deferred_deletes == NULL)
    {
      return false;
    }

    context->deferred_deletes = new_deferred_deletes;
    context->deferred_delete_capacity = new_capacity;
  }

  LDKRHIDeferredDelete* deferred_deletes = (LDKRHIDeferredDelete*)context->deferred_deletes;
  deferred_deletes[context->deferred_delete_count].type = type;
  deferred_deletes[context->deferred_delete_count].handle = handle;
  deferred_deletes[context->deferred_delete_count].frame_index = context->frame_index;
  context->deferred_delete_count++;
  return true;
}

static void ldk_rhi_collect_deferred_deletes(LDKRHIContext* context, bool force)
{
  if (context == NULL || context->deferred_delete_count == 0)
  {
    return;
  }

  LDKRHIDeferredDelete* deferred_deletes = (LDKRHIDeferredDelete*)context->deferred_deletes;
  uint32_t write_index = 0;

  for (uint32_t read_index = 0; read_index < context->deferred_delete_count; read_index++)
  {
    if (force || ldk_rhi_is_deferred_delete_ready(context, &deferred_deletes[read_index]))
    {
      ldk_rhi_destroy_deferred_now(context, &deferred_deletes[read_index]);
      continue;
    }

    if (write_index != read_index)
    {
      deferred_deletes[write_index] = deferred_deletes[read_index];
    }

    write_index++;
  }

  context->deferred_delete_count = write_index;
}

static void ldk_rhi_reset_bound_state(LDKRHIContext* context)
{
  if (context == NULL)
  {
    return;
  }

  context->bound_pipeline = LDK_RHI_INVALID_PIPELINE;
  context->bound_bindings = LDK_RHI_INVALID_BINDINGS;
  context->bound_vertex_buffer = LDK_RHI_INVALID_BUFFER;
  context->bound_index_buffer = LDK_RHI_INVALID_BUFFER;
}

static bool ldk_rhi_is_valid_buffer_usage(uint32_t usage)
{
  const uint32_t valid_bits = LDK_RHI_BUFFER_USAGE_VERTEX |
    LDK_RHI_BUFFER_USAGE_INDEX |
    LDK_RHI_BUFFER_USAGE_UNIFORM |
    LDK_RHI_BUFFER_USAGE_STORAGE |
    LDK_RHI_BUFFER_USAGE_TRANSFER_SRC |
    LDK_RHI_BUFFER_USAGE_TRANSFER_DST;

  return usage != LDK_RHI_BUFFER_USAGE_NONE && (usage & ~valid_bits) == 0;
}

static bool ldk_rhi_is_valid_texture_usage(uint32_t usage)
{
  const uint32_t valid_bits = LDK_RHI_TEXTURE_USAGE_SAMPLED |
    LDK_RHI_TEXTURE_USAGE_RENDER_TARGET |
    LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL |
    LDK_RHI_TEXTURE_USAGE_STORAGE |
    LDK_RHI_TEXTURE_USAGE_TRANSFER_SRC |
    LDK_RHI_TEXTURE_USAGE_TRANSFER_DST;

  return usage != LDK_RHI_TEXTURE_USAGE_NONE && (usage & ~valid_bits) == 0;
}

static bool ldk_rhi_is_valid_shader_stage_mask(uint32_t stages)
{
  const uint32_t valid_bits = LDK_RHI_SHADER_STAGE_VERTEX |
    LDK_RHI_SHADER_STAGE_FRAGMENT |
    LDK_RHI_SHADER_STAGE_COMPUTE;

  return stages != LDK_RHI_SHADER_STAGE_NONE && (stages & ~valid_bits) == 0;
}

static bool ldk_rhi_is_valid_shader_stage_single(uint32_t stage)
{
  return stage == LDK_RHI_SHADER_STAGE_VERTEX ||
    stage == LDK_RHI_SHADER_STAGE_FRAGMENT ||
    stage == LDK_RHI_SHADER_STAGE_COMPUTE;
}

static bool ldk_rhi_is_valid_format(LDKRHIFormat format)
{
  return format > LDK_RHI_FORMAT_INVALID && format <= LDK_RHI_FORMAT_D32_FLOAT;
}

static bool ldk_rhi_is_valid_texture_type(LDKRHITextureType type)
{
  return type >= LDK_RHI_TEXTURE_TYPE_2D && type <= LDK_RHI_TEXTURE_TYPE_CUBE;
}

static bool ldk_rhi_is_valid_texture_swizzle(LDKRHITextureSwizzle swizzle)
{
  return swizzle >= LDK_RHI_TEXTURE_SWIZZLE_IDENTITY && swizzle <= LDK_RHI_TEXTURE_SWIZZLE_A;
}

static bool ldk_rhi_is_valid_filter(LDKRHIFilter filter)
{
  return filter >= LDK_RHI_FILTER_NEAREST && filter <= LDK_RHI_FILTER_LINEAR;
}

static bool ldk_rhi_is_valid_wrap(LDKRHIWrap wrap)
{
  return wrap >= LDK_RHI_WRAP_REPEAT && wrap <= LDK_RHI_WRAP_MIRROR;
}

static bool ldk_rhi_is_valid_shader_code_format(LDKRHIShaderCodeFormat format)
{
  return format >= LDK_RHI_SHADER_CODE_FORMAT_GLSL && format <= LDK_RHI_SHADER_CODE_FORMAT_MSL;
}

static bool ldk_rhi_is_valid_primitive_topology(LDKRHIPrimitiveTopology topology)
{
  return topology >= LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES && topology <= LDK_RHI_PRIMITIVE_TOPOLOGY_LINES;
}

static bool ldk_rhi_is_valid_cull_mode(LDKRHICullMode cull_mode)
{
  return cull_mode >= LDK_RHI_CULL_MODE_NONE && cull_mode <= LDK_RHI_CULL_MODE_BACK;
}

static bool ldk_rhi_is_valid_front_face(LDKRHIFrontFace front_face)
{
  return front_face >= LDK_RHI_FRONT_FACE_CCW && front_face <= LDK_RHI_FRONT_FACE_CW;
}

static bool ldk_rhi_is_valid_compare_op(LDKRHICompareOp compare_op)
{
  return compare_op >= LDK_RHI_COMPARE_OP_NEVER && compare_op <= LDK_RHI_COMPARE_OP_ALWAYS;
}

static bool ldk_rhi_is_valid_blend_factor(LDKRHIBlendFactor factor)
{
  return factor >= LDK_RHI_BLEND_FACTOR_ZERO && factor <= LDK_RHI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
}

static bool ldk_rhi_is_valid_blend_op(LDKRHIBlendOp op)
{
  return op >= LDK_RHI_BLEND_OP_ADD && op <= LDK_RHI_BLEND_OP_REVERSE_SUBTRACT;
}

static bool ldk_rhi_is_valid_color_write_mask(uint32_t mask)
{
  return (mask & ~LDK_RHI_COLOR_WRITE_MASK_ALL) == 0;
}

static bool ldk_rhi_is_valid_vertex_format(LDKRHIVertexFormat format)
{
  return format >= LDK_RHI_VERTEX_FORMAT_FLOAT && format <= LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM;
}

static bool ldk_rhi_is_valid_binding_type(LDKRHIBindingType type)
{
  return type >= LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER && type <= LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER;
}

static bool ldk_rhi_is_valid_load_op(LDKRHILoadOp op)
{
  return op >= LDK_RHI_LOAD_OP_LOAD && op <= LDK_RHI_LOAD_OP_DONT_CARE;
}

static bool ldk_rhi_is_valid_store_op(LDKRHIStoreOp op)
{
  return op >= LDK_RHI_STORE_OP_STORE && op <= LDK_RHI_STORE_OP_DONT_CARE;
}

static bool ldk_rhi_is_valid_index_type(LDKRHIIndexType index_type)
{
  return index_type >= LDK_RHI_INDEX_TYPE_UINT16 && index_type <= LDK_RHI_INDEX_TYPE_UINT32;
}

static bool ldk_rhi_is_valid_blend_state(const LDKRHIBlendState* state)
{
  if (state == NULL)
  {
    return false;
  }

  if (!ldk_rhi_is_valid_blend_factor(state->src_color_factor) ||
      !ldk_rhi_is_valid_blend_factor(state->dst_color_factor) ||
      !ldk_rhi_is_valid_blend_op(state->color_op) ||
      !ldk_rhi_is_valid_blend_factor(state->src_alpha_factor) ||
      !ldk_rhi_is_valid_blend_factor(state->dst_alpha_factor) ||
      !ldk_rhi_is_valid_blend_op(state->alpha_op) ||
      !ldk_rhi_is_valid_color_write_mask(state->color_write_mask))
  {
    return false;
  }

  return true;
}

static bool ldk_rhi_is_valid_depth_state(const LDKRHIDepthState* state)
{
  if (state == NULL)
  {
    return false;
  }

  return ldk_rhi_is_valid_compare_op(state->compare_op);
}

static bool ldk_rhi_is_valid_raster_state(const LDKRHIRasterState* state)
{
  if (state == NULL)
  {
    return false;
  }

  return ldk_rhi_is_valid_cull_mode(state->cull_mode) && ldk_rhi_is_valid_front_face(state->front_face);
}

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
  state->color_write_mask = LDK_RHI_COLOR_WRITE_MASK_ALL;
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

  ldk_rhi_collect_deferred_deletes(context, true);
  free(context->deferred_deletes);
  context->deferred_deletes = NULL;
  context->deferred_delete_count = 0;
  context->deferred_delete_capacity = 0;

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

  if (!ldk_rhi_is_valid_buffer_usage(desc->usage))
  {
    return false;
  }

  if (desc->memory_usage < LDK_RHI_MEMORY_USAGE_GPU || desc->memory_usage > LDK_RHI_MEMORY_USAGE_GPU_TO_CPU)
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

  if (!ldk_rhi_is_valid_texture_type(desc->type))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_format(desc->format))
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

  if (!ldk_rhi_is_valid_texture_usage(desc->usage))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_texture_swizzle(desc->swizzle_r) ||
      !ldk_rhi_is_valid_texture_swizzle(desc->swizzle_g) ||
      !ldk_rhi_is_valid_texture_swizzle(desc->swizzle_b) ||
      !ldk_rhi_is_valid_texture_swizzle(desc->swizzle_a))
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_sampler_desc(const LDKRHISamplerDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (!ldk_rhi_is_valid_filter(desc->min_filter) ||
      !ldk_rhi_is_valid_filter(desc->mag_filter) ||
      !ldk_rhi_is_valid_filter(desc->mip_filter) ||
      !ldk_rhi_is_valid_wrap(desc->wrap_u) ||
      !ldk_rhi_is_valid_wrap(desc->wrap_v) ||
      !ldk_rhi_is_valid_wrap(desc->wrap_w))
  {
    return false;
  }

  return true;
}

bool ldk_rhi_is_valid_shader_module_desc(const LDKRHIShaderModuleDesc* desc)
{
  if (desc == NULL)
  {
    return false;
  }

  if (!ldk_rhi_is_valid_shader_stage_single(desc->stage))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_shader_code_format(desc->code_format))
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

  if (!ldk_rhi_is_valid_shader_stage_single(desc->stage))
  {
    return false;
  }

  if (desc->source_type < LDK_RHI_SHADER_SOURCE_TEXT || desc->source_type > LDK_RHI_SHADER_SOURCE_BINARY)
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

  for (uint32_t i = 0; i < desc->entry_count; i++)
  {
    const LDKRHIBindingLayoutEntry* entry = &desc->entries[i];

    if (entry->slot >= LDK_RHI_BINDING_MAX)
    {
      return false;
    }

    if (!ldk_rhi_is_valid_binding_type(entry->type))
    {
      return false;
    }

    if (!ldk_rhi_is_valid_shader_stage_mask(entry->stages))
    {
      return false;
    }

    for (uint32_t j = i + 1; j < desc->entry_count; j++)
    {
      if (entry->slot == desc->entries[j].slot)
      {
        return false;
      }
    }
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

  if (!ldk_rhi_is_valid_primitive_topology(desc->topology))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_blend_state(&desc->blend_state))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_depth_state(&desc->depth_state))
  {
    return false;
  }

  if (!ldk_rhi_is_valid_raster_state(&desc->raster_state))
  {
    return false;
  }

  if (desc->color_attachment_count > 0 && desc->blend_state.color_write_mask == LDK_RHI_COLOR_WRITE_MASK_NONE)
  {
    return false;
  }

  if (desc->vertex_layout.attribute_count > LDK_RHI_VERTEX_ATTRIBUTE_MAX)
  {
    return false;
  }

  if (desc->vertex_layout.attribute_count > 0 && desc->vertex_layout.stride == 0)
  {
    return false;
  }

  for (uint32_t i = 0; i < desc->vertex_layout.attribute_count; i++)
  {
    const LDKRHIVertexAttributeDesc* attribute = &desc->vertex_layout.attributes[i];

    if (attribute->location >= LDK_RHI_VERTEX_ATTRIBUTE_MAX)
    {
      return false;
    }

    if (!ldk_rhi_is_valid_vertex_format(attribute->format))
    {
      return false;
    }

    for (uint32_t j = i + 1; j < desc->vertex_layout.attribute_count; j++)
    {
      if (attribute->location == desc->vertex_layout.attributes[j].location)
      {
        return false;
      }
    }
  }

  if (desc->color_attachment_count > LDK_RHI_COLOR_ATTACHMENT_MAX)
  {
    return false;
  }

  for (uint32_t i = 0; i < desc->color_attachment_count; i++)
  {
    if (!ldk_rhi_is_valid_format(desc->color_formats[i]))
    {
      return false;
    }
  }

  if (desc->depth_format != LDK_RHI_FORMAT_INVALID && !ldk_rhi_is_valid_format(desc->depth_format))
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
    const LDKRHIPassColorAttachmentDesc* attachment = &desc->color_attachments[i];
    LDKRHITexture texture = attachment->texture;
    bool default_backbuffer = desc->color_attachment_count == 1 && texture == LDK_RHI_INVALID_TEXTURE;

    if (!default_backbuffer && !ldk_rhi_is_valid_texture(texture))
    {
      return false;
    }

    if (!ldk_rhi_is_valid_load_op(attachment->load_op))
    {
      return false;
    }

    if (!ldk_rhi_is_valid_store_op(attachment->store_op))
    {
      return false;
    }
  }

  if (desc->depth_attachment.valid)
  {
    if (!ldk_rhi_is_valid_texture(desc->depth_attachment.texture))
    {
      return false;
    }

    if (!ldk_rhi_is_valid_load_op(desc->depth_attachment.depth_load_op))
    {
      return false;
    }

    if (!ldk_rhi_is_valid_store_op(desc->depth_attachment.depth_store_op))
    {
      return false;
    }
  }

  if (desc->has_viewport)
  {
    if (desc->viewport.width <= 0.0f || desc->viewport.height <= 0.0f)
    {
      return false;
    }

    if (desc->viewport.min_depth > desc->viewport.max_depth)
    {
      return false;
    }
  }

  if (desc->has_scissor)
  {
    if (desc->scissor.width <= 0 || desc->scissor.height <= 0)
    {
      return false;
    }
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_BUFFER, buffer);
  if (context->bound_vertex_buffer == buffer)
  {
    context->bound_vertex_buffer = LDK_RHI_INVALID_BUFFER;
  }

  if (context->bound_index_buffer == buffer)
  {
    context->bound_index_buffer = LDK_RHI_INVALID_BUFFER;
  }
}

bool ldk_rhi_update_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_buffer(buffer) ||
      ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BUFFER, buffer) ||
      size == 0 || data == NULL || context->functions.update_buffer == NULL)
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_TEXTURE, texture);
}

bool ldk_rhi_update_texture(LDKRHIContext* context, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_texture(texture) ||
      ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_TEXTURE, texture) ||
      size == 0 || data == NULL || context->functions.update_texture == NULL)
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_SAMPLER, sampler);
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_SHADER_MODULE, shader_module);
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_BINDINGS_LAYOUT, bindings_layout);
}

LDKRHIPipeline ldk_rhi_create_pipeline(LDKRHIContext* context, const LDKRHIPipelineDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_pipeline_desc(desc) ||
      ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_SHADER_MODULE, desc->vertex_shader_module) ||
      ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_SHADER_MODULE, desc->fragment_shader_module) ||
      ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BINDINGS_LAYOUT, desc->bindings_layout) ||
      context->functions.create_pipeline == NULL)
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

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_PIPELINE, pipeline);
  if (context->bound_pipeline == pipeline)
  {
    context->bound_pipeline = LDK_RHI_INVALID_PIPELINE;
    context->bound_bindings = LDK_RHI_INVALID_BINDINGS;
  }
}

LDKRHIBindings ldk_rhi_create_bindings(LDKRHIContext* context, const LDKRHIBindingsDesc* desc)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings_desc(desc) || context->functions.create_bindings == NULL)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  if (ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BINDINGS_LAYOUT, desc->layout))
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  for (uint32_t i = 0; i < desc->binding_count; i++)
  {
    if (ldk_rhi_is_valid_buffer(desc->bindings[i].buffer) &&
        ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BUFFER, desc->bindings[i].buffer))
    {
      return LDK_RHI_INVALID_BINDINGS;
    }

    if (ldk_rhi_is_valid_texture(desc->bindings[i].texture) &&
        ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_TEXTURE, desc->bindings[i].texture))
    {
      return LDK_RHI_INVALID_BINDINGS;
    }

    if (ldk_rhi_is_valid_sampler(desc->bindings[i].sampler) &&
        ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_SAMPLER, desc->bindings[i].sampler))
    {
      return LDK_RHI_INVALID_BINDINGS;
    }
  }

  return context->functions.create_bindings(context->backend_user_data, desc);
}

void ldk_rhi_destroy_bindings(LDKRHIContext* context, LDKRHIBindings bindings)
{
  if (!ldk_rhi_has_backend(context) || !ldk_rhi_is_valid_bindings(bindings) || context->functions.destroy_bindings == NULL)
  {
    return;
  }

  ldk_rhi_enqueue_deferred_delete(context, LDK_RHI_DEFERRED_DELETE_BINDINGS, bindings);
  if (context->bound_bindings == bindings)
  {
    context->bound_bindings = LDK_RHI_INVALID_BINDINGS;
  }
}

void ldk_rhi_begin_frame(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && !context->frame_active && !context->pass_active)
  {
    ldk_rhi_collect_deferred_deletes(context, false);
    ldk_rhi_reset_bound_state(context);

    if (context->functions.begin_frame != NULL)
    {
      context->functions.begin_frame(context->backend_user_data);
    }

    context->frame_active = true;
  }
}

void ldk_rhi_end_frame(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && !context->pass_active)
  {
    if (context->functions.end_frame != NULL)
    {
      context->functions.end_frame(context->backend_user_data);
    }

    context->frame_active = false;
    ldk_rhi_reset_bound_state(context);
    context->frame_index++;
  }
}

void ldk_rhi_begin_pass(LDKRHIContext* context, const LDKRHIPassDesc* desc)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && !context->pass_active &&
      ldk_rhi_is_valid_pass_desc(desc) && context->functions.begin_pass != NULL)
  {
    for (uint32_t i = 0; i < desc->color_attachment_count; i++)
    {
      LDKRHITexture texture = desc->color_attachments[i].texture;

      if (texture != LDK_RHI_INVALID_TEXTURE &&
          ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_TEXTURE, texture))
      {
        return;
      }
    }

    if (desc->depth_attachment.valid &&
        ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_TEXTURE, desc->depth_attachment.texture))
    {
      return;
    }

    ldk_rhi_reset_bound_state(context);
    context->functions.begin_pass(context->backend_user_data, desc);
    context->pass_active = true;
  }
}

void ldk_rhi_end_pass(LDKRHIContext* context)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active && context->functions.end_pass != NULL)
  {
    context->functions.end_pass(context->backend_user_data);
    context->pass_active = false;
    ldk_rhi_reset_bound_state(context);
  }
}

void ldk_rhi_bind_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      pipeline != LDK_RHI_INVALID_PIPELINE &&
      !ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_PIPELINE, pipeline) &&
      context->functions.bind_pipeline != NULL)
  {
    context->functions.bind_pipeline(context->backend_user_data, pipeline);
    context->bound_pipeline = pipeline;
    context->bound_bindings = LDK_RHI_INVALID_BINDINGS;
  }
}

void ldk_rhi_bind_bindings(LDKRHIContext* context, LDKRHIBindings bindings)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      context->bound_pipeline != LDK_RHI_INVALID_PIPELINE &&
      bindings != LDK_RHI_INVALID_BINDINGS &&
      !ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BINDINGS, bindings) &&
      context->functions.bind_bindings != NULL)
  {
    context->functions.bind_bindings(context->backend_user_data, bindings);
    context->bound_bindings = bindings;
  }
}

void ldk_rhi_bind_vertex_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      buffer != LDK_RHI_INVALID_BUFFER &&
      !ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BUFFER, buffer) &&
      context->functions.bind_vertex_buffer != NULL)
  {
    context->functions.bind_vertex_buffer(context->backend_user_data, buffer, offset);
    context->bound_vertex_buffer = buffer;
  }
}

void ldk_rhi_bind_index_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      buffer != LDK_RHI_INVALID_BUFFER &&
      ldk_rhi_is_valid_index_type(index_type) &&
      !ldk_rhi_is_deferred_delete_pending(context, LDK_RHI_DEFERRED_DELETE_BUFFER, buffer) &&
      context->functions.bind_index_buffer != NULL)
  {
    context->functions.bind_index_buffer(context->backend_user_data, buffer, offset, index_type);
    context->bound_index_buffer = buffer;
  }
}

void ldk_rhi_set_viewport(LDKRHIContext* context, const LDKRHIViewport* viewport)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      viewport != NULL && context->functions.set_viewport != NULL)
  {
    context->functions.set_viewport(context->backend_user_data, viewport);
  }
}

void ldk_rhi_set_scissor(LDKRHIContext* context, const LDKRHIRect* scissor)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      scissor != NULL && context->functions.set_scissor != NULL)
  {
    context->functions.set_scissor(context->backend_user_data, scissor);
  }
}

void ldk_rhi_draw(LDKRHIContext* context, const LDKRHIDrawDesc* desc)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      desc != NULL && desc->vertex_count > 0 &&
      context->bound_pipeline != LDK_RHI_INVALID_PIPELINE &&
      context->bound_bindings != LDK_RHI_INVALID_BINDINGS &&
      context->bound_vertex_buffer != LDK_RHI_INVALID_BUFFER &&
      context->functions.draw != NULL)
  {
    context->functions.draw(context->backend_user_data, desc);
  }
}

void ldk_rhi_draw_indexed(LDKRHIContext* context, const LDKRHIDrawIndexedDesc* desc)
{
  if (ldk_rhi_has_backend(context) && context->frame_active && context->pass_active &&
      desc != NULL && desc->index_count > 0 &&
      context->bound_pipeline != LDK_RHI_INVALID_PIPELINE &&
      context->bound_bindings != LDK_RHI_INVALID_BINDINGS &&
      context->bound_vertex_buffer != LDK_RHI_INVALID_BUFFER &&
      context->bound_index_buffer != LDK_RHI_INVALID_BUFFER &&
      context->functions.draw_indexed != NULL)
  {
    context->functions.draw_indexed(context->backend_user_data, desc);
  }
}
