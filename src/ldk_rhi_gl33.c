#include "ldk_rhi_gl33.h"
#include "ldk_gl.h"
#include "ldk_rhi_gl33_shaders.h"
#include <module/ldk_renderer.h>

#include <math.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

typedef struct LDKRHIGL33TextureInfo
{
  GLenum target;
  LDKRHIFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mip_count;
} LDKRHIGL33TextureInfo;

typedef struct LDKRHIGL33BufferInfo
{
  GLenum target;
  LDKRHIBufferStatsClass stats_class;
} LDKRHIGL33BufferInfo;

typedef struct LDKRHIGL33BindingsLayoutEntry
{
  uint32_t slot;
  LDKRHIBindingType type;
  uint32_t stages;
} LDKRHIGL33BindingsLayoutEntry;

typedef struct LDKRHIGL33BindingsLayoutObject
{
  bool alive;
  uint32_t entry_count;
  LDKRHIGL33BindingsLayoutEntry entries[LDK_RHI_BINDING_MAX];
} LDKRHIGL33BindingsLayoutObject;

typedef struct LDKRHIGL33BindingObject
{
  uint32_t slot;
  LDKRHIBindingType type;
  uint32_t stages;
  LDKRHIBuffer buffer;
  uint32_t buffer_offset;
  uint32_t buffer_size;
  LDKRHITexture texture;
  LDKRHISampler sampler;
} LDKRHIGL33BindingObject;

typedef struct LDKRHIGL33BindingsObject
{
  bool alive;
  LDKRHIBindingsLayout layout;
  uint32_t binding_count;
  LDKRHIGL33BindingObject bindings[LDK_RHI_BINDING_MAX];
} LDKRHIGL33BindingsObject;

typedef struct LDKRHIGL33PipelineObject
{
  bool alive;
  GLuint program;
  GLuint vao;
  LDKRHIPrimitiveTopology topology;
  LDKRHIBlendState blend_state;
  LDKRHIDepthState depth_state;
  LDKRHIRasterState raster_state;
  LDKRHIVertexBufferLayoutDesc vertex_layout;
  uint32_t vertex_buffer_layout_count;
  LDKRHIVertexBufferLayoutDesc vertex_buffer_layouts[LDK_RHI_VERTEX_BUFFER_LAYOUT_MAX];
} LDKRHIGL33PipelineObject;

typedef struct LDKRHIGL33Backend
{
  // texturs
  LDKRHIGL33TextureInfo* textures;
  uint32_t texture_capacity;

  // buffers
  LDKRHIGL33BufferInfo* buffers;
  uint32_t buffer_capacity;

  // binding layouts
  LDKRHIGL33BindingsLayoutObject* bindings_layouts;
  uint32_t bindings_layout_capacity;

  // bindings
  LDKRHIGL33BindingsObject* bindings;
  uint32_t bindings_capacity;

  // pipeliens
  LDKRHIGL33PipelineObject* pipelines;
  uint32_t pipeline_capacity;

  // diagnostics
  LDKRHIFrameStats pending_frame_stats;
  LDKRHIFrameStats last_frame_stats;

  // state
  GLuint pass_fbo;
  GLuint current_fbo;
  GLsizei current_framebuffer_width;
  GLsizei current_framebuffer_height;

  LDKRHIPipeline current_pipeline;
  LDKRHIBuffer current_vertex_buffer;
  uint32_t current_vertex_buffer_offset;
  LDKRHIBuffer current_vertex_buffers[LDK_RHI_VERTEX_BUFFER_LAYOUT_MAX];
  uint32_t current_vertex_buffer_offsets[LDK_RHI_VERTEX_BUFFER_LAYOUT_MAX];
  LDKRHIBuffer current_index_buffer;
  uint32_t current_index_buffer_offset;
  LDKRHIIndexType current_index_type;
  GLfloat line_width_min;
  GLfloat line_width_max;
  bool line_width_range_valid;
} LDKRHIGL33Backend;

