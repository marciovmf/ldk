/**
 * @file ldk_rhi.h
 *
 * Thin Render Hardware Interface used by the engine renderer.
 *
 * The RHI is the backend-agnostic layer that owns GPU-like resources and draw
 * commands. It knows nothing about engine systems, modules or anything else
 * except buffers, textures, samplers, shaders, pipelines, bindings, passes, and draw calls.
 *
 * Typical usage:
 *
 * @code
 * LDKRHIContext rhi;
 * LDKRHIContextDesc context_desc;
 * LDKRHIFunctions functions;
 *
 * // Initialize RHI pointers. Probably via some actual rendering API wrapper like Vulkan, GL or whatever
 * ldk_rhi_initialize(&rhi, &context_desc, &functions);
 *
 * ldk_rhi_begin_frame(&rhi);
 *
 * LDKRHIPassDesc pass;
 * ldk_rhi_pass_desc_defaults(&pass);
 * pass.color_attachment_count = 1;
 * pass.color_attachments[0].texture = LDK_RHI_TEXTURE_INVALID;
 * pass.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
 * pass.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;
 * pass.has_viewport = true;
 * pass.viewport.width = (float)framebuffer_width;
 * pass.viewport.height = (float)framebuffer_height;
 * pass.viewport.min_depth = 0.0f;
 * pass.viewport.max_depth = 1.0f;
 *
 * ldk_rhi_begin_pass(&rhi, &pass);
 * ldk_rhi_bind_pipeline(&rhi, pipeline);
 * ldk_rhi_bind_bindings(&rhi, bindings);
 * ldk_rhi_bind_vertex_buffer(&rhi, vertex_buffer, 0);
 * ldk_rhi_bind_index_buffer(&rhi, index_buffer, 0, LDK_RHI_INDEX_TYPE_UINT32);
 * ldk_rhi_draw_indexed(&rhi, &draw_desc);
 * ldk_rhi_end_pass(&rhi);
 *
 * ldk_rhi_end_frame(&rhi);
 * ldk_rhi_terminate(&rhi);
 * @endcode
 *
 * Binding slots have no semantic meaning inside the RHI. The renderer defines
 * the ABI for built-in shaders. The RHI only sees slot, binding type, shader
 * stages, and resource handles.
 *
 * Texture swizzle can be used for single-channel font atlases. An R8 atlas that
 * stores glyph coverage in the red channel can be sampled as white RGB with
 * alpha from red by using swizzle { ONE, ONE, ONE, R }.
 */
