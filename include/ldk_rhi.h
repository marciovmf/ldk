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
    LDK_RHI_BUFFER_INVALID = 0,
    LDK_RHI_TEXTURE_INVALID = 0,
    LDK_RHI_SAMPLER_INVALID = 0,
    LDK_RHI_SHADER_MODULE_INVALID = 0,
    LDK_RHI_BINDINGS_LAYOUT_INVALID = 0,
    LDK_RHI_PIPELINE_INVALID = 0,
    LDK_RHI_BINDINGS_INVALID = 0
  };

  /* Compatibility aliases. */
#define LDK_RHI_INVALID_BUFFER LDK_RHI_BUFFER_INVALID
#define LDK_RHI_INVALID_TEXTURE LDK_RHI_TEXTURE_INVALID
#define LDK_RHI_INVALID_SAMPLER LDK_RHI_SAMPLER_INVALID
#define LDK_RHI_INVALID_SHADER_MODULE LDK_RHI_SHADER_MODULE_INVALID
#define LDK_RHI_INVALID_SHADER LDK_RHI_SHADER_MODULE_INVALID
#define LDK_RHI_INVALID_BINDINGS_LAYOUT LDK_RHI_BINDINGS_LAYOUT_INVALID
#define LDK_RHI_INVALID_PIPELINE LDK_RHI_PIPELINE_INVALID
#define LDK_RHI_INVALID_BINDINGS LDK_RHI_BINDINGS_INVALID
#define LDK_RHI_SHADER_INVALID LDK_RHI_SHADER_MODULE_INVALID

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

  void ldk_rhi_buffer_desc_defaults(LDKRHIBufferDesc* desc);
  void ldk_rhi_texture_desc_defaults(LDKRHITextureDesc* desc);
  void ldk_rhi_sampler_desc_defaults(LDKRHISamplerDesc* desc);
  void ldk_rhi_shader_module_desc_defaults(LDKRHIShaderModuleDesc* desc);
  void ldk_rhi_shader_desc_defaults(LDKRHIShaderDesc* desc);
  void ldk_rhi_blend_state_defaults(LDKRHIBlendState* state);
  void ldk_rhi_depth_state_defaults(LDKRHIDepthState* state);
  void ldk_rhi_raster_state_defaults(LDKRHIRasterState* state);
  void ldk_rhi_pipeline_desc_defaults(LDKRHIPipelineDesc* desc);
  void ldk_rhi_bindings_layout_desc_defaults(LDKRHIBindingsLayoutDesc* desc);
  void ldk_rhi_bindings_desc_defaults(LDKRHIBindingsDesc* desc);
  void ldk_rhi_pass_desc_defaults(LDKRHIPassDesc* desc);

  bool ldk_rhi_initialize(LDKRHIContext* context, const LDKRHIContextDesc* desc, const LDKRHIFunctions* functions);
  void ldk_rhi_terminate(LDKRHIContext* context);

  bool ldk_rhi_is_valid_buffer(LDKRHIBuffer buffer);
  bool ldk_rhi_is_valid_texture(LDKRHITexture texture);
  bool ldk_rhi_is_valid_sampler(LDKRHISampler sampler);
  bool ldk_rhi_is_valid_shader_module(LDKRHIShaderModule shader_module);
  bool ldk_rhi_is_valid_shader(LDKRHIShader shader);
  bool ldk_rhi_is_valid_bindings_layout(LDKRHIBindingsLayout bindings_layout);
  bool ldk_rhi_is_valid_pipeline(LDKRHIPipeline pipeline);
  bool ldk_rhi_is_valid_bindings(LDKRHIBindings bindings);

  bool ldk_rhi_is_valid_buffer_desc(const LDKRHIBufferDesc* desc);
  bool ldk_rhi_is_valid_texture_desc(const LDKRHITextureDesc* desc);
  bool ldk_rhi_is_valid_sampler_desc(const LDKRHISamplerDesc* desc);
  bool ldk_rhi_is_valid_shader_module_desc(const LDKRHIShaderModuleDesc* desc);
  bool ldk_rhi_is_valid_shader_desc(const LDKRHIShaderDesc* desc);
  bool ldk_rhi_is_valid_bindings_layout_desc(const LDKRHIBindingsLayoutDesc* desc);
  bool ldk_rhi_is_valid_pipeline_desc(const LDKRHIPipelineDesc* desc);
  bool ldk_rhi_is_valid_bindings_desc(const LDKRHIBindingsDesc* desc);
  bool ldk_rhi_is_valid_pass_desc(const LDKRHIPassDesc* desc);

  LDKRHIBuffer ldk_rhi_create_buffer(LDKRHIContext* context, const LDKRHIBufferDesc* desc);
  void ldk_rhi_destroy_buffer(LDKRHIContext* context, LDKRHIBuffer buffer);
  bool ldk_rhi_update_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, uint32_t size, const void* data);

  LDKRHITexture ldk_rhi_create_texture(LDKRHIContext* context, const LDKRHITextureDesc* desc);
  void ldk_rhi_destroy_texture(LDKRHIContext* context, LDKRHITexture texture);
  bool ldk_rhi_update_texture(LDKRHIContext* context, LDKRHITexture texture, uint32_t mip_level, uint32_t layer, const void* data, uint32_t size);

  LDKRHISampler ldk_rhi_create_sampler(LDKRHIContext* context, const LDKRHISamplerDesc* desc);
  void ldk_rhi_destroy_sampler(LDKRHIContext* context, LDKRHISampler sampler);


  LDKRHIShaderModule ldk_rhi_create_shader_module(LDKRHIContext* context, const LDKRHIShaderModuleDesc* desc);
  void ldk_rhi_destroy_shader_module(LDKRHIContext* context, LDKRHIShaderModule shader_module);

  LDKRHIShader ldk_rhi_create_shader(LDKRHIContext* context, const LDKRHIShaderDesc* desc);
  void ldk_rhi_destroy_shader(LDKRHIContext* context, LDKRHIShader shader);

  LDKRHIBindingsLayout ldk_rhi_create_bindings_layout(LDKRHIContext* context, const LDKRHIBindingsLayoutDesc* desc);
  void ldk_rhi_destroy_bindings_layout(LDKRHIContext* context, LDKRHIBindingsLayout bindings_layout);

  LDKRHIPipeline ldk_rhi_create_pipeline(LDKRHIContext* context, const LDKRHIPipelineDesc* desc);
  void ldk_rhi_destroy_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);

  LDKRHIBindings ldk_rhi_create_bindings(LDKRHIContext* context, const LDKRHIBindingsDesc* desc);
  void ldk_rhi_destroy_bindings(LDKRHIContext* context, LDKRHIBindings bindings);

  void ldk_rhi_begin_frame(LDKRHIContext* context);
  void ldk_rhi_end_frame(LDKRHIContext* context);

  void ldk_rhi_begin_pass(LDKRHIContext* context, const LDKRHIPassDesc* desc);
  void ldk_rhi_end_pass(LDKRHIContext* context);

  void ldk_rhi_bind_pipeline(LDKRHIContext* context, LDKRHIPipeline pipeline);
  void ldk_rhi_bind_bindings(LDKRHIContext* context, LDKRHIBindings bindings);
  void ldk_rhi_bind_vertex_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset);
  void ldk_rhi_bind_index_buffer(LDKRHIContext* context, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type);

  void ldk_rhi_set_viewport(LDKRHIContext* context, const LDKRHIViewport* viewport);
  void ldk_rhi_set_scissor(LDKRHIContext* context, const LDKRHIRect* scissor);

  void ldk_rhi_draw(LDKRHIContext* context, const LDKRHIDrawDesc* desc);
  void ldk_rhi_draw_indexed(LDKRHIContext* context, const LDKRHIDrawIndexedDesc* desc);

#ifdef __cplusplus
}
#endif

#endif /* LDK_RHI_H */