static void ldk_rhi_gl33_reset_bound_state(LDKRHIGL33Backend* backend)
{
  if (backend == NULL)
  {
    return;
  }

  backend->current_pipeline = LDK_RHI_INVALID_RESOURCE;
  backend->current_vertex_buffer = LDK_RHI_INVALID_RESOURCE;
  backend->current_vertex_buffer_offset = 0;
  backend->current_index_buffer = LDK_RHI_INVALID_RESOURCE;
  backend->current_index_buffer_offset = 0;

  for (uint32_t i = 0; i < LDK_RHI_VERTEX_BUFFER_LAYOUT_MAX; i++)
  {
    backend->current_vertex_buffers[i] = LDK_RHI_INVALID_RESOURCE;
    backend->current_vertex_buffer_offsets[i] = 0;
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  /* GL_ELEMENT_ARRAY_BUFFER binding belongs to the currently bound VAO.
   * In a core profile, changing it while VAO 0 is bound is invalid. Each
   * indexed draw binds its element buffer explicitly after binding the
   * pipeline VAO, so only reset the backend's logical state here. */
}

static bool ldk_rhi_gl33_grow(void** data, uint32_t* capacity, uint32_t element_size, uint32_t required_index)
{
  if (required_index < *capacity)
  {
    return true;
  }

  uint32_t new_capacity = *capacity == 0 ? 64 : *capacity;
  while (required_index >= new_capacity)
  {
    new_capacity = new_capacity * 2;
  }

  void* new_data = realloc(*data, (size_t)new_capacity * (size_t)element_size);
  if (new_data == NULL)
  {
    return false;
  }

  uint32_t old_capacity = *capacity;
  uint8_t* clear_begin = (uint8_t*)new_data + ((size_t)old_capacity * (size_t)element_size);
  size_t clear_size = ((size_t)new_capacity - (size_t)old_capacity) * (size_t)element_size;
  memset(clear_begin, 0, clear_size);

  *data = new_data;
  *capacity = new_capacity;
  return true;
}

static LDKRHIBindingsLayout ldk_rhi_gl33_alloc_bindings_layout(LDKRHIGL33Backend* backend)
{
  for (uint32_t i = 1; i < backend->bindings_layout_capacity; i++)
  {
    if (!backend->bindings_layouts[i].alive)
    {
      backend->bindings_layouts[i].alive = true;
      return (LDKRHIBindingsLayout)i;
    }
  }

  uint32_t index = backend->bindings_layout_capacity == 0 ? 1 : backend->bindings_layout_capacity;
  bool ok = ldk_rhi_gl33_grow((void**)&backend->bindings_layouts, &backend->bindings_layout_capacity, sizeof(backend->bindings_layouts[0]), index + 1);
  if (!ok)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  backend->bindings_layouts[index].alive = true;
  return (LDKRHIBindingsLayout)index;
}

static LDKRHIBindings ldk_rhi_gl33_alloc_bindings(LDKRHIGL33Backend* backend)
{
  for (uint32_t i = 1; i < backend->bindings_capacity; i++)
  {
    if (!backend->bindings[i].alive)
    {
      backend->bindings[i].alive = true;
      return (LDKRHIBindings)i;
    }
  }

  uint32_t index = backend->bindings_capacity == 0 ? 1 : backend->bindings_capacity;
  bool ok = ldk_rhi_gl33_grow((void**)&backend->bindings, &backend->bindings_capacity, sizeof(backend->bindings[0]), index + 1);
  if (!ok)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  backend->bindings[index].alive = true;
  return (LDKRHIBindings)index;
}

static LDKRHIPipeline ldk_rhi_gl33_alloc_pipeline(LDKRHIGL33Backend* backend)
{
  for (uint32_t i = 1; i < backend->pipeline_capacity; i++)
  {
    if (!backend->pipelines[i].alive)
    {
      backend->pipelines[i].alive = true;
      return (LDKRHIPipeline)i;
    }
  }

  uint32_t index = backend->pipeline_capacity == 0 ? 1 : backend->pipeline_capacity;
  bool ok = ldk_rhi_gl33_grow((void**)&backend->pipelines, &backend->pipeline_capacity, sizeof(backend->pipelines[0]), index + 1);
  if (!ok)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  backend->pipelines[index].alive = true;
  return (LDKRHIPipeline)index;
}

static GLenum ldk_rhi_gl33_buffer_target(uint32_t usage)
{
  if ((usage & LDK_RHI_BUFFER_USAGE_INDEX) != 0)
  {
    return GL_ELEMENT_ARRAY_BUFFER;
  }

  if ((usage & LDK_RHI_BUFFER_USAGE_UNIFORM) != 0)
  {
    return GL_UNIFORM_BUFFER;
  }

  return GL_ARRAY_BUFFER;
}

static LDKRHIBufferStatsClass ldk_rhi_gl33_buffer_stats_class_resolve(
    const LDKRHIBufferDesc* desc)
{
  if (desc->stats_class != LDK_RHI_BUFFER_STATS_AUTO)
  {
    return desc->stats_class;
  }

  if ((desc->usage & LDK_RHI_BUFFER_USAGE_UNIFORM) != 0)
  {
    return LDK_RHI_BUFFER_STATS_UNIFORM;
  }
  if ((desc->usage & LDK_RHI_BUFFER_USAGE_INDEX) != 0)
  {
    return LDK_RHI_BUFFER_STATS_INDEX;
  }
  if ((desc->usage & LDK_RHI_BUFFER_USAGE_VERTEX) != 0)
  {
    return LDK_RHI_BUFFER_STATS_VERTEX;
  }
  return LDK_RHI_BUFFER_STATS_OTHER;
}

static void ldk_rhi_gl33_buffer_update_stats_add(
    LDKRHIFrameStats* stats, LDKRHIBufferStatsClass stats_class, uint32_t size)
{
  switch (stats_class)
  {
    case LDK_RHI_BUFFER_STATS_UNIFORM:
      stats->uniform_buffer_update_count++;
      stats->uniform_buffer_update_bytes += (uint64_t)size;
      break;
    case LDK_RHI_BUFFER_STATS_VERTEX:
      stats->vertex_buffer_update_count++;
      stats->vertex_buffer_update_bytes += (uint64_t)size;
      break;
    case LDK_RHI_BUFFER_STATS_INDEX:
      stats->index_buffer_update_count++;
      stats->index_buffer_update_bytes += (uint64_t)size;
      break;
    case LDK_RHI_BUFFER_STATS_INSTANCE:
      stats->instance_buffer_update_count++;
      stats->instance_buffer_update_bytes += (uint64_t)size;
      break;
    case LDK_RHI_BUFFER_STATS_AUTO:
    case LDK_RHI_BUFFER_STATS_OTHER:
    default:
      stats->other_buffer_update_count++;
      stats->other_buffer_update_bytes += (uint64_t)size;
      break;
  }
}

static GLenum ldk_rhi_gl33_buffer_usage(LDKRHIMemoryUsage usage)
{
  if (usage == LDK_RHI_MEMORY_USAGE_CPU_TO_GPU)
  {
    return GL_DYNAMIC_DRAW;
  }

  if (usage == LDK_RHI_MEMORY_USAGE_GPU_TO_CPU)
  {
    return GL_DYNAMIC_READ;
  }

  return GL_STATIC_DRAW;
}

static GLenum ldk_rhi_gl33_texture_target(LDKRHITextureType type)
{
  if (type == LDK_RHI_TEXTURE_TYPE_3D)
  {
    return GL_TEXTURE_3D;
  }

  if (type == LDK_RHI_TEXTURE_TYPE_CUBE)
  {
    return GL_TEXTURE_CUBE_MAP;
  }

  return GL_TEXTURE_2D;
}

static GLenum ldk_rhi_gl33_internal_format(LDKRHIFormat format)
{
  switch (format)
  {
    case LDK_RHI_FORMAT_R8_UNORM: return GL_R8;
    case LDK_RHI_FORMAT_RG8_UNORM: return GL_RG8;
    case LDK_RHI_FORMAT_RGBA8_UNORM: return GL_RGBA8;
    case LDK_RHI_FORMAT_RGBA8_SRGB: return GL_SRGB8_ALPHA8;
    case LDK_RHI_FORMAT_BGRA8_UNORM: return GL_RGBA8;
    case LDK_RHI_FORMAT_BGRA8_SRGB: return GL_SRGB8_ALPHA8;
    case LDK_RHI_FORMAT_R16_UNORM: return GL_R16;
    case LDK_RHI_FORMAT_RG16_UNORM: return GL_RG16;
    case LDK_RHI_FORMAT_RGBA16_UNORM: return GL_RGBA16;
    case LDK_RHI_FORMAT_R16_FLOAT: return GL_R16F;
    case LDK_RHI_FORMAT_RG16_FLOAT: return GL_RG16F;
    case LDK_RHI_FORMAT_RGBA16_FLOAT: return GL_RGBA16F;
    case LDK_RHI_FORMAT_R32_FLOAT: return GL_R32F;
    case LDK_RHI_FORMAT_RG32_FLOAT: return GL_RG32F;
    case LDK_RHI_FORMAT_RGBA32_FLOAT: return GL_RGBA32F;
    case LDK_RHI_FORMAT_D24S8: return GL_DEPTH24_STENCIL8;
    case LDK_RHI_FORMAT_D32_FLOAT: return GL_DEPTH_COMPONENT32F;
    default: return GL_RGBA8;
  }
}

static GLenum ldk_rhi_gl33_external_format(LDKRHIFormat format)
{
  switch (format)
  {
    case LDK_RHI_FORMAT_R8_UNORM: return GL_RED;
    case LDK_RHI_FORMAT_RG8_UNORM: return GL_RG;
    case LDK_RHI_FORMAT_R16_UNORM: return GL_RED;
    case LDK_RHI_FORMAT_RG16_UNORM: return GL_RG;
    case LDK_RHI_FORMAT_RGBA16_UNORM: return GL_RGBA;
    case LDK_RHI_FORMAT_BGRA8_UNORM: return GL_BGRA;
    case LDK_RHI_FORMAT_BGRA8_SRGB: return GL_BGRA;
    case LDK_RHI_FORMAT_D24S8: return GL_DEPTH_STENCIL;
    case LDK_RHI_FORMAT_D32_FLOAT: return GL_DEPTH_COMPONENT;
    default: return GL_RGBA;
  }
}

static GLenum ldk_rhi_gl33_external_type(LDKRHIFormat format)
{
  switch (format)
  {
    case LDK_RHI_FORMAT_R16_UNORM:
    case LDK_RHI_FORMAT_RG16_UNORM:
    case LDK_RHI_FORMAT_RGBA16_UNORM:
      return GL_UNSIGNED_SHORT;
    case LDK_RHI_FORMAT_R16_FLOAT:
    case LDK_RHI_FORMAT_RG16_FLOAT:
    case LDK_RHI_FORMAT_RGBA16_FLOAT:
    case LDK_RHI_FORMAT_R32_FLOAT:
    case LDK_RHI_FORMAT_RG32_FLOAT:
    case LDK_RHI_FORMAT_RGBA32_FLOAT:
    case LDK_RHI_FORMAT_D32_FLOAT:
      return GL_FLOAT;
    case LDK_RHI_FORMAT_D24S8:
      return GL_UNSIGNED_INT_24_8;
    default:
      return GL_UNSIGNED_BYTE;
  }
}

static GLenum ldk_rhi_gl33_swizzle(LDKRHITextureSwizzle swizzle, GLenum identity)
{
  switch (swizzle)
  {
    case LDK_RHI_TEXTURE_SWIZZLE_ZERO: return GL_ZERO;
    case LDK_RHI_TEXTURE_SWIZZLE_ONE: return GL_ONE;
    case LDK_RHI_TEXTURE_SWIZZLE_R: return GL_RED;
    case LDK_RHI_TEXTURE_SWIZZLE_G: return GL_GREEN;
    case LDK_RHI_TEXTURE_SWIZZLE_B: return GL_BLUE;
    case LDK_RHI_TEXTURE_SWIZZLE_A: return GL_ALPHA;
    case LDK_RHI_TEXTURE_SWIZZLE_IDENTITY: return identity;
    default: return identity;
  }
}

static GLenum ldk_rhi_gl33_filter(LDKRHIFilter filter)
{
  if (filter == LDK_RHI_FILTER_NEAREST)
  {
    return GL_NEAREST;
  }

  return GL_LINEAR;
}

static GLenum ldk_rhi_gl33_min_filter(
    LDKRHIFilter min_filter,
    LDKRHIFilter mip_filter)
{
  if (mip_filter == LDK_RHI_FILTER_NONE)
  {
    return ldk_rhi_gl33_filter(min_filter);
  }

  if (min_filter == LDK_RHI_FILTER_NEAREST)
  {
    return mip_filter == LDK_RHI_FILTER_NEAREST
      ? GL_NEAREST_MIPMAP_NEAREST
      : GL_NEAREST_MIPMAP_LINEAR;
  }

  return mip_filter == LDK_RHI_FILTER_NEAREST
    ? GL_LINEAR_MIPMAP_NEAREST
    : GL_LINEAR_MIPMAP_LINEAR;
}

static GLenum ldk_rhi_gl33_wrap(LDKRHIWrap wrap)
{
  if (wrap == LDK_RHI_WRAP_REPEAT)
  {
    return GL_REPEAT;
  }

  if (wrap == LDK_RHI_WRAP_MIRRORED_REPEAT)
  {
    return GL_MIRRORED_REPEAT;
  }

  return GL_CLAMP_TO_EDGE;
}

static GLenum ldk_rhi_gl33_shader_stage(uint32_t stage)
{
  if ((stage & LDK_RHI_SHADER_STAGE_VERTEX) != 0)
  {
    return GL_VERTEX_SHADER;
  }

  if ((stage & LDK_RHI_SHADER_STAGE_FRAGMENT) != 0)
  {
    return GL_FRAGMENT_SHADER;
  }

  return 0;
}

static GLenum ldk_rhi_gl33_topology(LDKRHIPrimitiveTopology topology)
{
  if (topology == LDK_RHI_PRIMITIVE_TOPOLOGY_LINES)
  {
    return GL_LINES;
  }

  return GL_TRIANGLES;
}

static GLenum ldk_rhi_gl33_polygon_mode(LDKRHIPolygonMode polygon_mode)
{
  return polygon_mode == LDK_RHI_POLYGON_MODE_LINE ? GL_LINE : GL_FILL;
}

static GLenum ldk_rhi_gl33_compare_op(LDKRHICompareOp op)
{
  switch (op)
  {
    case LDK_RHI_COMPARE_OP_NEVER: return GL_NEVER;
    case LDK_RHI_COMPARE_OP_LESS: return GL_LESS;
    case LDK_RHI_COMPARE_OP_LESS_EQUAL: return GL_LEQUAL;
    case LDK_RHI_COMPARE_OP_EQUAL: return GL_EQUAL;
    case LDK_RHI_COMPARE_OP_GREATER: return GL_GREATER;
    case LDK_RHI_COMPARE_OP_GREATER_EQUAL: return GL_GEQUAL;
    case LDK_RHI_COMPARE_OP_NOT_EQUAL: return GL_NOTEQUAL;
    case LDK_RHI_COMPARE_OP_ALWAYS: return GL_ALWAYS;
    default: return GL_LEQUAL;
  }
}

static GLenum ldk_rhi_gl33_blend_factor(LDKRHIBlendFactor factor)
{
  switch (factor)
  {
    case LDK_RHI_BLEND_FACTOR_ZERO: return GL_ZERO;
    case LDK_RHI_BLEND_FACTOR_ONE: return GL_ONE;
    case LDK_RHI_BLEND_FACTOR_SRC_COLOR: return GL_SRC_COLOR;
    case LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
    case LDK_RHI_BLEND_FACTOR_DST_COLOR: return GL_DST_COLOR;
    case LDK_RHI_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
    case LDK_RHI_BLEND_FACTOR_SRC_ALPHA: return GL_SRC_ALPHA;
    case LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
    case LDK_RHI_BLEND_FACTOR_DST_ALPHA: return GL_DST_ALPHA;
    case LDK_RHI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
    default: return GL_ONE;
  }
}

static GLenum ldk_rhi_gl33_blend_op(LDKRHIBlendOp op)
{
  if (op == LDK_RHI_BLEND_OP_SUBTRACT)
  {
    return GL_FUNC_SUBTRACT;
  }

  if (op == LDK_RHI_BLEND_OP_REVERSE_SUBTRACT)
  {
    return GL_FUNC_REVERSE_SUBTRACT;
  }

  return GL_FUNC_ADD;
}

static void ldk_rhi_gl33_vertex_format(LDKRHIVertexFormat format, GLint* count, GLenum* type, GLboolean* normalized)
{
  *count = 1;
  *type = GL_FLOAT;
  *normalized = GL_FALSE;

  if (format == LDK_RHI_VERTEX_FORMAT_FLOAT2)
  {
    *count = 2;
    return;
  }

  if (format == LDK_RHI_VERTEX_FORMAT_FLOAT3)
  {
    *count = 3;
    return;
  }

  if (format == LDK_RHI_VERTEX_FORMAT_FLOAT4)
  {
    *count = 4;
    return;
  }

  if (format == LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM)
  {
    *count = 4;
    *type = GL_UNSIGNED_BYTE;
    *normalized = GL_TRUE;
    return;
  }
}

static void ldk_rhi_gl33_apply_pipeline_state(const LDKRHIGL33PipelineObject* pipeline)
{
  if (pipeline->blend_state.enabled)
  {
    glEnable(GL_BLEND);
    glBlendFuncSeparate(ldk_rhi_gl33_blend_factor(pipeline->blend_state.src_color_factor), ldk_rhi_gl33_blend_factor(pipeline->blend_state.dst_color_factor), ldk_rhi_gl33_blend_factor(pipeline->blend_state.src_alpha_factor), ldk_rhi_gl33_blend_factor(pipeline->blend_state.dst_alpha_factor));
    glBlendEquationSeparate(ldk_rhi_gl33_blend_op(pipeline->blend_state.color_op), ldk_rhi_gl33_blend_op(pipeline->blend_state.alpha_op));
  }
  else
  {
    glDisable(GL_BLEND);
  }

  glColorMask(
      (pipeline->blend_state.color_write_mask & LDK_RHI_COLOR_WRITE_MASK_R) ? GL_TRUE : GL_FALSE,
      (pipeline->blend_state.color_write_mask & LDK_RHI_COLOR_WRITE_MASK_G) ? GL_TRUE : GL_FALSE,
      (pipeline->blend_state.color_write_mask & LDK_RHI_COLOR_WRITE_MASK_B) ? GL_TRUE : GL_FALSE,
      (pipeline->blend_state.color_write_mask & LDK_RHI_COLOR_WRITE_MASK_A) ? GL_TRUE : GL_FALSE
      );

  if (pipeline->depth_state.test_enabled)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(ldk_rhi_gl33_compare_op(pipeline->depth_state.compare_op));
  }
  else
  {
    glDisable(GL_DEPTH_TEST);
  }

  glDepthMask(pipeline->depth_state.write_enabled ? GL_TRUE : GL_FALSE);

  if (pipeline->raster_state.cull_mode == LDK_RHI_CULL_MODE_NONE)
  {
    glDisable(GL_CULL_FACE);
  }
  else
  {
    glEnable(GL_CULL_FACE);
    glCullFace(pipeline->raster_state.cull_mode == LDK_RHI_CULL_MODE_FRONT ? GL_FRONT : GL_BACK);
  }

  glFrontFace(pipeline->raster_state.front_face == LDK_RHI_FRONT_FACE_CW ? GL_CW : GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK,
      ldk_rhi_gl33_polygon_mode(pipeline->raster_state.polygon_mode));

  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_POLYGON_OFFSET_LINE);
  if (pipeline->raster_state.depth_bias_enabled)
  {
    glEnable(pipeline->raster_state.polygon_mode == LDK_RHI_POLYGON_MODE_LINE
            ? GL_POLYGON_OFFSET_LINE
            : GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(pipeline->raster_state.depth_bias_slope_factor,
        pipeline->raster_state.depth_bias_constant_factor);
  }

  if (pipeline->raster_state.scissor_enabled)
  {
    glEnable(GL_SCISSOR_TEST);
  }
  else
  {
    glDisable(GL_SCISSOR_TEST);
  }
}

