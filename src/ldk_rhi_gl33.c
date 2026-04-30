#include "ldk_rhi_gl33.h"
#include "ldk_renderer.h"

#include <ldk_gl.h>

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
} LDKRHIGL33TextureInfo;

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
  LDKRHIGL33BindingsLayoutEntry entries[LDK_RHI_MAX_BINDINGS];
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
  LDKRHIGL33BindingObject bindings[LDK_RHI_MAX_BINDINGS];
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
} LDKRHIGL33PipelineObject;

typedef struct LDKRHIGL33Backend
{
  LDKRHIGL33TextureInfo* textures;
  uint32_t texture_capacity;

  LDKRHIGL33BindingsLayoutObject* bindings_layouts;
  uint32_t bindings_layout_capacity;

  LDKRHIGL33BindingsObject* bindings;
  uint32_t bindings_capacity;

  LDKRHIGL33PipelineObject* pipelines;
  uint32_t pipeline_capacity;

  GLuint current_fbo;
  LDKRHIPipeline current_pipeline;
  LDKRHIBuffer current_vertex_buffer;
  uint32_t current_vertex_buffer_offset;
  LDKRHIBuffer current_index_buffer;
  uint32_t current_index_buffer_offset;
  LDKRHIIndexType current_index_type;
} LDKRHIGL33Backend;

static char const* LDK_RHI_GL33_UI_PASS_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec2 a_position;\n"
"layout(location = 1) in vec2 a_uv;\n"
"layout(location = 2) in vec4 a_color;\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  vec2 u_viewport_size;\n"
"};\n"
"out vec2 v_uv;\n"
"out vec4 v_color;\n"
"void main()\n"
"{\n"
"  vec2 ndc = vec2(\n"
"    (a_position.x / u_viewport_size.x) * 2.0 - 1.0,\n"
"    1.0 - (a_position.y / u_viewport_size.y) * 2.0\n"
"  );\n"
"  v_uv = a_uv;\n"
"  v_color = a_color;\n"
"  gl_Position = vec4(ndc, 0.0, 1.0);\n"
"}\n";

static char const* LDK_RHI_GL33_UI_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"in vec4 v_color;\n"
"out vec4 out_color;\n"
"uniform sampler2D LDK_TEXTURE_1;\n"
"void main()\n"
"{\n"
"  vec4 tex = texture(LDK_TEXTURE_1, v_uv);\n"
"  out_color = tex * v_color;\n"
"}\n";

static uint32_t ldk_rhi_gl33_cstr_size(char const* cstr)
{
  return (uint32_t)strlen(cstr);
}

static char const* ldk_rhi_gl33_builtin_shader_source(uint32_t shader, uint32_t stage)
{
  if (shader == LDK_SHADER_UI_PASS && stage == LDK_RHI_SHADER_STAGE_VERTEX)
  {
    return LDK_RHI_GL33_UI_PASS_VERTEX_SHADER;
  }

  if (shader == LDK_SHADER_UI_PASS && stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_UI_PASS_FRAGMENT_SHADER;
  }

  return NULL;
}

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(LDKRHIContext* rhi, uint32_t shader, uint32_t stage)
{
  if (rhi == NULL)
  {
    return LDK_RHI_INVALID_SHADER_MODULE;
  }

  char const* code = ldk_rhi_gl33_builtin_shader_source(shader, stage);
  if (code == NULL)
  {
    return LDK_RHI_INVALID_SHADER_MODULE;
  }

  LDKRHIShaderModuleDesc desc = {0};
  ldk_rhi_shader_module_desc_defaults(&desc);
  desc.stage = stage;
  desc.code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
  desc.code = code;
  desc.code_size = ldk_rhi_gl33_cstr_size(code);
  return ldk_rhi_create_shader_module(rhi, &desc);
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
    return LDK_RHI_INVALID_BINDINGS_LAYOUT;
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
    return LDK_RHI_INVALID_BINDINGS;
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
    return LDK_RHI_INVALID_PIPELINE;
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
  glBindBuffer(GL_ARRAY_BUFFER, backend->current_vertex_buffer);

  for (uint32_t i = 0; i < pipeline->vertex_layout.attribute_count; i++)
  {
    LDKRHIVertexAttributeDesc attribute = pipeline->vertex_layout.attributes[i];
    GLint count = 0;
    GLenum type = 0;
    GLboolean normalized = GL_FALSE;
    ldk_rhi_gl33_vertex_format(attribute.format, &count, &type, &normalized);
    uintptr_t offset = (uintptr_t)backend->current_vertex_buffer_offset + (uintptr_t)attribute.offset;
    glEnableVertexAttribArray(attribute.location);
    glVertexAttribPointer(attribute.location, count, type, normalized, (GLsizei)pipeline->vertex_layout.stride, (const void*)offset);
  }
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

  free(backend->textures);
  free(backend->bindings_layouts);
  free(backend->bindings);
  free(backend->pipelines);
  free(backend);
}