#ifndef LDK_RHI_H
#define LDK_RHI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  typedef uint32_t LDKRHIBuffer;
  typedef uint32_t LDKRHITexture;
  typedef uint32_t LDKRHISampler;
  typedef uint32_t LDKRHIShaderModule;
  typedef uint32_t LDKRHIBindingsLayout;
  typedef uint32_t LDKRHIPipeline;
  typedef uint32_t LDKRHIBindings;

  /* Compatibility alias. Prefer LDKRHIShaderModule in new code. */
  typedef LDKRHIShaderModule LDKRHIShader;

  typedef struct LDKRHIContext LDKRHIContext;

  enum
  {
    LDK_RHI_INVALID_BUFFER = 0,
    LDK_RHI_INVALID_TEXTURE = 0,
    LDK_RHI_INVALID_SAMPLER = 0,
    LDK_RHI_INVALID_SHADER_MODULE = 0,
    LDK_RHI_INVALID_SHADER = 0,
    LDK_RHI_INVALID_BINDINGS_LAYOUT = 0,
    LDK_RHI_INVALID_PIPELINE = 0,
    LDK_RHI_INVALID_BINDINGS = 0,
    LDK_RHI_SHADER_INVALID = 0,
  };

  /* Compatibility aliases. */

  typedef enum LDKRHIBackendType
  {
    LDK_RHI_BACKEND_NONE = 0,
    LDK_RHI_BACKEND_OPENGL33
  } LDKRHIBackendType;

  typedef enum LDKRHIBufferUsage
  {
    LDK_RHI_BUFFER_USAGE_NONE = 0,
    LDK_RHI_BUFFER_USAGE_VERTEX = 1 << 0,
    LDK_RHI_BUFFER_USAGE_INDEX = 1 << 1,
    LDK_RHI_BUFFER_USAGE_UNIFORM = 1 << 2,
    LDK_RHI_BUFFER_USAGE_STORAGE = 1 << 3,
    LDK_RHI_BUFFER_USAGE_TRANSFER_SRC = 1 << 4,
    LDK_RHI_BUFFER_USAGE_TRANSFER_DST = 1 << 5
  } LDKRHIBufferUsage;

  typedef enum LDKRHIMemoryUsage
  {
    LDK_RHI_MEMORY_USAGE_GPU = 0,
    LDK_RHI_MEMORY_USAGE_CPU_TO_GPU,
    LDK_RHI_MEMORY_USAGE_GPU_TO_CPU
  } LDKRHIMemoryUsage;

  typedef enum LDKRHITextureType
  {
    LDK_RHI_TEXTURE_TYPE_2D = 0,
    LDK_RHI_TEXTURE_TYPE_3D,
    LDK_RHI_TEXTURE_TYPE_CUBE
  } LDKRHITextureType;

  typedef enum LDKRHITextureUsage
  {
    LDK_RHI_TEXTURE_USAGE_NONE = 0,
    LDK_RHI_TEXTURE_USAGE_SAMPLED = 1 << 0,
    LDK_RHI_TEXTURE_USAGE_RENDER_TARGET = 1 << 1,
    LDK_RHI_TEXTURE_USAGE_DEPTH_STENCIL = 1 << 2,
    LDK_RHI_TEXTURE_USAGE_STORAGE = 1 << 3,
    LDK_RHI_TEXTURE_USAGE_TRANSFER_SRC = 1 << 4,
    LDK_RHI_TEXTURE_USAGE_TRANSFER_DST = 1 << 5
  } LDKRHITextureUsage;

  typedef enum LDKRHIFormat
  {
    LDK_RHI_FORMAT_INVALID = 0,
    LDK_RHI_FORMAT_R8_UNORM,
    LDK_RHI_FORMAT_RG8_UNORM,
    LDK_RHI_FORMAT_RGBA8_UNORM,
    LDK_RHI_FORMAT_RGBA8_SRGB,
    LDK_RHI_FORMAT_BGRA8_UNORM,
    LDK_RHI_FORMAT_BGRA8_SRGB,
    LDK_RHI_FORMAT_R16_FLOAT,
    LDK_RHI_FORMAT_RG16_FLOAT,
    LDK_RHI_FORMAT_RGBA16_FLOAT,
    LDK_RHI_FORMAT_R32_FLOAT,
    LDK_RHI_FORMAT_RG32_FLOAT,
    LDK_RHI_FORMAT_RGBA32_FLOAT,
    LDK_RHI_FORMAT_D24S8,
    LDK_RHI_FORMAT_D32_FLOAT
  } LDKRHIFormat;

  typedef enum LDKRHITextureSwizzle
  {
    LDK_RHI_TEXTURE_SWIZZLE_IDENTITY = 0,
    LDK_RHI_TEXTURE_SWIZZLE_ZERO,
    LDK_RHI_TEXTURE_SWIZZLE_ONE,
    LDK_RHI_TEXTURE_SWIZZLE_R,
    LDK_RHI_TEXTURE_SWIZZLE_G,
    LDK_RHI_TEXTURE_SWIZZLE_B,
    LDK_RHI_TEXTURE_SWIZZLE_A
  } LDKRHITextureSwizzle;

  typedef enum LDKRHIFilter
  {
    LDK_RHI_FILTER_NEAREST = 0,
    LDK_RHI_FILTER_LINEAR
  } LDKRHIFilter;

  typedef enum LDKRHIWrap
  {
    LDK_RHI_WRAP_REPEAT = 0,
    LDK_RHI_WRAP_CLAMP_TO_EDGE,
    LDK_RHI_WRAP_MIRROR,
    LDK_RHI_WRAP_MIRRORED_REPEAT = LDK_RHI_WRAP_MIRROR
  } LDKRHIWrap;

  typedef enum LDKRHIShaderStage
  {
    LDK_RHI_SHADER_STAGE_NONE = 0,
    LDK_RHI_SHADER_STAGE_VERTEX = 1 << 0,
    LDK_RHI_SHADER_STAGE_FRAGMENT = 1 << 1,
    LDK_RHI_SHADER_STAGE_COMPUTE = 1 << 2
  } LDKRHIShaderStage;

  typedef enum LDKRHIShaderCodeFormat
  {
    LDK_RHI_SHADER_CODE_FORMAT_GLSL = 0,
    LDK_RHI_SHADER_CODE_FORMAT_SPIRV,
    LDK_RHI_SHADER_CODE_FORMAT_DXIL,
    LDK_RHI_SHADER_CODE_FORMAT_MSL
  } LDKRHIShaderCodeFormat;

  /* Compatibility name. */
  typedef enum LDKRHIShaderSourceType
  {
    LDK_RHI_SHADER_SOURCE_TEXT = LDK_RHI_SHADER_CODE_FORMAT_GLSL,
    LDK_RHI_SHADER_SOURCE_BINARY = LDK_RHI_SHADER_CODE_FORMAT_SPIRV
  } LDKRHIShaderSourceType;

  typedef enum LDKRHIPrimitiveTopology
  {
    LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES = 0,
    LDK_RHI_PRIMITIVE_TOPOLOGY_LINES
  } LDKRHIPrimitiveTopology;

  typedef enum LDKRHICullMode
  {
    LDK_RHI_CULL_MODE_NONE = 0,
    LDK_RHI_CULL_MODE_FRONT,
    LDK_RHI_CULL_MODE_BACK
  } LDKRHICullMode;

  typedef enum LDKRHIFrontFace
  {
    LDK_RHI_FRONT_FACE_CCW = 0,
    LDK_RHI_FRONT_FACE_CW
  } LDKRHIFrontFace;

  typedef enum LDKRHICompareOp
  {
    LDK_RHI_COMPARE_OP_NEVER = 0,
    LDK_RHI_COMPARE_OP_LESS,
    LDK_RHI_COMPARE_OP_LESS_EQUAL,
    LDK_RHI_COMPARE_OP_EQUAL,
    LDK_RHI_COMPARE_OP_GREATER,
    LDK_RHI_COMPARE_OP_GREATER_EQUAL,
    LDK_RHI_COMPARE_OP_NOT_EQUAL,
    LDK_RHI_COMPARE_OP_ALWAYS
  } LDKRHICompareOp;

  typedef enum LDKRHIBlendFactor
  {
    LDK_RHI_BLEND_FACTOR_ZERO = 0,
    LDK_RHI_BLEND_FACTOR_ONE,
    LDK_RHI_BLEND_FACTOR_SRC_COLOR,
    LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    LDK_RHI_BLEND_FACTOR_DST_COLOR,
    LDK_RHI_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    LDK_RHI_BLEND_FACTOR_SRC_ALPHA,
    LDK_RHI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    LDK_RHI_BLEND_FACTOR_DST_ALPHA,
    LDK_RHI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA
  } LDKRHIBlendFactor;

  typedef enum LDKRHIBlendOp
  {
    LDK_RHI_BLEND_OP_ADD = 0,
    LDK_RHI_BLEND_OP_SUBTRACT,
    LDK_RHI_BLEND_OP_REVERSE_SUBTRACT
  } LDKRHIBlendOp;

  typedef enum LDKRHIVertexFormat
  {
    LDK_RHI_VERTEX_FORMAT_FLOAT = 0,
    LDK_RHI_VERTEX_FORMAT_FLOAT2,
    LDK_RHI_VERTEX_FORMAT_FLOAT3,
    LDK_RHI_VERTEX_FORMAT_FLOAT4,
    LDK_RHI_VERTEX_FORMAT_UBYTE4_NORM
  } LDKRHIVertexFormat;

  typedef enum LDKRHIBindingType
  {
    LDK_RHI_BINDING_TYPE_UNIFORM_BUFFER = 0,
    LDK_RHI_BINDING_TYPE_STORAGE_BUFFER,
    LDK_RHI_BINDING_TYPE_TEXTURE,
    LDK_RHI_BINDING_TYPE_SAMPLER,
    LDK_RHI_BINDING_TYPE_TEXTURE_SAMPLER
  } LDKRHIBindingType;

  typedef enum LDKRHILoadOp
  {
    LDK_RHI_LOAD_OP_LOAD = 0,
    LDK_RHI_LOAD_OP_CLEAR,
    LDK_RHI_LOAD_OP_DONT_CARE
  } LDKRHILoadOp;

  typedef enum LDKRHIStoreOp
  {
    LDK_RHI_STORE_OP_STORE = 0,
    LDK_RHI_STORE_OP_DONT_CARE
  } LDKRHIStoreOp;

  typedef enum LDKRHIIndexType
  {
    LDK_RHI_INDEX_TYPE_UINT16 = 0,
    LDK_RHI_INDEX_TYPE_UINT32
  } LDKRHIIndexType;

  typedef struct LDKRHIColor
  {
    float r;
    float g;
    float b;
    float a;
  } LDKRHIColor;

  typedef struct LDKRHIViewport
  {
    float x;
    float y;
    float width;
    float height;
    float min_depth;
    float max_depth;
  } LDKRHIViewport;

  typedef struct LDKRHIRect
  {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
  } LDKRHIRect;

  typedef struct LDKRHIBufferDesc
  {
    uint32_t size;
    uint32_t usage;
    LDKRHIMemoryUsage memory_usage;
    const void* initial_data;
  } LDKRHIBufferDesc;

  typedef struct LDKRHITextureDesc
  {
    LDKRHITextureType type;
    LDKRHIFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mip_count;
    uint32_t layer_count;
    uint32_t usage;
    const void* initial_data;
    uint32_t initial_data_size;
    LDKRHITextureSwizzle swizzle_r;
    LDKRHITextureSwizzle swizzle_g;
    LDKRHITextureSwizzle swizzle_b;
    LDKRHITextureSwizzle swizzle_a;
  } LDKRHITextureDesc;

  typedef struct LDKRHISamplerDesc
  {
    LDKRHIFilter min_filter;
    LDKRHIFilter mag_filter;
    LDKRHIFilter mip_filter;
    LDKRHIWrap wrap_u;
    LDKRHIWrap wrap_v;
    LDKRHIWrap wrap_w;
  } LDKRHISamplerDesc;

  typedef struct LDKRHIShaderModuleDesc
  {
    uint32_t stage;
    LDKRHIShaderCodeFormat code_format;
    const void* code;
    uint32_t code_size;
    const char* entry_point;
  } LDKRHIShaderModuleDesc;

  /* Compatibility desc. */
  typedef struct LDKRHIShaderDesc
  {
    uint32_t stage;
    LDKRHIShaderSourceType source_type;
    const void* data;
    uint32_t size;
  } LDKRHIShaderDesc;

  typedef struct LDKRHIBlendState
  {
    bool enabled;
    LDKRHIBlendFactor src_color_factor;
    LDKRHIBlendFactor dst_color_factor;
    LDKRHIBlendOp color_op;
    LDKRHIBlendFactor src_alpha_factor;
    LDKRHIBlendFactor dst_alpha_factor;
    LDKRHIBlendOp alpha_op;
  } LDKRHIBlendState;

  typedef struct LDKRHIDepthState
  {
    bool test_enabled;
    bool write_enabled;
    LDKRHICompareOp compare_op;
  } LDKRHIDepthState;

  typedef struct LDKRHIRasterState
  {
    LDKRHICullMode cull_mode;
    LDKRHIFrontFace front_face;
    bool scissor_enabled;
  } LDKRHIRasterState;

  typedef struct LDKRHIVertexAttributeDesc
  {
    uint32_t location;
    LDKRHIVertexFormat format;
    uint32_t offset;
  } LDKRHIVertexAttributeDesc;