static void ldk_rhi_gl33_apply_vertex_layout(LDKRHIGL33Backend* backend, const LDKRHIGL33PipelineObject* pipeline)
{
  glBindVertexArray(pipeline->vao);

  for (uint32_t i = 0; i < LDK_RHI_VERTEX_ATTRIBUTE_MAX; i++)
  {
    glDisableVertexAttribArray(i);
    glVertexAttribDivisor(i, 0);
  }

  for (uint32_t layout_index = 0; layout_index < pipeline->vertex_buffer_layout_count; layout_index++)
  {
    const LDKRHIVertexBufferLayoutDesc* layout = &pipeline->vertex_buffer_layouts[layout_index];
    LDKRHIBuffer buffer = backend->current_vertex_buffers[layout_index];
    uint32_t buffer_offset = backend->current_vertex_buffer_offsets[layout_index];

    if (buffer == LDK_RHI_INVALID_RESOURCE)
    {
      continue;
    }

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buffer);

    for (uint32_t i = 0; i < layout->attribute_count; i++)
    {
      LDKRHIVertexAttributeDesc attribute = layout->attributes[i];
      GLint count = 0;
      GLenum type = 0;
      GLboolean normalized = GL_FALSE;
      GLuint divisor = layout->input_rate == LDK_RHI_VERTEX_INPUT_RATE_PER_INSTANCE ? 1u : 0u;
      ldk_rhi_gl33_vertex_format(attribute.format, &count, &type, &normalized);

      uintptr_t offset = (uintptr_t)buffer_offset + (uintptr_t)attribute.offset;

      glEnableVertexAttribArray(attribute.location);
      glVertexAttribPointer(attribute.location, count, type, normalized, (GLsizei)layout->stride, (const void*)offset);
      glVertexAttribDivisor(attribute.location, divisor);
    }
  }
}