static LDKRHIBuffer ldk_rhi_gl33_create_buffer(void* backend_user_data, const LDKRHIBufferDesc* desc)
{
  (void)backend_user_data;
  GLuint buffer = 0;
  GLenum target = ldk_rhi_gl33_buffer_target(desc->usage);
  GLenum usage = ldk_rhi_gl33_buffer_usage(desc->memory_usage);
  glGenBuffers(1, &buffer);
  glBindBuffer(target, buffer);
  glBufferData(target, (GLsizeiptr)desc->size, desc->initial_data, usage);
  glBindBuffer(target, 0);
  return (LDKRHIBuffer)buffer;
}

static void ldk_rhi_gl33_destroy_buffer(void* backend_user_data, LDKRHIBuffer buffer)
{
  (void)backend_user_data;
  GLuint gl_buffer = (GLuint)buffer;
  glDeleteBuffers(1, &gl_buffer);
}

static bool ldk_rhi_gl33_update_buffer(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data)
{
  (void)backend_user_data;
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buffer);
  glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
  return true;
}

static LDKRHITexture ldk_rhi_gl33_create_texture(void* backend_user_data, const LDKRHITextureDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint texture = 0;
  GLenum target = ldk_rhi_gl33_texture_target(desc->type);
  GLenum internal_format = ldk_rhi_gl33_internal_format(desc->format);
  GLenum external_format = ldk_rhi_gl33_external_format(desc->format);
  GLenum external_type = ldk_rhi_gl33_external_type(desc->format);
  glGenTextures(1, &texture);
  glBindTexture(target, texture);

  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

  if (desc->mip_count > 1)
  {
    glGenerateMipmap(target);
  }

  bool ok = ldk_rhi_gl33_grow((void**)&backend->textures, &backend->texture_capacity, sizeof(backend->textures[0]), texture + 1);
  if (ok)
  {
    backend->textures[texture].target = target;
    backend->textures[texture].format = desc->format;
    backend->textures[texture].width = desc->width;
    backend->textures[texture].height = desc->height;
    backend->textures[texture].depth = desc->depth;
  }

  glBindTexture(target, 0);
  return (LDKRHITexture)texture;
}

static void ldk_rhi_gl33_destroy_texture(void* backend_user_data, LDKRHITexture texture)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint gl_texture = (GLuint)texture;
  glDeleteTextures(1, &gl_texture);

  if (texture < backend->texture_capacity)
  {
    memset(&backend->textures[texture], 0, sizeof(backend->textures[texture]));
  }
}

static bool ldk_rhi_gl33_update_texture(void* backend_user_data, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size)
{
  (void)layer;
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

  return true;
}

static LDKRHISampler ldk_rhi_gl33_create_sampler(void* backend_user_data, const LDKRHISamplerDesc* desc)
{
  (void)backend_user_data;
  GLuint sampler = 0;
  glGenSamplers(1, &sampler);
  glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, ldk_rhi_gl33_filter(desc->min_filter));
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

static LDKRHIShaderModule ldk_rhi_gl33_create_shader_module(void* backend_user_data, const LDKRHIShaderModuleDesc* desc)
{
  (void)backend_user_data;
  if (desc->code_format != LDK_RHI_SHADER_CODE_FORMAT_GLSL)
  {
    return LDK_RHI_INVALID_SHADER_MODULE;
  }

  GLenum stage = ldk_rhi_gl33_shader_stage(desc->stage);
  if (stage == 0)
  {
    return LDK_RHI_INVALID_SHADER_MODULE;
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
    return LDK_RHI_INVALID_SHADER_MODULE;
  }

  return (LDKRHIShaderModule)shader;
}

static void ldk_rhi_gl33_destroy_shader_module(void* backend_user_data, LDKRHIShaderModule shader_module)
{
  (void)backend_user_data;
  GLuint shader = (GLuint)shader_module;
  glDeleteShader(shader);
}