#define LDK_RHI_VERTEX_ATTRIBUTE_MAX 16
#define LDK_RHI_COLOR_ATTACHMENT_MAX 8
#define LDK_RHI_BINDING_MAX 16

  /* Compatibility max names. */
#define LDK_RHI_MAX_VERTEX_ATTRIBUTES LDK_RHI_VERTEX_ATTRIBUTE_MAX
#define LDK_RHI_MAX_COLOR_ATTACHMENTS LDK_RHI_COLOR_ATTACHMENT_MAX
#define LDK_RHI_MAX_BINDINGS LDK_RHI_BINDING_MAX

  typedef struct LDKRHIVertexBufferLayoutDesc
  {
    uint32_t stride;
    uint32_t attribute_count;
    LDKRHIVertexAttributeDesc attributes[LDK_RHI_VERTEX_ATTRIBUTE_MAX];
  } LDKRHIVertexBufferLayoutDesc;

  typedef struct LDKRHIBindingLayoutEntry
  {
    uint32_t slot;
    LDKRHIBindingType type;
    uint32_t stages;
  } LDKRHIBindingLayoutEntry;

  typedef struct LDKRHIBindingsLayoutDesc
  {
    uint32_t entry_count;
    LDKRHIBindingLayoutEntry entries[LDK_RHI_BINDING_MAX];
  } LDKRHIBindingsLayoutDesc;

  typedef struct LDKRHIPipelineDesc
  {
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPrimitiveTopology topology;
    LDKRHIBlendState blend_state;
    LDKRHIDepthState depth_state;
    LDKRHIRasterState raster_state;
    LDKRHIVertexBufferLayoutDesc vertex_layout;
    uint32_t color_attachment_count;
    LDKRHIFormat color_formats[LDK_RHI_COLOR_ATTACHMENT_MAX];
    LDKRHIFormat depth_format;
  } LDKRHIPipelineDesc;

  typedef struct LDKRHIBindingDesc
  {
    uint32_t slot;
    LDKRHIBindingType type;
    uint32_t stages;
    LDKRHIBuffer buffer;
    uint32_t buffer_offset;
    uint32_t buffer_size;
    LDKRHITexture texture;
    LDKRHISampler sampler;
  } LDKRHIBindingDesc;

  typedef struct LDKRHIBindingsDesc
  {
    LDKRHIBindingsLayout layout;
    uint32_t binding_count;
    LDKRHIBindingDesc bindings[LDK_RHI_BINDING_MAX];
  } LDKRHIBindingsDesc;

  typedef struct LDKRHIPassColorAttachmentDesc
  {
    LDKRHITexture texture;
    uint32_t mip_level;
    uint32_t layer;
    LDKRHILoadOp load_op;
    LDKRHIStoreOp store_op;
    LDKRHIColor clear_color;
  } LDKRHIPassColorAttachmentDesc;

  typedef struct LDKRHIPassDepthAttachmentDesc
  {
    bool valid;
    LDKRHITexture texture;
    uint32_t mip_level;
    uint32_t layer;
    LDKRHILoadOp depth_load_op;
    LDKRHIStoreOp depth_store_op;
    float clear_depth;
    uint32_t clear_stencil;
  } LDKRHIPassDepthAttachmentDesc;

  typedef struct LDKRHIPassDesc
  {
    uint32_t color_attachment_count;
    LDKRHIPassColorAttachmentDesc color_attachments[LDK_RHI_COLOR_ATTACHMENT_MAX];
    LDKRHIPassDepthAttachmentDesc depth_attachment;
    bool has_viewport;
    LDKRHIViewport viewport;
    bool has_scissor;
    LDKRHIRect scissor;
  } LDKRHIPassDesc;

  typedef struct LDKRHIContextDesc
  {
    LDKRHIBackendType backend_type;
    void* backend_api;
    void* backend_user_data;
  } LDKRHIContextDesc;

  typedef struct LDKRHIDrawIndexedDesc
  {
    uint32_t index_count;
    uint32_t first_index;
    int32_t vertex_offset;
  } LDKRHIDrawIndexedDesc;

  typedef struct LDKRHIDrawDesc
  {
    uint32_t vertex_count;
    uint32_t first_vertex;
  } LDKRHIDrawDesc;

  typedef struct LDKRHIFunctions
  {
    void (*shutdown)(void* backend_user_data);

    LDKRHIBuffer (*create_buffer)(void* backend_user_data, const LDKRHIBufferDesc* desc);
    void (*destroy_buffer)(void* backend_user_data, LDKRHIBuffer buffer);
    bool (*update_buffer)(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data);

    LDKRHITexture (*create_texture)(void* backend_user_data, const LDKRHITextureDesc* desc);
    void (*destroy_texture)(void* backend_user_data, LDKRHITexture texture);
    bool (*update_texture)(void* backend_user_data, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size);

    LDKRHISampler (*create_sampler)(void* backend_user_data, const LDKRHISamplerDesc* desc);
    void (*destroy_sampler)(void* backend_user_data, LDKRHISampler sampler);

    LDKRHIShaderModule (*create_shader_module)(void* backend_user_data, const LDKRHIShaderModuleDesc* desc);
    void (*destroy_shader_module)(void* backend_user_data, LDKRHIShaderModule shader_module);

    LDKRHIBindingsLayout (*create_bindings_layout)(void* backend_user_data, const LDKRHIBindingsLayoutDesc* desc);
    void (*destroy_bindings_layout)(void* backend_user_data, LDKRHIBindingsLayout bindings_layout);

    LDKRHIPipeline (*create_pipeline)(void* backend_user_data, const LDKRHIPipelineDesc* desc);
    void (*destroy_pipeline)(void* backend_user_data, LDKRHIPipeline pipeline);

    LDKRHIBindings (*create_bindings)(void* backend_user_data, const LDKRHIBindingsDesc* desc);
    void (*destroy_bindings)(void* backend_user_data, LDKRHIBindings bindings);

    void (*begin_frame)(void* backend_user_data);
    void (*end_frame)(void* backend_user_data);

    void (*begin_pass)(void* backend_user_data, const LDKRHIPassDesc* desc);
    void (*end_pass)(void* backend_user_data);

    void (*bind_pipeline)(void* backend_user_data, LDKRHIPipeline pipeline);
    void (*bind_bindings)(void* backend_user_data, LDKRHIBindings bindings);
    void (*bind_vertex_buffer)(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset);
    void (*bind_index_buffer)(void* backend_user_data, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type);

    void (*set_viewport)(void* backend_user_data, const LDKRHIViewport* viewport);
    void (*set_scissor)(void* backend_user_data, const LDKRHIRect* scissor);

    void (*draw)(void* backend_user_data, const LDKRHIDrawDesc* desc);
    void (*draw_indexed)(void* backend_user_data, const LDKRHIDrawIndexedDesc* desc);
  } LDKRHIFunctions;

  struct LDKRHIContext
  {
    LDKRHIBackendType backend_type;
    void* backend_user_data;
    LDKRHIFunctions functions;
  };

  /**
   * @brief Initializes a buffer descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_buffer_desc_defaults(LDKRHIBufferDesc* desc);

  /**
   * @brief Initializes a texture descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_texture_desc_defaults(LDKRHITextureDesc* desc);

  /**
   * @brief Initializes a sampler descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_sampler_desc_defaults(LDKRHISamplerDesc* desc);

  /**
   * @brief Initializes a shader module descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_shader_module_desc_defaults(LDKRHIShaderModuleDesc* desc);

  /**
   * @brief Initializes a shader descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_shader_desc_defaults(LDKRHIShaderDesc* desc);

  /**
   * @brief Initializes a blend state with default values.
   * @param state Pointer to the state to initialize.
   */
  void ldk_rhi_blend_state_defaults(LDKRHIBlendState* state);

  /**
   * @brief Initializes a depth state with default values.
   * @param state Pointer to the state to initialize.
   */
  void ldk_rhi_depth_state_defaults(LDKRHIDepthState* state);

  /**
   * @brief Initializes a raster state with default values.
   * @param state Pointer to the state to initialize.
   */
  void ldk_rhi_raster_state_defaults(LDKRHIRasterState* state);

  /**
   * @brief Initializes a pipeline descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_pipeline_desc_defaults(LDKRHIPipelineDesc* desc);

  /**
   * @brief Initializes a bindings layout descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_bindings_layout_desc_defaults(LDKRHIBindingsLayoutDesc* desc);

  /**
   * @brief Initializes a bindings descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_bindings_desc_defaults(LDKRHIBindingsDesc* desc);

  /**
   * @brief Initializes a pass descriptor with default values.
   * @param desc Pointer to the descriptor to initialize.
   */
  void ldk_rhi_pass_desc_defaults(LDKRHIPassDesc* desc);

  /**
   * @brief Initializes the RHI context.
   * @param context Pointer to the context to initialize.
   * @param desc Initialization descriptor.
   * @param functions Backend function table.
   * @return true if initialization succeeded, false otherwise.
   */
  bool ldk_rhi_initialize(LDKRHIContext* context, const LDKRHIContextDesc* desc, const LDKRHIFunctions* functions);

  /**
   * @brief Terminates the RHI context.
   * @param context Pointer to the context to terminate.
   */
  void ldk_rhi_terminate(LDKRHIContext* context);

  /**
   * @brief Checks if a buffer handle is valid.
   * @param buffer Buffer handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_buffer(LDKRHIBuffer buffer);

  /**
   * @brief Checks if a texture handle is valid.
   * @param texture Texture handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_texture(LDKRHITexture texture);

  /**
   * @brief Checks if a sampler handle is valid.
   * @param sampler Sampler handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_sampler(LDKRHISampler sampler);

  /**
   * @brief Checks if a shader module handle is valid.
   * @param shader_module Shader module handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader_module(LDKRHIShaderModule shader_module);

  /**
   * @brief Checks if a shader handle is valid.
   * @param shader Shader handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader(LDKRHIShader shader);

  /**
   * @brief Checks if a bindings layout handle is valid.
   * @param bindings_layout Bindings layout handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings_layout(LDKRHIBindingsLayout bindings_layout);

  /**
   * @brief Checks if a pipeline handle is valid.
   * @param pipeline Pipeline handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_pipeline(LDKRHIPipeline pipeline);

  /**
   * @brief Checks if a bindings handle is valid.
   * @param bindings Bindings handle.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings(LDKRHIBindings bindings);

  /**
   * @brief Validates a buffer descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_buffer_desc(const LDKRHIBufferDesc* desc);

  /**
   * @brief Validates a texture descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_texture_desc(const LDKRHITextureDesc* desc);

  /**
   * @brief Validates a sampler descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_sampler_desc(const LDKRHISamplerDesc* desc);

  /**
   * @brief Validates a shader module descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader_module_desc(const LDKRHIShaderModuleDesc* desc);

  /**
   * @brief Validates a shader descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader_desc(const LDKRHIShaderDesc* desc);

  /**
   * @brief Validates a bindings layout descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings_layout_desc(const LDKRHIBindingsLayoutDesc* desc);

  /**
   * @brief Validates a pipeline descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_pipeline_desc(const LDKRHIPipelineDesc* desc);

  /**
   * @brief Validates a bindings descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings_desc(const LDKRHIBindingsDesc* desc);

  /**
   * @brief Validates a pass descriptor.
   * @param desc Descriptor to validate.
   * @return true if valid, false otherwise.
   */
  bool ldk_rhi_is_valid_pass_desc(const LDKRHIPassDesc* desc);

  /**
   * @brief Creates a GPU buffer.
   * @param context RHI context.
   * @param desc Buffer descriptor.
   * @return A valid buffer handle on success, or an invalid handle on failure.
   */
  LDKRHIBuffer ldk_rhi_create_buffer(LDKRHIContext* context, const LDKRHIBufferDesc* desc);

  /**
   * @brief Destroys a GPU buffer.
   * @param context RHI context.
   * @param buffer Buffer handle.
   */
  void ldk_rhi_destroy_buffer(LDKRHIContext* context, LDKRHIBuffer buffer);

  /**
   * @brief Updates a region of a buffer.
   * @param context RHI context.
   * @param buffer Buffer handle.
   * @param offset Byte offset within the buffer.
   * @param size Size in bytes.
   * @param data Pointer to source data.
   * @return true if the update succeeded, false otherwise.
   */
  bool ldk_rhi_update_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data);

  /**
   * @brief Creates a GPU texture.
   * @param context RHI context.
   * @param desc Texture descriptor.
   * @return A valid texture handle on success, or an invalid handle on failure.
   */
  LDKRHITexture ldk_rhi_create_texture(LDKRHIContext* context, const LDKRHITextureDesc* desc);

  /**
   * @brief Destroys a GPU texture.
   * @param context RHI context.
   * @param texture Texture handle.
   */
  void ldk_rhi_destroy_texture(LDKRHIContext* context, LDKRHITexture texture);

  /**
   * @brief Updates a texture subresource.
   * @param context RHI context.
   * @param texture Texture handle.
   * @param mip_level Mip level to update.
   * @param layer Array layer to update.
   * @param data Pointer to source data.
   * @param size Size in bytes.
   * @return true if the update succeeded, false otherwise.
   */
  bool ldk_rhi_update_texture(LDKRHIContext* context, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size);

  /**
   * @brief Creates a sampler object.
   * @param context RHI context.
   * @param desc Sampler descriptor.
   *
   * @return A valid sampler handle on success, or an invalid handle on failure.
   */
  LDKRHISampler ldk_rhi_create_sampler(LDKRHIContext* context, const LDKRHISamplerDesc* desc);

  /**
   * @brief Destroys a sampler object.
   * @param context RHI context.
   * @param sampler Sampler handle.
   */
  void ldk_rhi_destroy_sampler(LDKRHIContext* context, LDKRHISampler sampler);

  /**
   * @brief Creates a shader module.
   * @param context RHI context.
   * @param desc Shader module descriptor.
   *
   * @return A valid shader module handle on success, or an invalid handle on failure.
   */
  LDKRHIShaderModule ldk_rhi_create_shader_module(LDKRHIContext* context, const LDKRHIShaderModuleDesc* desc);

  /**
   * @brief Destroys a shader module.
   * @param context RHI context.
   * @param shader_module Shader module handle.
   */
  void ldk_rhi_destroy_shader_module(LDKRHIContext* context, LDKRHIShaderModule shader_module);

  /**
   * @brief Creates a shader object.
   * @param context RHI context.
   * @param desc Shader descriptor.
   * @return A valid shader handle on success, or an invalid handle on failure.
   */
  LDKRHIShader ldk_rhi_create_shader(LDKRHIContext* context, const LDKRHIShaderDesc* desc);

  /**
   * @brief Destroys a shader object.
   * @param context RHI context.
   * @param shader Shader handle.
   */
  void ldk_rhi_destroy_shader(LDKRHIContext* context, LDKRHIShader shader);

  /**
   * @brief Creates a bindings layout.
   * @param context RHI context.
   * @param desc Layout descriptor.
   * @return A valid layout handle on success, or an invalid handle on failure.
   */
  LDKRHIBindingsLayout ldk_rhi_create_bindings_layout(LDKRHIContext* context, const LDKRHIBindingsLayoutDesc* desc);

  /**
   * @brief Destroys a bindings layout.
   * @param context RHI context.
   * @param bindings_layout Layout handle.
   */
  void ldk_rhi_destroy_bindings_layout(LDKRHIContext* context, LDKRHIBindingsLayout bindings_layout);

  /**
   * @brief Creates a pipeline.
   * @param context RHI context.
   * @param desc Pipeline descriptor.
   * @return A valid pipeline handle on success, or an invalid handle on failure.
   */
  LDKRHIPipeline ldk_rhi_create_pipeline(LDKRHIContext* context, const LDKRHIPipelineDesc* desc);

  /**
   * @brief Destroys a pipeline.
   * @param context RHI context.
   * @param pipeline Pipeline handle.
   */
  void ldk_rhi_destroy_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);

  /**
   * @brief Creates a bindings object.
   * @param context RHI context.
   * @param desc Bindings descriptor.
   * @return A valid bindings handle on success, or an invalid handle on failure.
   */
  LDKRHIBindings ldk_rhi_create_bindings(LDKRHIContext* context, const LDKRHIBindingsDesc* desc);

  /**
   * @brief Destroys a bindings object.
   * @param context RHI context.
   * @param bindings Bindings handle.
   */
  void ldk_rhi_destroy_bindings(LDKRHIContext* context, LDKRHIBindings bindings);

  /**
   * @brief Begins a new frame.
   * @param context RHI context.
   */
  void ldk_rhi_begin_frame(LDKRHIContext* context);

  /**
   * @brief Ends the current frame.
   * @param context RHI context.
   */
  void ldk_rhi_end_frame(LDKRHIContext* context);

  /**
   * @brief Begins a rendering pass.
   * @param context RHI context.
   * @param desc Pass descriptor.
   */
  void ldk_rhi_begin_pass(LDKRHIContext* context, const LDKRHIPassDesc* desc);

  /**
   * @brief Ends the current rendering pass.
   * @param context RHI context.
   */
  void ldk_rhi_end_pass(LDKRHIContext* context);

  /**
   * @brief Binds a pipeline.
   * @param context RHI context.
   * @param pipeline Pipeline handle.
   */
  void ldk_rhi_bind_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);

  /**
   * @brief Binds resource bindings.
   * @param context RHI context.
   * @param bindings Bindings handle.
   */
  void ldk_rhi_bind_bindings(LDKRHIContext* context, LDKRHIBindings bindings);

  /**
   * @brief Binds a vertex buffer.
   * @param context RHI context.
   * @param buffer Buffer handle.
   * @param offset Byte offset.
   */
  void ldk_rhi_bind_vertex_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset);

  /**
   * @brief Binds an index buffer.
   * @param context RHI context.
   * @param buffer Buffer handle.
   * @param offset Byte offset.
   * @param index_type Index type.
   */
  void ldk_rhi_bind_index_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type);

  /**
   * @brief Sets the viewport.
   * @param context RHI context.
   * @param viewport Viewport definition.
   */
  void ldk_rhi_set_viewport(LDKRHIContext* context, const LDKRHIViewport* viewport);

  /**
   * @brief Sets the scissor rectangle.
   * @param context RHI context.
   * @param scissor Rectangle definition.
   */
  void ldk_rhi_set_scissor(LDKRHIContext* context, const LDKRHIRect* scissor);

  /**
   * @brief Issues a non-indexed draw call.
   * @param context RHI context.
   * @param desc Draw descriptor.
   */
  void ldk_rhi_draw(LDKRHIContext* context, const LDKRHIDrawDesc* desc);

  /**
   * @brief Issues an indexed draw call.
   * @param context RHI context.
   * @param desc Draw descriptor.
   */
  void ldk_rhi_draw_indexed(LDKRHIContext* context, const LDKRHIDrawIndexedDesc* desc);

  /**
   * @brief Fills a buffer descriptor with safe default values.
   *
   * @param desc Pointer to the buffer descriptor to initialize.
   */
  void ldk_rhi_buffer_desc_defaults(LDKRHIBufferDesc* desc);

  /**
   * @brief Fills a texture descriptor with safe default values.
   *
   * @param desc Pointer to the texture descriptor to initialize.
   */
  void ldk_rhi_texture_desc_defaults(LDKRHITextureDesc* desc);

  /**
   * @brief Fills a sampler descriptor with default filtering and wrapping state.
   *
   * @param desc Pointer to the sampler descriptor to initialize.
   */
  void ldk_rhi_sampler_desc_defaults(LDKRHISamplerDesc* desc);

  /**
   * @brief Fills a shader descriptor with safe default values.
   *
   * @param desc Pointer to the shader descriptor to initialize.
   */
  void ldk_rhi_shader_desc_defaults(LDKRHIShaderDesc* desc);

  /**
   * @brief Fills a blend state with default disabled blending.
   *
   * @param state Pointer to the blend state to initialize.
   */
  void ldk_rhi_blend_state_defaults(LDKRHIBlendState* state);

  /**
   * @brief Fills a depth state with default depth testing configuration.
   *
   * @param state Pointer to the depth state to initialize.
   */
  void ldk_rhi_depth_state_defaults(LDKRHIDepthState* state);

  /**
   * @brief Fills a raster state with default culling, front-face, and scissor configuration.
   *
   * @param state Pointer to the raster state to initialize.
   */
  void ldk_rhi_raster_state_defaults(LDKRHIRasterState* state);

  /**
   * @brief Fills a pipeline descriptor with safe default values.
   *
   * @param desc Pointer to the pipeline descriptor to initialize.
   */
  void ldk_rhi_pipeline_desc_defaults(LDKRHIPipelineDesc* desc);

  /**
   * @brief Fills a bindings descriptor with no bindings.
   *
   * @param desc Pointer to the bindings descriptor to initialize.
   */
  void ldk_rhi_bindings_desc_defaults(LDKRHIBindingsDesc* desc);

  /**
   * @brief Fills a pass descriptor with no attachments and no dynamic viewport or scissor.
   *
   * @param desc Pointer to the pass descriptor to initialize.
   */
  void ldk_rhi_pass_desc_defaults(LDKRHIPassDesc* desc);

  /**
   * @brief Initializes an RHI context using the supplied backend functions.
   *
   * @param context Pointer to the context to initialize.
   * @param desc Pointer to the context initialization descriptor.
   * @param functions Pointer to the backend function table.
   *
   * @return true if the context was initialized successfully, false otherwise.
   */
  bool ldk_rhi_initialize(LDKRHIContext* context, const LDKRHIContextDesc* desc, const LDKRHIFunctions* functions);

  /**
   * @brief Terminates an RHI context and calls backend shutdown if present.
   *
   * @param context Pointer to the context to terminate.
   */
  void ldk_rhi_terminate(LDKRHIContext* context);

  /**
   * @brief Checks whether a buffer handle is valid.
   *
   * @param buffer Buffer handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_buffer(LDKRHIBuffer buffer);

  /**
   * @brief Checks whether a texture handle is valid.
   *
   * @param texture Texture handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_texture(LDKRHITexture texture);

  /**
   * @brief Checks whether a sampler handle is valid.
   *
   * @param sampler Sampler handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_sampler(LDKRHISampler sampler);

  /**
   * @brief Checks whether a shader handle is valid.
   *
   * @param shader Shader handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader(LDKRHIShader shader);

  /**
   * @brief Checks whether a pipeline handle is valid.
   *
   * @param pipeline Pipeline handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_pipeline(LDKRHIPipeline pipeline);

  /**
   * @brief Checks whether a bindings handle is valid.
   *
   * @param bindings Bindings handle to check.
   *
   * @return true if the handle is not invalid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings(LDKRHIBindings bindings);

  /**
   * @brief Validates a buffer descriptor before buffer creation.
   *
   * @param desc Pointer to the buffer descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_buffer_desc(const LDKRHIBufferDesc* desc);

  /**
   * @brief Validates a texture descriptor before texture creation.
   *
   * @param desc Pointer to the texture descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_texture_desc(const LDKRHITextureDesc* desc);

  /**
   * @brief Validates a sampler descriptor before sampler creation.
   *
   * @param desc Pointer to the sampler descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_sampler_desc(const LDKRHISamplerDesc* desc);

  /**
   * @brief Validates a shader descriptor before shader creation.
   *
   * @param desc Pointer to the shader descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_shader_desc(const LDKRHIShaderDesc* desc);

  /**
   * @brief Validates a pipeline descriptor before pipeline creation.
   *
   * @param desc Pointer to the pipeline descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_pipeline_desc(const LDKRHIPipelineDesc* desc);

  /**
   * @brief Validates a bindings descriptor before bindings creation.
   *
   * @param desc Pointer to the bindings descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_bindings_desc(const LDKRHIBindingsDesc* desc);

  /**
   * @brief Validates a pass descriptor before beginning a pass.
   *
   * @param desc Pointer to the pass descriptor to validate.
   *
   * @return true if the descriptor is valid, false otherwise.
   */
  bool ldk_rhi_is_valid_pass_desc(const LDKRHIPassDesc* desc);

  /**
   * @brief Creates a GPU buffer using the active backend.
   *
   * @param context RHI context.
   * @param desc Pointer to the buffer descriptor.
   *
   * @return A valid buffer handle on success, or LDK_RHI_BUFFER_INVALID on failure.
   */
  LDKRHIBuffer ldk_rhi_create_buffer(LDKRHIContext* context, const LDKRHIBufferDesc* desc);

  /**
   * @brief Destroys a GPU buffer.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param buffer Buffer handle to destroy.
   */
  void ldk_rhi_destroy_buffer(LDKRHIContext* context, LDKRHIBuffer buffer);

  /**
   * @brief Updates a byte range inside an existing buffer.
   *
   * @param context RHI context.
   * @param buffer Buffer handle to update.
   * @param offset Byte offset inside the buffer.
   * @param size Number of bytes to upload.
   * @param data Pointer to the source data.
   *
   * @return true if the buffer update succeeded, false otherwise.
   */
  bool ldk_rhi_update_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data);

  /**
   * @brief Creates a GPU texture using the active backend.
   *
   * @param context RHI context.
   * @param desc Pointer to the texture descriptor.
   *
   * @return A valid texture handle on success, or LDK_RHI_TEXTURE_INVALID on failure.
   */
  LDKRHITexture ldk_rhi_create_texture(LDKRHIContext* context, const LDKRHITextureDesc* desc);

  /**
   * @brief Destroys a GPU texture.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param texture Texture handle to destroy.
   */
  void ldk_rhi_destroy_texture(LDKRHIContext* context, LDKRHITexture texture);

  /**
   * @brief Updates texture data for a mip level and layer.
   *
   * @param context RHI context.
   * @param texture Texture handle to update.
   * @param mip_level Mip level to update.
   * @param layer Array layer, cube face, or texture layer to update.
   * @param data Pointer to the source texel data.
   * @param size Size of the source data in bytes.
   *
   * @return true if the texture update succeeded, false otherwise.
   */
  bool ldk_rhi_update_texture(LDKRHIContext* context, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size);

  /**
   * @brief Creates a sampler object from filtering and wrapping state.
   *
   * @param context RHI context.
   * @param desc Pointer to the sampler descriptor.
   *
   * @return A valid sampler handle on success, or LDK_RHI_SAMPLER_INVALID on failure.
   */
  LDKRHISampler ldk_rhi_create_sampler(LDKRHIContext* context, const LDKRHISamplerDesc* desc);

  /**
   * @brief Destroys a sampler object.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param sampler Sampler handle to destroy.
   */
  void ldk_rhi_destroy_sampler(LDKRHIContext* context, LDKRHISampler sampler);

  /**
   * @brief Creates a shader object from source or binary data.
   *
   * @param context RHI context.
   * @param desc Pointer to the shader descriptor.
   *
   * @return A valid shader handle on success, or LDK_RHI_SHADER_INVALID on failure.
   */
  LDKRHIShader ldk_rhi_create_shader(LDKRHIContext* context, const LDKRHIShaderDesc* desc);

  /**
   * @brief Destroys a shader object.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param shader Shader handle to destroy.
   */
  void ldk_rhi_destroy_shader(LDKRHIContext* context, LDKRHIShader shader);

  /**
   * @brief Creates an immutable graphics pipeline.
   *
   * @param context RHI context.
   * @param desc Pointer to the pipeline descriptor.
   *
   * @return A valid pipeline handle on success, or LDK_RHI_PIPELINE_INVALID on failure.
   */
  LDKRHIPipeline ldk_rhi_create_pipeline(LDKRHIContext* context, const LDKRHIPipelineDesc* desc);

  /**
   * @brief Destroys a graphics pipeline.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param pipeline Pipeline handle to destroy.
   */
  void ldk_rhi_destroy_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);

  /**
   * @brief Creates a bindings object from buffer, texture, and sampler bindings.
   *
   * @param context RHI context.
   * @param desc Pointer to the bindings descriptor.
   *
   * @return A valid bindings handle on success, or LDK_RHI_BINDINGS_INVALID on failure.
   */
  LDKRHIBindings ldk_rhi_create_bindings(LDKRHIContext* context, const LDKRHIBindingsDesc* desc);

  /**
   * @brief Destroys a bindings object.
   *
   * Passing an invalid handle has no effect.
   *
   * @param context RHI context.
   * @param bindings Bindings handle to destroy.
   */
  void ldk_rhi_destroy_bindings(LDKRHIContext* context, LDKRHIBindings bindings);

  /**
   * @brief Begins a rendering frame.
   *
   * @param context RHI context.
   */
  void ldk_rhi_begin_frame(LDKRHIContext* context);

  /**
   * @brief Ends the current rendering frame.
   *
   * @param context RHI context.
   */
  void ldk_rhi_end_frame(LDKRHIContext* context);

  /**
   * @brief Begins a render pass using the supplied pass descriptor.
   *
   * @param context RHI context.
   * @param desc Pointer to the pass descriptor.
   */
  void ldk_rhi_begin_pass(LDKRHIContext* context, const LDKRHIPassDesc* desc);

  /**
   * @brief Ends the current render pass.
   *
   * @param context RHI context.
   */
  void ldk_rhi_end_pass(LDKRHIContext* context);

  /**
   * @brief Binds a graphics pipeline for subsequent draw calls.
   *
   * @param context RHI context.
   * @param pipeline Pipeline handle to bind.
   */
  void ldk_rhi_bind_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);

  /**
   * @brief Binds a resource bindings object for subsequent draw calls.
   *
   * @param context RHI context.
   * @param bindings Bindings handle to bind.
   */
  void ldk_rhi_bind_bindings(LDKRHIContext* context, LDKRHIBindings bindings);

  /**
   * @brief Binds a vertex buffer and byte offset for subsequent draw calls.
   *
   * @param context RHI context.
   * @param buffer Vertex buffer handle.
   * @param offset Byte offset inside the vertex buffer.
   */
  void ldk_rhi_bind_vertex_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset);

  /**
   * @brief Binds an index buffer, byte offset, and index type for indexed draw calls.
   *
   * @param context RHI context.
   * @param buffer Index buffer handle.
   * @param offset Byte offset inside the index buffer.
   * @param index_type Type of indices stored in the buffer.
   */
  void ldk_rhi_bind_index_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type);

  /**
   * @brief Sets the active viewport dynamically.
   *
   * @param context RHI context.
   * @param viewport Pointer to the viewport to apply.
   */
  void ldk_rhi_set_viewport(LDKRHIContext* context, const LDKRHIViewport* viewport);

  /**
   * @brief Sets the active scissor rectangle dynamically.
   *
   * @param context RHI context.
   * @param scissor Pointer to the scissor rectangle to apply.
   */
  void ldk_rhi_set_scissor(LDKRHIContext* context, const LDKRHIRect* scissor);

  /**
   * @brief Issues a non-indexed draw call.
   *
   * @param context RHI context.
   * @param desc Pointer to the draw descriptor.
   */
  void ldk_rhi_draw(LDKRHIContext* context, const LDKRHIDrawDesc* desc);

  /**
   * @brief Issues an indexed draw call.
   *
   * @param context RHI context.
   * @param desc Pointer to the indexed draw descriptor.
   */
  void ldk_rhi_draw_indexed(LDKRHIContext* context, const LDKRHIDrawIndexedDesc* desc);

#ifdef __cplusplus
}
#endif

#endif // LDK_RHI_H