static bool ldk_rhi_gl33_pipeline_vertex_buffers_bound(LDKRHIGL33Backend* backend, const LDKRHIGL33PipelineObject* pipeline)
{
  for (uint32_t layout_index = 0; layout_index < pipeline->vertex_buffer_layout_count; layout_index++)
  {
    if (backend->current_vertex_buffers[layout_index] == LDK_RHI_INVALID_RESOURCE)
    {
      return false;
    }
  }

  return true;
}

static void ldk_rhi_gl33_shutdown(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend == NULL)
  {
    return;
  }

  for (uint32_t i = 1; i < backend->pipeline_capacity; i++)
  {
    if (backend->pipelines[i].alive)
    {
      glDeleteProgram(backend->pipelines[i].program);
      glDeleteVertexArrays(1, &backend->pipelines[i].vao);
    }
  }

  if (backend->pass_fbo != 0)
  {
    glDeleteFramebuffers(1, &backend->pass_fbo);
    backend->pending_frame_stats.framebuffer_destroy_count++;
    backend->pass_fbo = 0;
    backend->current_fbo = 0;
  }

  free(backend->textures);
  free(backend->buffers);
  free(backend->bindings_layouts);
  free(backend->bindings);
  free(backend->pipelines);
  free(backend);
}

static LDKRHIBuffer ldk_rhi_gl33_buffer_create(void* backend_user_data, const LDKRHIBufferDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;

  GLuint buffer = 0;
  GLenum target = ldk_rhi_gl33_buffer_target(desc->usage);
  GLenum usage = ldk_rhi_gl33_buffer_usage(desc->memory_usage);

  glGenBuffers(1, &buffer);
  if (buffer != 0)
  {
    backend->pending_frame_stats.buffer_create_count++;
  }

  /* Buffer storage is not tied to the target used for allocation. Use a
   * neutral transfer binding so index-buffer creation never depends on a
   * VAO being current. GL_ELEMENT_ARRAY_BUFFER is VAO state in core GL. */
  glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
  glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)desc->size,
      desc->initial_data, usage);
  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

  bool ok = ldk_rhi_gl33_grow(
      (void**)&backend->buffers,
      &backend->buffer_capacity,
      sizeof(backend->buffers[0]),
      buffer + 1);

  if (!ok)
  {
    glDeleteBuffers(1, &buffer);
    backend->pending_frame_stats.buffer_destroy_count++;
    return LDK_RHI_INVALID_RESOURCE;
  }

  backend->buffers[buffer].target = target;
  backend->buffers[buffer].stats_class =
      ldk_rhi_gl33_buffer_stats_class_resolve(desc);

  return (LDKRHIBuffer)buffer;
}

static void ldk_rhi_gl33_buffer_destroy(void* backend_user_data, LDKRHIBuffer buffer)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;

  GLuint gl_buffer = (GLuint)buffer;
  glDeleteBuffers(1, &gl_buffer);
  backend->pending_frame_stats.buffer_destroy_count++;

  if (buffer < backend->buffer_capacity)
  {
    memset(&backend->buffers[buffer], 0, sizeof(backend->buffers[buffer]));
  }
}

static bool ldk_rhi_gl33_buffer_update(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;

  if (buffer >= backend->buffer_capacity)
  {
    return false;
  }

  GLenum target = backend->buffers[buffer].target;
  if (target == 0)
  {
    return false;
  }

  /* Do not upload through GL_ELEMENT_ARRAY_BUFFER. Its binding is stored in
   * the current VAO and is invalid when VAO 0 is bound in a core profile.
   * COPY_WRITE_BUFFER provides target-independent buffer uploads without
   * disturbing vertex/index draw state. */
  glBindBuffer(GL_COPY_WRITE_BUFFER, (GLuint)buffer);
  glBufferSubData(
      GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
  backend->pending_frame_stats.buffer_update_count++;
  backend->pending_frame_stats.buffer_update_bytes += (uint64_t)size;
  ldk_rhi_gl33_buffer_update_stats_add(
      &backend->pending_frame_stats, backend->buffers[buffer].stats_class, size);

  return true;
}

static LDKRHITexture ldk_rhi_gl33_texture_create(void* backend_user_data, const LDKRHITextureDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint texture = 0;
  GLenum target = ldk_rhi_gl33_texture_target(desc->type);
  GLenum internal_format = ldk_rhi_gl33_internal_format(desc->format);
  GLenum external_format = ldk_rhi_gl33_external_format(desc->format);
  GLenum external_type = ldk_rhi_gl33_external_type(desc->format);
  glGenTextures(1, &texture);
  if (texture != 0)
  {
    backend->pending_frame_stats.texture_create_count++;
  }
  glBindTexture(target, texture);

  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP)
  {
    glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  }
  glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, (GLint)(desc->mip_count - 1));

  glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, ldk_rhi_gl33_swizzle(desc->swizzle_r, GL_RED));
  glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, ldk_rhi_gl33_swizzle(desc->swizzle_g, GL_GREEN));
  glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, ldk_rhi_gl33_swizzle(desc->swizzle_b, GL_BLUE));
  glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, ldk_rhi_gl33_swizzle(desc->swizzle_a, GL_ALPHA));

  if (target == GL_TEXTURE_2D)
  {
    glTexImage2D(target, 0, (GLint)internal_format, (GLsizei)desc->width, (GLsizei)desc->height, 0, external_format, external_type, desc->initial_data);
  }
  else if (target == GL_TEXTURE_3D)
  {
    glTexImage3D(target, 0, (GLint)internal_format, (GLsizei)desc->width, (GLsizei)desc->height, (GLsizei)desc->depth, 0, external_format, external_type, desc->initial_data);
  }
  else if (target == GL_TEXTURE_CUBE_MAP)
  {
    for (uint32_t face = 0; face < 6; ++face)
    {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
          (GLint)internal_format, (GLsizei)desc->width,
          (GLsizei)desc->height, 0, external_format, external_type, NULL);
    }
  }

  if (desc->mip_count > 1)
  {
    glGenerateMipmap(target);
  }

  bool ok = ldk_rhi_gl33_grow((void**)&backend->textures, &backend->texture_capacity, sizeof(backend->textures[0]), texture + 1);
  if (!ok)
  {
    glBindTexture(target, 0);
    glDeleteTextures(1, &texture);
    backend->pending_frame_stats.texture_destroy_count++;
    return LDK_RHI_INVALID_RESOURCE;
  }

  backend->textures[texture].target = target;
  backend->textures[texture].format = desc->format;
  backend->textures[texture].width = desc->width;
  backend->textures[texture].height = desc->height;
  backend->textures[texture].depth = desc->depth;
  backend->textures[texture].mip_count = desc->mip_count;

  glBindTexture(target, 0);
  return (LDKRHITexture)texture;
}

static void ldk_rhi_gl33_texture_destroy(void* backend_user_data, LDKRHITexture texture)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint gl_texture = (GLuint)texture;
  glDeleteTextures(1, &gl_texture);
  backend->pending_frame_stats.texture_destroy_count++;

  if (texture < backend->texture_capacity)
  {
    memset(&backend->textures[texture], 0, sizeof(backend->textures[texture]));
  }
}

static bool ldk_rhi_gl33_texture_update(void* backend_user_data, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size)
{
  (void)size;
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (texture >= backend->texture_capacity)
  {
    return false;
  }

  LDKRHIGL33TextureInfo info = backend->textures[texture];
  if (info.target == 0)
  {
    return false;
  }

  GLenum external_format = ldk_rhi_gl33_external_format(info.format);
  GLenum external_type = ldk_rhi_gl33_external_type(info.format);
  glBindTexture(info.target, (GLuint)texture);

  if (info.target == GL_TEXTURE_2D)
  {
    glTexSubImage2D(info.target, (GLint)mip_level, 0, 0, (GLsizei)info.width, (GLsizei)info.height, external_format, external_type, data);
  }
  else if (info.target == GL_TEXTURE_3D)
  {
    glTexSubImage3D(info.target, (GLint)mip_level, 0, 0, 0, (GLsizei)info.width, (GLsizei)info.height, (GLsizei)info.depth, external_format, external_type, data);
  }
  else if (info.target == GL_TEXTURE_CUBE_MAP)
  {
    if (layer >= 6)
    {
      glBindTexture(info.target, 0);
      return false;
    }
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer,
        (GLint)mip_level, 0, 0, (GLsizei)info.width,
        (GLsizei)info.height, external_format, external_type, data);
  }
  else
  {
    glBindTexture(info.target, 0);
    return false;
  }

  if (mip_level == 0 && info.mip_count > 1)
  {
    glGenerateMipmap(info.target);
  }

  glBindTexture(info.target, 0);
  return true;
}