static LDKRHIBindingsLayout ldk_rhi_gl33_create_bindings_layout(void* backend_user_data, const LDKRHIBindingsLayoutDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  LDKRHIBindingsLayout handle = ldk_rhi_gl33_alloc_bindings_layout(backend);
  if (handle == LDK_RHI_INVALID_BINDINGS_LAYOUT)
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

static void ldk_rhi_gl33_destroy_bindings_layout(void* backend_user_data, LDKRHIBindingsLayout bindings_layout)
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
  if (bindings_layout == LDK_RHI_INVALID_BINDINGS_LAYOUT)
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

static LDKRHIPipeline ldk_rhi_gl33_create_pipeline(void* backend_user_data, const LDKRHIPipelineDesc* desc)
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
    return LDK_RHI_INVALID_PIPELINE;
  }

  ldk_rhi_gl33_apply_program_bindings(backend, program, desc->bindings_layout);

  LDKRHIPipeline handle = ldk_rhi_gl33_alloc_pipeline(backend);
  if (handle == LDK_RHI_INVALID_PIPELINE)
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
  return handle;
}

static void ldk_rhi_gl33_destroy_pipeline(void* backend_user_data, LDKRHIPipeline pipeline)
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

static LDKRHIBindings ldk_rhi_gl33_create_bindings(LDKRHIGL33Backend* backend, const LDKRHIBindingsDesc* desc)
{
  if (!desc)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  if (desc->binding_count > LDK_RHI_MAX_BINDINGS)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  if (desc->layout >= backend->bindings_layout_capacity || !backend->bindings_layouts[desc->layout].alive)
  {
    return LDK_RHI_INVALID_BINDINGS;
  }

  LDKRHIGL33BindingsLayoutObject* layout = &backend->bindings_layouts[desc->layout];

  uint32_t index = ldk_rhi_gl33_alloc_bindings(backend);

  if (index == LDK_RHI_INVALID_BINDINGS)
  {
    return LDK_RHI_INVALID_BINDINGS;
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
      return LDK_RHI_INVALID_BINDINGS;
    }
  }

  return index;
}

static void ldk_rhi_gl33_destroy_bindings(void* backend_user_data, LDKRHIBindings bindings)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (bindings >= backend->bindings_capacity)
  {
    return;
  }

  memset(&backend->bindings[bindings], 0, sizeof(backend->bindings[bindings]));
}

static void ldk_rhi_gl33_begin_frame(void* backend_user_data)
{
  (void)backend_user_data;
}

static void ldk_rhi_gl33_end_frame(void* backend_user_data)
{
  (void)backend_user_data;
}

static void ldk_rhi_gl33_begin_pass(void* backend_user_data, const LDKRHIPassDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  GLuint fbo = 0;
  bool use_default_framebuffer = false;

  if (desc->color_attachment_count == 1 && desc->color_attachments[0].texture == LDK_RHI_INVALID_TEXTURE)
  {
    use_default_framebuffer = true;
  }

  if (use_default_framebuffer)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  else if (desc->color_attachment_count > 0 || desc->depth_attachment.valid)
  {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLenum draw_buffers[LDK_RHI_MAX_COLOR_ATTACHMENTS];
    for (uint32_t i = 0; i < desc->color_attachment_count; i++)
    {
      GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, (GLuint)desc->color_attachments[i].texture, (GLint)desc->color_attachments[i].mip_level);
      draw_buffers[i] = attachment;
    }

    if (desc->color_attachment_count > 0)
    {
      glDrawBuffers((GLsizei)desc->color_attachment_count, draw_buffers);
    }

    if (desc->depth_attachment.valid)
    {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (GLuint)desc->depth_attachment.texture, (GLint)desc->depth_attachment.mip_level);
    }
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  backend->current_fbo = fbo;

  if (desc->has_viewport)
  {
    glViewport((GLint)desc->viewport.x, (GLint)desc->viewport.y, (GLsizei)desc->viewport.width, (GLsizei)desc->viewport.height);
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

    glClear(clear_mask);

    if (restore_scissor)
    {
      glEnable(GL_SCISSOR_TEST);
      glScissor(restore_scissor_box[0], restore_scissor_box[1], restore_scissor_box[2], restore_scissor_box[3]);
    }
  }
}

static void ldk_rhi_gl33_end_pass(void* backend_user_data)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_fbo != 0)
  {
    GLuint fbo = backend->current_fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    backend->current_fbo = 0;
  }
}

static void ldk_rhi_gl33_bind_pipeline(void* backend_user_data, LDKRHIPipeline pipeline)
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

  if (backend->current_vertex_buffer != LDK_RHI_INVALID_BUFFER)
  {
    ldk_rhi_gl33_apply_vertex_layout(backend, object);
  }
}

static void ldk_rhi_gl33_bind_bindings(void* backend_user_data, LDKRHIBindings bindings)
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

static void ldk_rhi_gl33_bind_vertex_buffer(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  backend->current_vertex_buffer = buffer;
  backend->current_vertex_buffer_offset = offset;

  if (backend->current_pipeline != LDK_RHI_INVALID_PIPELINE && backend->current_pipeline < backend->pipeline_capacity)
  {
    LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
    if (pipeline->alive)
    {
      ldk_rhi_gl33_apply_vertex_layout(backend, pipeline);
    }
  }
}

static void ldk_rhi_gl33_bind_index_buffer(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  backend->current_index_buffer = buffer;
  backend->current_index_buffer_offset = offset;
  backend->current_index_type = index_type;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)buffer);
}

static void ldk_rhi_gl33_set_viewport(void* backend_user_data, const LDKRHIViewport* viewport)
{
  (void)backend_user_data;
  glViewport((GLint)viewport->x, (GLint)viewport->y, (GLsizei)viewport->width, (GLsizei)viewport->height);
  glDepthRange(viewport->min_depth, viewport->max_depth);
}

static void ldk_rhi_gl33_set_scissor(void* backend_user_data, const LDKRHIRect* scissor)
{
  (void)backend_user_data;
  glEnable(GL_SCISSOR_TEST);
  glScissor(scissor->x, scissor->y, scissor->width, scissor->height);
}

static void ldk_rhi_gl33_draw(void* backend_user_data, const LDKRHIDrawDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_PIPELINE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
  GLenum mode = ldk_rhi_gl33_topology(pipeline->topology);
  glDrawArrays(mode, (GLint)desc->first_vertex, (GLsizei)desc->vertex_count);
}

static void ldk_rhi_gl33_draw_indexed(void* backend_user_data, const LDKRHIDrawIndexedDesc* desc)
{
  LDKRHIGL33Backend* backend = (LDKRHIGL33Backend*)backend_user_data;
  if (backend->current_pipeline == LDK_RHI_INVALID_PIPELINE || backend->current_pipeline >= backend->pipeline_capacity)
  {
    return;
  }

  LDKRHIGL33PipelineObject* pipeline = &backend->pipelines[backend->current_pipeline];
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
  functions.create_buffer = ldk_rhi_gl33_create_buffer;
  functions.destroy_buffer = ldk_rhi_gl33_destroy_buffer;
  functions.update_buffer = ldk_rhi_gl33_update_buffer;
  functions.create_texture = ldk_rhi_gl33_create_texture;
  functions.destroy_texture = ldk_rhi_gl33_destroy_texture;
  functions.update_texture = ldk_rhi_gl33_update_texture;
  functions.create_sampler = ldk_rhi_gl33_create_sampler;
  functions.destroy_sampler = ldk_rhi_gl33_destroy_sampler;
  functions.create_shader_module = ldk_rhi_gl33_create_shader_module;
  functions.destroy_shader_module = ldk_rhi_gl33_destroy_shader_module;
  functions.create_bindings_layout = ldk_rhi_gl33_create_bindings_layout;
  functions.destroy_bindings_layout = ldk_rhi_gl33_destroy_bindings_layout;
  functions.create_pipeline = ldk_rhi_gl33_create_pipeline;
  functions.destroy_pipeline = ldk_rhi_gl33_destroy_pipeline;
  functions.create_bindings = ldk_rhi_gl33_create_bindings;
  functions.destroy_bindings = ldk_rhi_gl33_destroy_bindings;
  functions.begin_frame = ldk_rhi_gl33_begin_frame;
  functions.end_frame = ldk_rhi_gl33_end_frame;
  functions.begin_pass = ldk_rhi_gl33_begin_pass;
  functions.end_pass = ldk_rhi_gl33_end_pass;
  functions.bind_pipeline = ldk_rhi_gl33_bind_pipeline;
  functions.bind_bindings = ldk_rhi_gl33_bind_bindings;
  functions.bind_vertex_buffer = ldk_rhi_gl33_bind_vertex_buffer;
  functions.bind_index_buffer = ldk_rhi_gl33_bind_index_buffer;
  functions.set_viewport = ldk_rhi_gl33_set_viewport;
  functions.set_scissor = ldk_rhi_gl33_set_scissor;
  functions.draw = ldk_rhi_gl33_draw;
  functions.draw_indexed = ldk_rhi_gl33_draw_indexed;

  bool ok = ldk_rhi_initialize(context, &desc, &functions);
  if (!ok)
  {
    free(backend);
    return false;
  }

  return true;
}