static LDKRHISampler ldk_rhi_gl33_create_sampler(void* backend_user_data, const LDKRHISamplerDesc* desc)
{
  (void)backend_user_data;
  GLuint sampler = 0;
  glGenSamplers(1, &sampler);
  glSamplerParameteri(
      sampler,
      GL_TEXTURE_MIN_FILTER,
      ldk_rhi_gl33_min_filter(desc->min_filter, desc->mip_filter));
  glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, ldk_rhi_gl33_filter(desc->mag_filter));
  glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, ldk_rhi_gl33_wrap(desc->wrap_u));
  glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, ldk_rhi_gl33_wrap(desc->wrap_v));
  glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, ldk_rhi_gl33_wrap(desc->wrap_w));
  return (LDKRHISampler)sampler;
}

static void ldk_rhi_gl33_destroy_sampler(void* backend_user_data, LDKRHISampler sampler)
{
  (void)backend_user_data;
  GLuint gl_sampler = (GLuint)sampler;
  glDeleteSamplers(1, &gl_sampler);
}

static LDKRHIShaderModule ldk_rhi_gl33_shader_module_create(
    void* backend_user_data, const LDKRHIShaderModuleDesc* desc)
{
  (void)backend_user_data;
  if (desc->code_format != LDK_RHI_SHADER_CODE_FORMAT_GLSL)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  GLenum stage = ldk_rhi_gl33_shader_stage(desc->stage);
  if (stage == 0)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  GLuint shader = glCreateShader(stage);
  const GLchar* source = (const GLchar*)desc->code;
  GLint length = (GLint)desc->code_size;
  glShaderSource(shader, 1, &source, &length);
  glCompileShader(shader);

  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE)
  {
    glDeleteShader(shader);
    return LDK_RHI_INVALID_RESOURCE;
  }

  return (LDKRHIShaderModule)shader;
}

static void ldk_rhi_gl33_shader_module_destroy(void* backend_user_data, LDKRHIShaderModule shader_module)
{
  (void)backend_user_data;
  GLuint shader = (GLuint)shader_module;
  glDeleteShader(shader);
}

static LDKRHIBindingsLayout ldk_rhi_gl33_bindings_layout_create(void* backend_user_data, const LDKRHIBindingsLayoutDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  LDKRHIBindingsLayout handle = ldk_rhi_gl33_alloc_bindings_layout(backend);
  if (handle == LDK_RHI_INVALID_RESOURCE)
  {
    return handle;
  }

  LDKRHIGL33BindingsLayoutObject* layout = &backend->bindings_layouts[handle];
  layout->entry_count = desc->entry_count;
  for (uint32_t i = 0; i < desc->entry_count; i++)
  {
    layout->entries[i].slot = desc->entries[i].slot;
    layout->entries[i].type = desc->entries[i].type;
    layout->entries[i].stages = desc->entries[i].stages;
  }

  return handle;
}

static void ldk_rhi_gl33_bindings_layout_destroy(void* backend_user_data, LDKRHIBindingsLayout bindings_layout)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (bindings_layout >= backend->bindings_layout_capacity)
  {
    return;
  }

  memset(&backend->bindings_layouts[bindings_layout], 0, sizeof(backend->bindings_layouts[bindings_layout]));
}

static void ldk_rhi_gl33_apply_program_bindings(LDKRHIGL33Backend* backend, GLuint program, LDKRHIBindingsLayout bindings_layout)
{
  if (bindings_layout == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  if (bindings_layout >= backend->bindings_layout_capacity)
  {
    return;
  }

  LDKRHIGL33BindingsLayoutObject* layout = &backend->bindings_layouts[bindings_layout];
  if (!layout->alive)
  {
    return;
  }

  glUseProgram(program);

  for (uint32_t i = 0; i < layout->entry_count; i++)
  {
    char name[32] = {0};
    uint32_t slot = layout->entries[i].slot;
    LDKRHIBindingType type = layout->entries[i].type;

    if (type == LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER)
    {
      snprintf(name, sizeof(name), "LDK_UBO_%u", slot);
      GLuint block_index = glGetUniformBlockIndex(program, name);
      if (block_index != GL_INVALID_INDEX)
      {
        glUniformBlockBinding(program, block_index, slot);
      }
    }
    else if (type == LDK_RHI_BINDING_TYPE_TEXTURE || type == LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER)
    {
      snprintf(name, sizeof(name), "LDK_TEXTURE_%u", slot);
      GLint location = glGetUniformLocation(program, name);
      if (location >= 0)
      {
        glUniform1i(location, (GLint)slot);
      }
    }
  }

  glUseProgram(0);
}

static LDKRHIPipeline ldk_rhi_gl33_pipeline_create(void* backend_user_data, const LDKRHIPipelineDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint program = glCreateProgram();
  glAttachShader(program, (GLuint)desc->vertex_shader_module);
  glAttachShader(program, (GLuint)desc->fragment_shader_module);
  glLinkProgram(program);

  GLint status = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE)
  {
    glDeleteProgram(program);
    return LDK_RHI_INVALID_RESOURCE;
  }

  ldk_rhi_gl33_apply_program_bindings(backend, program, desc->bindings_layout);

  LDKRHIPipeline handle = ldk_rhi_gl33_alloc_pipeline(backend);
  if (handle == LDK_RHI_INVALID_RESOURCE)
  {
    glDeleteProgram(program);
    return handle;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[handle];
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  pipeline->program = program;
  pipeline->vao = vao;
  pipeline->topology = desc->topology;
  pipeline->blend_state = desc->blend_state;
  pipeline->depth_state = desc->depth_state;
  pipeline->raster_state = desc->raster_state;
  pipeline->vertex_layout = desc->vertex_layout;

  if (desc->vertex_buffer_layout_count > 0)
  {
    pipeline->vertex_buffer_layout_count = desc->vertex_buffer_layout_count;
    memcpy(pipeline->vertex_buffer_layouts, desc->vertex_buffer_layouts, sizeof(LDKRHIVertexBufferLayoutDesc) * desc->vertex_buffer_layout_count);
  }
  else if (desc->vertex_layout.attribute_count > 0)
  {
    pipeline->vertex_buffer_layout_count = 1;
    pipeline->vertex_buffer_layouts[0] = desc->vertex_layout;
    pipeline->vertex_buffer_layouts[0].input_rate = LDK_RHI_VERTEX_INPUT_RATE_PER_VERTEX;
  }

  return handle;
}

static void ldk_rhi_gl33_pipeline_destroy(void* backend_user_data, LDKRHIPipeline pipeline)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* object = &backend->pipelines[pipeline];
  if (!object->alive)
  {
    return;
  }

  glDeleteProgram(object->program);
  glDeleteVertexArrays(1, &object->vao);
  memset(object, 0, sizeof(*object));
}

static LDKRHIBindings ldk_rhi_gl33_bindings_create(LDKRHIGL33Backend* backend, const LDKRHIBindingsDesc* desc)
{
  if (!desc)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  if (desc->binding_count > LDK_RHI_BINDING_MAX)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  if (desc->layout >= backend->bindings_layout_capacity || !backend->bindings_layouts[desc->layout].alive)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIGL33BindingsLayoutObject* layout = &backend->bindings_layouts[desc->layout];

  uint32_t index = (uint32_t) ldk_rhi_gl33_alloc_bindings(backend);

  if (index == LDK_RHI_INVALID_RESOURCE)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIGL33BindingsObject* object = &backend->bindings[index];

  object->alive = true;
  object->layout = desc->layout;
  object->binding_count = desc->binding_count;

  for (uint32_t i = 0; i < desc->binding_count; i++)
  {
    bool found = false;

    object->bindings[i].slot = desc->bindings[i].slot;
    object->bindings[i].buffer = desc->bindings[i].buffer;
    object->bindings[i].buffer_offset = desc->bindings[i].buffer_offset;
    object->bindings[i].buffer_size = desc->bindings[i].buffer_size;
    object->bindings[i].texture = desc->bindings[i].texture;
    object->bindings[i].sampler = desc->bindings[i].sampler;

    for (uint32_t j = 0; j < layout->entry_count; j++)
    {
      if (layout->entries[j].slot == desc->bindings[i].slot)
      {
        object->bindings[i].type = layout->entries[j].type;
        object->bindings[i].stages = layout->entries[j].stages;
        found = true;
        break;
      }
    }

    if (!found)
    {
      memset(object, 0, sizeof(*object));
      return LDK_RHI_INVALID_RESOURCE;
    }
  }

  return index;
}

static void ldk_rhi_gl33_bindings_destroy(void* backend_user_data, LDKRHIBindings bindings)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (bindings >= backend->bindings_capacity)
  {
    return;
  }

  memset(&backend->bindings[bindings], 0, sizeof(backend->bindings[bindings]));
}

static bool ldk_rhi_gl33_pass_fbo_ensure(LDKRHIGL33Backend* backend)
{
  if (backend->pass_fbo != 0)
  {
    return true;
  }

  /*
   * The GL33 backend is initialized before the engine creates a window and
   * makes the real OpenGL context current.  Therefore GL objects must not be
   * created from ldk_rhi_gl33_initialize().  Lazily create the scratch pass
   * framebuffer once rendering starts, when a context is guaranteed current.
   */
  glGenFramebuffers(1, &backend->pass_fbo);
  if (backend->pass_fbo == 0)
  {
    return false;
  }

  backend->pending_frame_stats.framebuffer_create_count++;
  return true;
}

static void ldk_rhi_gl33_frame_begin(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (!ldk_rhi_gl33_pass_fbo_ensure(backend))
  {
    fprintf(stderr, "LDK RHI GL33: failed to create persistent pass framebuffer\n");
  }
}

static void ldk_rhi_gl33_frame_end(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  backend->last_frame_stats = backend->pending_frame_stats;
  memset(&backend->pending_frame_stats, 0, sizeof(backend->pending_frame_stats));
}

static LDKRHIFrameStats ldk_rhi_gl33_frame_stats_get(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  return backend != NULL ? backend->last_frame_stats : (LDKRHIFrameStats){0};
}

static void ldk_rhi_gl33_pass_begin(void* backend_user_data, const LDKRHIPassDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  bool use_default_framebuffer = false;

  ldk_rhi_gl33_reset_bound_state(backend);

  if (desc->color_attachment_count == 1 && desc->color_attachments[0].texture == LDK_RHI_INVALID_RESOURCE)
  {
    use_default_framebuffer = true;
  }

  if (use_default_framebuffer)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    backend->current_fbo = 0;
  }
  else if (desc->color_attachment_count > 0 || desc->depth_attachment.valid)
  {
    if (!ldk_rhi_gl33_pass_fbo_ensure(backend))
    {
      fprintf(stderr, "LDK RHI GL33: no framebuffer available for render pass\n");
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      backend->current_fbo = 0;
      return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, backend->pass_fbo);
    backend->current_fbo = backend->pass_fbo;

    /*
     * pass_fbo is intentionally persistent. Detach the previous pass first so
     * a pass with fewer/no color attachments or no depth attachment cannot
     * retain references to textures from the previous pass.
     */
    for (uint32_t i = 0; i < LDK_RHI_COLOR_ATTACHMENT_MAX; i++)
    {
      glFramebufferTexture2D(
          GL_FRAMEBUFFER,
          GL_COLOR_ATTACHMENT0 + i,
          GL_TEXTURE_2D,
          0,
          0);
    }
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        0,
        0);

    GLenum draw_buffers[LDK_RHI_COLOR_ATTACHMENT_MAX];
    for (uint32_t i = 0; i < desc->color_attachment_count; i++)
    {
      GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
      glFramebufferTexture2D(
          GL_FRAMEBUFFER,
          attachment,
          GL_TEXTURE_2D,
          (GLuint)desc->color_attachments[i].texture,
          (GLint)desc->color_attachments[i].mip_level);
      draw_buffers[i] = attachment;
    }

    if (desc->color_attachment_count > 0)
    {
      glDrawBuffers((GLsizei)desc->color_attachment_count, draw_buffers);
      glReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    else
    {
      glDrawBuffer(GL_NONE);
      glReadBuffer(GL_NONE);
    }

    if (desc->depth_attachment.valid)
    {
      glFramebufferTexture2D(
          GL_FRAMEBUFFER,
          GL_DEPTH_ATTACHMENT,
          GL_TEXTURE_2D,
          (GLuint)desc->depth_attachment.texture,
          (GLint)desc->depth_attachment.mip_level);
    }

#ifdef LDK_DEBUG
    GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
    {
      fprintf(stderr,
          "LDK RHI GL33: incomplete framebuffer in pass_begin (0x%04X)\n",
          (unsigned int)framebuffer_status);
    }
#endif
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    backend->current_fbo = 0;
  }

  if (desc->has_viewport)
  {
    glViewport((GLint)desc->viewport.x, (GLint)desc->viewport.y, (GLsizei)desc->viewport.width, (GLsizei)desc->viewport.height);
    backend->current_framebuffer_width = (GLsizei) desc->viewport.width;
    backend->current_framebuffer_height = (GLsizei) desc->viewport.height;
  }

  if (desc->has_scissor)
  {
    glEnable(GL_SCISSOR_TEST);
    glScissor(desc->scissor.x, desc->scissor.y, desc->scissor.width, desc->scissor.height);
  }

  GLbitfield clear_mask = 0;
  for (uint32_t i = 0; i < desc->color_attachment_count; i++)
  {
    if (desc->color_attachments[i].load_op == LDK_RHI_LOAD_OP_CLEAR)
    {
      LDKRHIColor color = desc->color_attachments[i].clear_color;
      glClearColor(color.r, color.g, color.b, color.a);
      clear_mask |= GL_COLOR_BUFFER_BIT;
      break;
    }
  }

  if (desc->depth_attachment.valid && desc->depth_attachment.depth_load_op == LDK_RHI_LOAD_OP_CLEAR)
  {
    glClearDepth(desc->depth_attachment.clear_depth);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
  }

  if (clear_mask != 0)
  {
    GLboolean restore_scissor = GL_FALSE;
    GLint restore_scissor_box[4] = {0, 0, 0, 0};

    if (!desc->has_scissor && glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE)
    {
      restore_scissor = GL_TRUE;
      glGetIntegerv(GL_SCISSOR_BOX, restore_scissor_box);
      glDisable(GL_SCISSOR_TEST);
    }

    GLboolean restore_depth_mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &restore_depth_mask);

    if ((clear_mask & GL_DEPTH_BUFFER_BIT) != 0)
    {
      glDepthMask(GL_TRUE);
    }

    glClear(clear_mask);

    if ((clear_mask & GL_DEPTH_BUFFER_BIT) != 0)
    {
      glDepthMask(restore_depth_mask);
    }

    if (restore_scissor)
    {
      glEnable(GL_SCISSOR_TEST);
      glScissor(restore_scissor_box[0], restore_scissor_box[1], restore_scissor_box[2], restore_scissor_box[3]);
    }
  }
}

static void ldk_rhi_gl33_pass_end(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_fbo != 0)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    backend->current_fbo = 0;
  }

  ldk_rhi_gl33_reset_bound_state(backend);
}

static void ldk_rhi_gl33_pipeline_bind(void* backend_user_data, LDKRHIPipeline pipeline)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* object = &backend->pipelines[pipeline];
  if (!object->alive)
  {
    return;
  }

  backend->current_pipeline = pipeline;
  glUseProgram(object->program);
  glBindVertexArray(object->vao);
  ldk_rhi_gl33_apply_pipeline_state(object);
  ldk_rhi_gl33_apply_vertex_layout(backend, object);

  if (backend->current_index_buffer != LDK_RHI_INVALID_RESOURCE)
  {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)backend->current_index_buffer);
  }
}

static void ldk_rhi_gl33_bindings_bind(void* backend_user_data, LDKRHIBindings bindings)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (bindings >= backend->bindings_capacity)
  {
    return;
  }

  LDKRHIGL33BindingsObject* object = &backend->bindings[bindings];
  if (!object->alive)
  {
    return;
  }

  for (uint32_t i = 0; i < object->binding_count; i++)
  {
    LDKRHIGL33BindingObject binding = object->bindings[i];
    if (binding.type == LDK_RHI_BINDING_TYPE_TEXTURE || binding.type == LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER)
    {
      GLenum target = GL_TEXTURE_2D;
      if (binding.texture < backend->texture_capacity && backend->textures[binding.texture].target != 0)
      {
        target = backend->textures[binding.texture].target;
      }

      glActiveTexture(GL_TEXTURE0 + binding.slot);
      glBindTexture(target, (GLuint)binding.texture);
    }

    if (binding.type == LDK_RHI_BINDING_TYPE_SAMPLER || binding.type == LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER)
    {
      glBindSampler(binding.slot, (GLuint)binding.sampler);
    }

    if (binding.type == LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER)
    {
      if (binding.buffer_size > 0)
      {
        glBindBufferRange(GL_UNIFORM_BUFFER, binding.slot, (GLuint)binding.buffer, (GLintptr)binding.buffer_offset, (GLsizeiptr)binding.buffer_size);
      }
      else
      {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding.slot, (GLuint)binding.buffer);
      }
    }
  }
}

static void ldk_rhi_gl33_vertext_buffer_bind(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  backend->current_vertex_buffer = buffer;
  backend->current_vertex_buffer_offset = offset;
  backend->current_vertex_buffers[0] = buffer;
  backend->current_vertex_buffer_offsets[0] = offset;

  if (backend->current_pipeline != LDK_RHI_INVALID_RESOURCE && backend->current_pipeline < backend->pipeline_capacity)
  {
    LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
    if (pipeline->alive)
    {
      ldk_rhi_gl33_apply_vertex_layout(backend, pipeline);
    }
  }
}

static void ldk_rhi_gl33_vertex_buffer_bind_at(void* backend_user_data, uint32_t slot, LDKRHIBuffer buffer, uint32_t offset)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;

  if (slot >= LDK_RHI_VERTEX_BUFFER_LAYOUT_MAX)
  {
    return;
  }

  backend->current_vertex_buffers[slot] = buffer;
  backend->current_vertex_buffer_offsets[slot] = offset;

  if (slot == 0)
  {
    backend->current_vertex_buffer = buffer;
    backend->current_vertex_buffer_offset = offset;
  }

  if (backend->current_pipeline != LDK_RHI_INVALID_RESOURCE && backend->current_pipeline < backend->pipeline_capacity)
  {
    LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
    if (pipeline->alive)
    {
      ldk_rhi_gl33_apply_vertex_layout(backend, pipeline);
    }
  }
}

static void ldk_rhi_gl33_index_buffer_bind(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  backend->current_index_buffer = buffer;
  backend->current_index_buffer_offset = offset;
  backend->current_index_type = index_type;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)buffer);
}

static void ldk_rhi_gl33_viewport_set(void* backend_user_data, const LDKRHIViewport* viewport)
{
  (void)backend_user_data;
  glViewport((GLint)viewport->x, (GLint)viewport->y, (GLsizei)viewport->width, (GLsizei)viewport->height);
  glDepthRange(viewport->min_depth, viewport->max_depth);
}

static void ldk_rhi_gl33_scissor_set(void* backend_user_data, const LDKRHIRect* scissor)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  glEnable(GL_SCISSOR_TEST);
  i32 y = backend->current_framebuffer_height - scissor->y - scissor->height;
  glScissor(scissor->x, y, scissor->width, scissor->height);
}

static void ldk_rhi_gl33_line_width_set(
    void* backend_user_data, float line_width)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;

  if (!backend->line_width_range_valid)
  {
    GLfloat range[2] = {1.0f, 1.0f};
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
    if (!isfinite(range[0]) || !isfinite(range[1]) || range[0] <= 0.0f ||
        range[1] < range[0])
    {
      range[0] = 1.0f;
      range[1] = 1.0f;
    }
    backend->line_width_min = range[0];
    backend->line_width_max = range[1];
    backend->line_width_range_valid = true;
  }

  if (line_width < backend->line_width_min)
  {
    line_width = backend->line_width_min;
  }
  else if (line_width > backend->line_width_max)
  {
    line_width = backend->line_width_max;
  }

  glLineWidth(line_width);
}

static void ldk_rhi_gl33_draw(void* backend_user_data, const LDKRHIDrawDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_RESOURCE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
  if (!ldk_rhi_gl33_pipeline_vertex_buffers_bound(backend, pipeline))
  {
    return;
  }

  GLenum mode = ldk_rhi_gl33_topology(pipeline->topology);
  glDrawArrays(mode, (GLint)desc->first_vertex, (GLsizei)desc->vertex_count);
}

static void ldk_rhi_gl33_draw_indexed(void* backend_user_data, const LDKRHIDrawIndexedDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_RESOURCE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
  if (!ldk_rhi_gl33_pipeline_vertex_buffers_bound(backend, pipeline))
  {
    return;
  }

  if (backend->current_index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  GLenum mode = ldk_rhi_gl33_topology(pipeline->topology);
  GLenum type = backend->current_index_type == LDK_RHI_INDEX_TYPE_UINT32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
  uint32_t index_size = backend->current_index_type == LDK_RHI_INDEX_TYPE_UINT32 ? 4u : 2u;
  uintptr_t index_offset = (uintptr_t)backend->current_index_buffer_offset + ((uintptr_t)desc->first_index * (uintptr_t)index_size);

  if (desc->vertex_offset != 0)
  {
    glDrawElementsBaseVertex(mode, (GLsizei)desc->index_count, type, (const void*)index_offset, desc->vertex_offset);
  }
  else
  {
    glDrawElements(mode, (GLsizei)desc->index_count, type, (const void*)index_offset);
  }
}

static void ldk_rhi_gl33_draw_instanced(void* backend_user_data, const LDKRHIDrawInstancedDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_RESOURCE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  //NOTE: OpenGL 3.3 does not support base-instance draws.
  if (desc->first_instance != 0)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
  if (!ldk_rhi_gl33_pipeline_vertex_buffers_bound(backend, pipeline))
  {
    return;
  }

  GLenum mode = ldk_rhi_gl33_topology(pipeline->topology);
  glDrawArraysInstanced(mode, (GLint)desc->first_vertex, (GLsizei)desc->vertex_count, (GLsizei)desc->instance_count);
}

static void ldk_rhi_gl33_draw_indexed_instanced(void* backend_user_data, const LDKRHIDrawIndexedInstancedDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_RESOURCE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  //NOTE: OpenGL 3.3 does not support base-instance draws.
  if (desc->first_instance != 0)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
  if (!ldk_rhi_gl33_pipeline_vertex_buffers_bound(backend, pipeline))
  {
    return;
  }

  if (backend->current_index_buffer == LDK_RHI_INVALID_RESOURCE)
  {
    return;
  }

  GLenum mode = ldk_rhi_gl33_topology(pipeline->topology);
  GLenum type = backend->current_index_type == LDK_RHI_INDEX_TYPE_UINT32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
  uint32_t index_size = backend->current_index_type == LDK_RHI_INDEX_TYPE_UINT32 ? 4u : 2u;
  uintptr_t index_offset = (uintptr_t)backend->current_index_buffer_offset + ((uintptr_t)desc->first_index * (uintptr_t)index_size);

  if (desc->vertex_offset != 0)
  {
    glDrawElementsInstancedBaseVertex(mode, (GLsizei)desc->index_count, type, (const void*)index_offset, (GLsizei)desc->instance_count, desc->vertex_offset);
  }
  else
  {
    glDrawElementsInstanced(mode, (GLsizei)desc->index_count, type, (const void*)index_offset, (GLsizei)desc->instance_count);
  }
}

bool ldk_rhi_gl33_initialize(LDKRHIContext* context)
{
  if (context == NULL)
  {
    return false;
  }

  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)calloc(1, sizeof(*backend));
  if (backend == NULL)
  {
    return false;
  }

  LDKRHIContextDesc desc = {0};
  desc.backend_type = LDK_RHI_BACKEND_OPENGL33;
  desc.backend_api = NULL;
  desc.backend_user_data = backend;

  LDKRHIFunctions functions = {0};
  functions.shutdown = ldk_rhi_gl33_shutdown;
  functions.buffer_create = ldk_rhi_gl33_buffer_create;
  functions.buffer_destroy = ldk_rhi_gl33_buffer_destroy;
  functions.buffer_update = ldk_rhi_gl33_buffer_update;
  functions.texture_create = ldk_rhi_gl33_texture_create;
  functions.texture_destroy = ldk_rhi_gl33_texture_destroy;
  functions.texture_update = ldk_rhi_gl33_texture_update;
  functions.create_sampler = ldk_rhi_gl33_create_sampler;
  functions.destroy_sampler = ldk_rhi_gl33_destroy_sampler;
  functions.shader_module_create = ldk_rhi_gl33_shader_module_create;
  functions.shader_module_destroy = ldk_rhi_gl33_shader_module_destroy;
  functions.bindings_layout_create = ldk_rhi_gl33_bindings_layout_create;
  functions.bindings_layout_destroy = ldk_rhi_gl33_bindings_layout_destroy;
  functions.pipeline_create = ldk_rhi_gl33_pipeline_create;
  functions.pipeline_destroy = ldk_rhi_gl33_pipeline_destroy;
  functions.bindings_create = ldk_rhi_gl33_bindings_create;
  functions.bindings_destroy = ldk_rhi_gl33_bindings_destroy;
  functions.frame_begin = ldk_rhi_gl33_frame_begin;
  functions.frame_end = ldk_rhi_gl33_frame_end;
  functions.frame_stats_get = ldk_rhi_gl33_frame_stats_get;
  functions.pass_begin = ldk_rhi_gl33_pass_begin;
  functions.pass_end = ldk_rhi_gl33_pass_end;
  functions.pipeline_bind = ldk_rhi_gl33_pipeline_bind;
  functions.bindings_bind = ldk_rhi_gl33_bindings_bind;
  functions.vertex_buffer_bind = ldk_rhi_gl33_vertext_buffer_bind;
  functions.vertex_buffer_bind_at = ldk_rhi_gl33_vertex_buffer_bind_at;
  functions.index_buffer_bind = ldk_rhi_gl33_index_buffer_bind;
  functions.viewport_set = ldk_rhi_gl33_viewport_set;
  functions.scissor_set = ldk_rhi_gl33_scissor_set;
  functions.line_width_set = ldk_rhi_gl33_line_width_set;
  functions.draw = ldk_rhi_gl33_draw;
  functions.draw_indexed = ldk_rhi_gl33_draw_indexed;
  functions.draw_instanced = ldk_rhi_gl33_draw_instanced;
  functions.draw_indexed_instanced = ldk_rhi_gl33_draw_indexed_instanced;

  bool ok = ldk_rhi_initialize(context, &desc, &functions);
  if (!ok)
  {
    free(backend);
    return false;
  }

  return true;
}

LDK_STATIC_ASSERT(LDK_RENDERER_MAX_LIGHTS_PER_VIEW == 16, gl33_light_count);

static char const* ldk_rhi_gl33_builtin_shader_source(u32 shader, u32 stage)
{
  if (shader == LDK_SHADER_WATER_PASS)
  {
    if (stage == LDK_RHI_SHADER_STAGE_VERTEX)
    {
      return LDK_RHI_GL33_WATER_PASS_VERTEX_SHADER;
    }
    if (stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
    {
      return LDK_RHI_GL33_WATER_PASS_FRAGMENT_SHADER;
    }
  }
  if (shader == LDK_SHADER_SHADOW_PASS ||
      shader == LDK_SHADER_SHADOW_PASS_INSTANCED)
  {
    if (stage == LDK_RHI_SHADER_STAGE_VERTEX)
    {
      return LDK_RHI_GL33_SHADOW_PASS_VERTEX_SHADER;
    }
    if (stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
    {
      return LDK_RHI_GL33_SHADOW_PASS_FRAGMENT_SHADER;
    }
  }

  if (shader == LDK_SHADER_SHADOW_PASS_CUTOUT ||
      shader == LDK_SHADER_SHADOW_PASS_CUTOUT_INSTANCED)
  {
    if (stage == LDK_RHI_SHADER_STAGE_VERTEX)
    {
      return LDK_RHI_GL33_SHADOW_PASS_CUTOUT_VERTEX_SHADER;
    }
    if (stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
    {
      return LDK_RHI_GL33_SHADOW_PASS_CUTOUT_FRAGMENT_SHADER;
    }
  }

  if (shader == LDK_SHADER_SHADOW_PASS_VEGETATION ||
      shader == LDK_SHADER_SHADOW_PASS_VEGETATION_INSTANCED)
  {
    if (stage == LDK_RHI_SHADER_STAGE_VERTEX)
    {
      return LDK_RHI_GL33_SHADOW_PASS_VEGETATION_VERTEX_SHADER;
    }
    if (stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
    {
      return LDK_RHI_GL33_SHADOW_PASS_FRAGMENT_SHADER;
    }
  }

  if (shader == LDK_SHADER_TEXT_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_TEXT_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_TEXT_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_TEXT_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_UI_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_UI_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_UI_PASS && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_UI_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_MESH_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_INSTANCED && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_MESH_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_INSTANCED && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_FRAGMENT_SHADER;
  }

  if ((shader == LDK_SHADER_VEGETATION_PASS ||
          shader == LDK_SHADER_VEGETATION_PASS_INSTANCED) &&
      stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_VEGETATION_PASS_VERTEX_SHADER;
  }

  if ((shader == LDK_SHADER_VEGETATION_PASS ||
          shader == LDK_SHADER_VEGETATION_PASS_INSTANCED) &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_VEGETATION_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_VEGETATION_UNLIT_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_VEGETATION_PASS_UNLIT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_TERRAIN_PASS &&
      stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_TERRAIN_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_TERRAIN_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_TERRAIN_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_UNLIT &&
      stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_MESH_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_UNLIT &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_UNLIT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_SOLID_UNLIT &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_SOLID_UNLIT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_TEXTURED &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_TEXTURED_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_TEXTURED_UNLIT &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_TEXTURED_CUTOUT &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_TEXTURED_CUTOUT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_MESH_PASS_TEXTURED_UNLIT_CUTOUT &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_CUTOUT_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_GRID_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_GRID_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_GRID_PASS && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_GRID_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_PRESENT_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_PRESENT_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_PRESENT_PASS && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_PRESENT_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_POST_PROCESS_PASS &&
      stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_POST_PROCESS_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_POST_PROCESS_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_POST_PROCESS_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_SKYBOX_PASS &&
      stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_SKYBOX_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_SKYBOX_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_SKYBOX_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_BLOOM_EXTRACT_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_BLOOM_EXTRACT_PASS_FRAGMENT_SHADER;
  }

  if (shader == LDK_SHADER_BLUR_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_BLUR_PASS_FRAGMENT_SHADER;
  }

  return NULL;
}

static bool ldk_rhi_gl33_shader_is_instanced(u32 shader)
{
  return shader == LDK_SHADER_MESH_PASS_INSTANCED ||
      shader == LDK_SHADER_SHADOW_PASS_INSTANCED ||
      shader == LDK_SHADER_SHADOW_PASS_CUTOUT_INSTANCED ||
      shader == LDK_SHADER_VEGETATION_PASS_INSTANCED ||
      shader == LDK_SHADER_SHADOW_PASS_VEGETATION_INSTANCED;
}

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(
    LDKRHIContext* rhi, u32 shader, u32 stage)
{
  static const char version[] = "#version 330 core\n";
  static const char instanced[] = "#define LDK_INSTANCED\n";
  const char* code;
  size_t code_size;
  char* owned_code = NULL;
  LDKRHIShaderModuleDesc desc = {0};
  LDKRHIShaderModule result;

  if (!rhi)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  code = ldk_rhi_gl33_builtin_shader_source(shader, stage);
  if (!code)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  code_size = strlen(code);
  if (stage == LDK_RHI_SHADER_STAGE_VERTEX &&
      ldk_rhi_gl33_shader_is_instanced(shader))
  {
    const size_t prefix_size = sizeof(instanced) - 1u;
    const size_t version_size = sizeof(version) - 1u;
    const bool has_version = code_size >= version_size &&
        memcmp(code, version, version_size) == 0;
    const size_t insert_at = has_version ? version_size : 0u;

    if (code_size > SIZE_MAX - prefix_size - 1u)
    {
      return LDK_RHI_INVALID_RESOURCE;
    }
    owned_code = (char*)malloc(code_size + prefix_size + 1u);
    if (!owned_code)
    {
      return LDK_RHI_INVALID_RESOURCE;
    }

    memcpy(owned_code, code, insert_at);
    memcpy(owned_code + insert_at, instanced, prefix_size);
    memcpy(owned_code + insert_at + prefix_size,
        code + insert_at, code_size - insert_at);
    code_size += prefix_size;
    owned_code[code_size] = '\0';
    code = owned_code;
  }

  ldk_rhi_shader_module_desc_defaults(&desc);
  desc.stage = stage;
  desc.code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
  desc.code = code;
  desc.code_size = (u32)code_size;
  result = ldk_rhi_shader_module_create(rhi, &desc);
  free(owned_code);
  return result;
}
