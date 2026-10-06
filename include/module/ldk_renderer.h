/**
 * @file ldk_renderer.h
 * @brief LDK engine renderer.
 *
 * API agnostic renderer built on top of ldk_rhi.
 */

#ifndef LDK_RENDERER_H
#define LDK_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_material.h>
#include <ldk_mesh.h>
#include <ldk_resource.h>
#include <ldk_skybox.h>
#include <ldk_ttf.h>
#include <ldk_image.h>
#include <module/ldk_rhi.h>
#include <module/ldk_ui.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef LDK_RENDERER_ALLOC
#define LDK_RENDERER_ALLOC(size) malloc(size)
#endif

#ifndef LDK_RENDERER_FREE
#define LDK_RENDERER_FREE(ptr) free(ptr)
#endif

#ifndef LDK_RENDERER_REALLOC
#define LDK_RENDERER_REALLOC(ptr, size) realloc(ptr, size)
#endif

  typedef enum LDKShader
  {
    LDK_SHADER_INVALID = 0,
    LDK_SHADER_UI_PASS,
    LDK_SHADER_TEXT_PASS,
    LDK_SHADER_MESH_PASS,
    LDK_SHADER_PRESENT_PASS,
    LDK_SHADER_MESH_PASS_INSTANCED,
    LDK_SHADER_GRID_PASS,
    LDK_SHADER_MESH_PASS_UNLIT,
    LDK_SHADER_MESH_PASS_TEXTURED,
    LDK_SHADER_MESH_PASS_TEXTURED_UNLIT,
    LDK_SHADER_SHADOW_PASS,
    LDK_SHADER_MESH_PASS_TEXTURED_CUTOUT,
    LDK_SHADER_MESH_PASS_TEXTURED_UNLIT_CUTOUT,
    LDK_SHADER_SHADOW_PASS_CUTOUT,
    LDK_SHADER_SHADOW_PASS_INSTANCED,
    LDK_SHADER_SHADOW_PASS_CUTOUT_INSTANCED,
    LDK_SHADER_VEGETATION_PASS,
    LDK_SHADER_VEGETATION_PASS_INSTANCED,
    LDK_SHADER_SHADOW_PASS_VEGETATION,
    LDK_SHADER_SHADOW_PASS_VEGETATION_INSTANCED,
    LDK_SHADER_POST_PROCESS_PASS,
    LDK_SHADER_BLUR_PASS,
    LDK_SHADER_SKYBOX_PASS,
    LDK_SHADER_MESH_PASS_SOLID_UNLIT,
    LDK_SHADER_TERRAIN_PASS,
    LDK_SHADER_WATER_PASS
  } LDKShader;

  typedef struct LDKRendererMeshDesc
  {
    const LDKMeshVertex* vertices;
    u32 vertex_count;
    const u32* indices;
    u32 index_count;
    /* True when vertex tangent fields contain authored/initialized data. */
    bool has_tangents;
  } LDKRendererMeshDesc;

  typedef struct LDKRendererMeshResource
  {
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    u32 vertex_count;
    u32 index_count;
    u32 vertex_capacity;
    u32 index_capacity;
    bool alive;
  } LDKRendererMeshResource;

  typedef struct LDKRendererInstanceSetResource
  {
    LDKRHIBuffer transform_buffer;
    LDKRHIBuffer color_buffer;
    u32 instance_count;
    u32 instance_capacity;
    bool alive;
  } LDKRendererInstanceSetResource;

  typedef u64 LDKRendererViewId;

#define LDK_RENDERER_VIEW_INVALID ((LDKRendererViewId)0)
#define LDK_RENDERER_VIEW_ALL ((LDKRendererViewId)UINT64_MAX)

#define LDK_RENDERER_WATER_WAVE_COUNT 3u

  typedef enum LDKRendererWaterTexture
  {
    LDK_RENDERER_WATER_TEXTURE_NORMAL = 0,
    LDK_RENDERER_WATER_TEXTURE_FOAM,
    LDK_RENDERER_WATER_TEXTURE_NOISE,
    LDK_RENDERER_WATER_TEXTURE_CAUSTICS,
    LDK_RENDERER_WATER_TEXTURE_COUNT
  } LDKRendererWaterTexture;

  typedef struct LDKRendererWaterWave
  {
    float height; // Amplitude in world units.
    float length; // Wavelength in world units.
    float speed; // Travel speed in world units per second.
    float direction_degrees; // Zero travels along +X, 90 along +Z.
  } LDKRendererWaterWave;

  typedef struct LDKRendererWaterDesc
  {
    float water_level;
    rgba32 shallow_color;
    rgba32 deep_color;
    float depth_color_distance;
    float edge_fade_distance;
    LDKRendererWaterWave waves[LDK_RENDERER_WATER_WAVE_COUNT];
    float detail_scale;
    float detail_strength;
    float detail_speed;
    float specular;
    float shininess;
    rgba32 foam_color;
    float foam_width; // Horizontal surface distance in world units.
    float foam_strength;
    float foam_scale;
    float shore_range;
    float shore_wave_length;
    float shore_wave_speed;
    float shore_foam_strength;
    /* Optional acquired image resources. Scales are world units per repeat;
     * speeds are world units per second. Masks use their red channel. */
    LDKResourceTexture textures[LDK_RENDERER_WATER_TEXTURE_COUNT];
    float noise_scale;
    float noise_speed;
    float distortion_strength;
    float foam_speed;
    float foam_cutoff;
    float surface_foam_strength;
    float caustics_scale;
    float caustics_strength;
    float caustics_speed;
    float caustics_depth;
    float time_seconds;
  } LDKRendererWaterDesc;

  typedef struct LDKRendererWaterSubmit
  {
    LDKResourceMesh mesh;
    LDKRendererViewId view_id;
    LDKRendererWaterDesc desc;
  } LDKRendererWaterSubmit;

  typedef struct LDKRendererWaterPass
  {
    LDKRHIContext *rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline ldr_pipeline;
    LDKRHIPipeline hdr_pipeline;
    LDKRHIBuffer params_buffer;
    LDKRHISampler depth_sampler;
    LDKRHISampler texture_sampler;
    LDKRHITexture fallback_texture;
    bool is_initialized;
  } LDKRendererWaterPass;

#define LDK_RENDERER_MAX_LIGHTS_PER_VIEW 16

  typedef enum LDKRendererLightType
  {
    LDK_RENDERER_LIGHT_POINT = 0,
    LDK_RENDERER_LIGHT_SPOT,
    LDK_RENDERER_LIGHT_DIRECTIONAL
  } LDKRendererLightType;

  typedef struct LDKRendererLightSubmit
  {
    LDKRendererLightType type;
    LDKRendererViewId view_id;
    Vec3 position;
    Vec3 direction;
    u32 color; // 0xRRGGBBAA; alpha is ignored.
    float intensity;
    float range;
    float inner_angle; // Half-cone angle, radians.
    float outer_angle; // Half-cone angle, radians.
    bool casts_shadows; // Directional only; first eligible light per view.
  } LDKRendererLightSubmit;

  typedef struct LDKRendererAmbientLight
  {
    u32 color; // 0xRRGGBBAA; alpha is ignored.
    float intensity;
  } LDKRendererAmbientLight;

  /* BILLBOARD affects the color pass only. Billboard transforms are expected
   * to encode translation, XY roll and scale; camera-facing orientation is
   * applied from the current render view in the vertex shader. */
  typedef enum LDKRendererMeshSubmitFlag
  {
    LDK_RENDERER_MESH_SUBMIT_FLAG_NONE = 0,
    LDK_RENDERER_MESH_SUBMIT_FLAG_OVERLAY = 1 << 0,
    LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS = 1 << 1,
    LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD = 1 << 2
  } LDKRendererMeshSubmitFlag;

  typedef struct LDKRendererMeshSubmit
  {
    LDKResourceMesh mesh;
    LDKResourceMaterial material;
    u32 first_index;
    u32 index_count;
    Mat4 world;
    LDKRendererViewId view_id;
    u32 flags;
    // Zero count denotes an ordinary submit; offsets index renderer-owned data.
    u32 instance_offset;
    u32 instance_count;
    LDKResourceInstanceSet instance_set;
  } LDKRendererMeshSubmit;

  /* Internal frame data for the shared triangular line prism. */
  typedef struct LDKRendererLineSubmit
  {
    Mat4 world;
    LDKRendererViewId view_id;
    u32 color;
    bool depth_test;
  } LDKRendererLineSubmit;

  typedef struct LDKRendererWireframeSubmit
  {
    LDKResourceMesh mesh;
    Mat4 world;
    LDKRendererViewId view_id;
    u32 color;
    float line_width;
    u32 flags;
  } LDKRendererWireframeSubmit;

  typedef enum LDKRendererTextSubmitFlag
  {
    LDK_RENDERER_TEXT_SUBMIT_FLAG_NONE = 0,
    LDK_RENDERER_TEXT_SUBMIT_FLAG_BILLBOARD = 1 << 0,
    LDK_RENDERER_TEXT_SUBMIT_FLAG_NO_DEPTH_TEST = 1 << 1
  } LDKRendererTextSubmitFlag;

  typedef struct LDKRendererTextVertex
  {
    float x;
    float y;
    float u;
    float v;
    u32 color;
  } LDKRendererTextVertex;

  typedef struct LDKRendererTextSubmit
  {
    Mat4 world;
    LDKRendererViewId view_id;
    LDKRHITexture texture;
    u32 index_offset;
    u32 index_count;
    u32 flags;
  } LDKRendererTextSubmit;

  typedef struct LDKRendererConfig
  {
    LDKRHIContext* rhi;
    u32 initial_ui_vertex_capacity;
    u32 initial_ui_index_capacity;
    u32 game_width;
    u32 game_height;
    bool present_game;
    // Zero selects the default (2048 texels, 60 world units).
    u32 shadow_map_resolution;
    float shadow_distance;
  } LDKRendererConfig;

  typedef struct LDKRendererFrameDesc
  {
    i32 framebuffer_width;
    i32 framebuffer_height;
    rgba32 clear_color;
    bool clear_color_enabled;
  } LDKRendererFrameDesc;

  typedef struct LDKRendererFrameDomainStats
  {
    u32 rendered_view_count;
    u32 opaque_mesh_render_count;
    u32 overlay_mesh_render_count;

    u32 batch_count;
    u32 instanced_batch_count;
    u32 instanced_instance_count;
    u32 max_batch_size;

    u32 draw_call_count;
    u32 opaque_mesh_draw_call_count;
    u32 overlay_mesh_draw_call_count;
    u32 shadow_draw_call_count;
    u32 line_draw_call_count;
    u32 grid_draw_call_count;
    u32 skybox_draw_call_count;
    u32 ui_draw_call_count;
    u32 present_draw_call_count;
  } LDKRendererFrameDomainStats;

  typedef struct LDKRendererFrameStats
  {
    double cpu_time_ms;
    LDKRHIFrameStats rhi;

    // Submission count is global because a VIEW_ALL submission may be rendered
    // into both game and non-game views. The remaining counters describe actual
    // rendered work and are also available split by destination below.
    u32 rendered_view_count;
    u32 mesh_submit_count;
    u32 opaque_mesh_render_count;
    u32 overlay_mesh_render_count;

    u32 batch_count;
    u32 instanced_batch_count;
    u32 instanced_instance_count;
    u32 max_batch_size;

    u32 draw_call_count;
    u32 opaque_mesh_draw_call_count;
    u32 overlay_mesh_draw_call_count;
    u32 shadow_draw_call_count;
    u32 line_draw_call_count;
    u32 grid_draw_call_count;
    u32 skybox_draw_call_count;
    u32 ui_draw_call_count;
    u32 present_draw_call_count;

    LDKRendererFrameDomainStats game;
    LDKRendererFrameDomainStats non_game;
  } LDKRendererFrameStats;

  typedef struct LDKRendererBindingsCacheEntry
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    LDKRHIBindings bindings;
  } LDKRendererBindingsCacheEntry;

  typedef struct LDKRendererMeshBindingsCacheEntry
  {
    LDKRHITexture textures[LDK_MATERIAL_TEXTURE_SLOT_COUNT];
    LDKRHISampler samplers[LDK_MATERIAL_TEXTURE_SLOT_COUNT];
    LDKRHIBindings bindings;
  } LDKRendererMeshBindingsCacheEntry;

  typedef struct LDKRendererTextPass
  {
    LDKRHIContext *rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline ldr_pipeline;
    LDKRHIPipeline hdr_pipeline;
    LDKRHIPipeline ldr_no_depth_pipeline;
    LDKRHIPipeline hdr_no_depth_pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHISampler sampler;
    LDKRendererBindingsCacheEntry *bindings_cache;
    u32 bindings_cache_count;
    u32 bindings_cache_capacity;
    u32 vertex_capacity;
    u32 index_capacity;
    bool is_initialized;
  } LDKRendererTextPass;

  typedef struct LDKRendererUIPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHITexture white_texture;
    LDKRHISampler sampler;
    LDKRendererBindingsCacheEntry* bindings_cache;
    u32 bindings_cache_count;
    u32 bindings_cache_capacity;
    u32 vertex_capacity;
    u32 index_capacity;
    bool is_initialized;
  } LDKRendererUIPass;

  typedef struct LDKRendererShadowPass
  {
    u32 resolution;
    float distance;
    LDKRHIContext *rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIShaderModule cutout_vertex_shader_module;
    LDKRHIShaderModule cutout_fragment_shader_module;
    LDKRHIShaderModule instanced_vertex_shader_module;
    LDKRHIShaderModule cutout_instanced_vertex_shader_module;
    LDKRHIShaderModule vegetation_vertex_shader_module;
    LDKRHIShaderModule vegetation_instanced_vertex_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIPipeline cutout_pipeline;
    LDKRHIPipeline instanced_pipeline;
    LDKRHIPipeline cutout_instanced_pipeline;
    LDKRHIPipeline vegetation_pipeline;
    LDKRHIPipeline vegetation_instanced_pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBuffer material_buffer;
    LDKRHIBuffer instance_buffer;
    u32 instance_capacity;
    LDKRHIBindings bindings;
    LDKRHIBindings vegetation_bindings;
    LDKRendererBindingsCacheEntry *cutout_bindings_cache;
    u32 cutout_bindings_cache_count;
    u32 cutout_bindings_cache_capacity;
    LDKRHITexture depth_texture;
    LDKRHISampler sampler;
  } LDKRendererShadowPass;

  typedef struct LDKRendererMeshPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule instanced_vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIShaderModule overlay_fragment_shader_module;
    LDKRHIShaderModule solid_unlit_fragment_shader_module;
    LDKRHIShaderModule textured_fragment_shader_module;
    LDKRHIShaderModule textured_unlit_fragment_shader_module;
    LDKRHIShaderModule textured_cutout_fragment_shader_module;
    LDKRHIShaderModule textured_unlit_cutout_fragment_shader_module;
    LDKRHIShaderModule vegetation_vertex_shader_module;
    LDKRHIShaderModule vegetation_instanced_vertex_shader_module;
    LDKRHIShaderModule vegetation_fragment_shader_module;
    LDKRHIShaderModule terrain_vertex_shader_module;
    LDKRHIShaderModule terrain_fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline vertex_color_pipeline;
    LDKRHIPipeline vertex_color_unlit_pipeline;
    LDKRHIPipeline vertex_color_blend_pipeline;
    LDKRHIPipeline vertex_color_unlit_blend_pipeline;
    LDKRHIPipeline overlay_pipeline;
    LDKRHIPipeline wireframe_pipeline;
    LDKRHIPipeline textured_pipeline;
    LDKRHIPipeline textured_unlit_pipeline;
    LDKRHIPipeline textured_overlay_pipeline;
    LDKRHIPipeline textured_cutout_pipeline;
    LDKRHIPipeline textured_unlit_cutout_pipeline;
    LDKRHIPipeline textured_unlit_cutout_overlay_pipeline;
    LDKRHIPipeline textured_blend_pipeline;
    LDKRHIPipeline textured_unlit_blend_pipeline;
    LDKRHIPipeline vertex_color_instanced_pipeline;
    LDKRHIPipeline vertex_color_unlit_instanced_pipeline;
    LDKRHIPipeline vertex_color_blend_instanced_pipeline;
    LDKRHIPipeline vertex_color_unlit_blend_instanced_pipeline;
    LDKRHIPipeline textured_instanced_pipeline;
    LDKRHIPipeline textured_unlit_instanced_pipeline;
    LDKRHIPipeline textured_cutout_instanced_pipeline;
    LDKRHIPipeline textured_unlit_cutout_instanced_pipeline;
    LDKRHIPipeline textured_blend_instanced_pipeline;
    LDKRHIPipeline textured_unlit_blend_instanced_pipeline;
    LDKRHIPipeline vegetation_pipeline;
    LDKRHIPipeline vegetation_instanced_pipeline;
    LDKRHIPipeline terrain_pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBuffer material_buffer;
    LDKRHIBuffer lighting_buffer;
    LDKRHIBuffer instance_buffer;
    LDKRHIBuffer instance_color_buffer;
    u32 instance_capacity;
    // Borrowed from the renderer-owned shadow pass.
    LDKRHITexture shadow_texture;
    LDKRHISampler shadow_sampler;
    // Renderer-owned neutral resources for optional lit material maps.
    LDKRHITexture flat_normal_texture;
    LDKRHITexture white_specular_texture;
    LDKRHISampler fallback_sampler;
    LDKRHIBindings bindings;
    LDKRHIBindings vegetation_bindings;
    LDKRendererMeshBindingsCacheEntry *material_bindings_cache;
    u32 material_bindings_cache_count;
    u32 material_bindings_cache_capacity;
    bool is_initialized;
  } LDKRendererMeshPass;

  typedef struct LDKRendererGridPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHIBindings bindings;
    bool is_initialized;
  } LDKRendererGridPass;

  typedef struct LDKRendererSkyboxPass
  {
    LDKRHIContext *rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline ldr_pipeline;
    LDKRHIPipeline hdr_pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer params_buffer;
    bool is_initialized;
  } LDKRendererSkyboxPass;

  typedef struct LDKRendererPostProcessing
  {
    bool enabled;
    bool tonemapping_enabled;
    float exposure;
    bool blur_enabled;
    float blur_strength;
    float blur_focus_x;
    float blur_focus_y;
    float blur_focus_size;
    float blur_focus_feather;
    bool blur_inverted;
    bool color_enabled;
    float brightness;
    float contrast;
    float saturation;
    bool vignette_enabled;
    float vignette_intensity;
    float vignette_radius;
    float vignette_softness;
    bool screen_distortion_enabled;
    float screen_distortion_strength;
    bool chromatic_aberration_enabled;
    float chromatic_aberration_strength;
    bool heat_enabled;
    float heat_strength;
    float heat_amplitude;
    float heat_scale;
    float heat_speed;
    bool inverse_enabled;
    bool black_and_white_enabled;
    bool retro_enabled;
    float retro_pixel_size;
    float retro_color_levels;
    float retro_dither_strength;
    float retro_pattern_scale;
    bool drunk_enabled;
    float drunk_strength;
    float drunk_speed;
    float drunk_ghosting;
    float drunk_chromatic_aberration;
    float drunk_movement;
  } LDKRendererPostProcessing;

  typedef struct LDKRendererPostProcessPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHISampler sampler;
    bool is_initialized;
  } LDKRendererPostProcessPass;

  typedef struct LDKRendererBlurPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline ldr_pipeline;
    LDKRHIPipeline hdr_pipeline;
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer params_buffer;
    LDKRHISampler sampler;
    bool is_initialized;
  } LDKRendererBlurPass;

  typedef struct LDKRendererFontPageCacheEntry
  {
    LDKFontInstance* font;
    u32 page_index;
    u32 width;
    u32 height;
    LDKRHITexture texture;
  } LDKRendererFontPageCacheEntry;

  typedef struct LDKRendererFontPage
  {
    LDKFontInstance* font;
    u32 page_index;
    u32 width;
    u32 height;
    LDKRHITexture texture;
  } LDKRendererFontPage;

  typedef struct LDKRendererTarget
  {
    LDKRHITexture color_texture;
    LDKRHITexture depth_texture;
    i32 width;
    i32 height;
    LDKRHIFormat color_format;
    LDKRHIFormat depth_format;
  } LDKRendererTarget;

  typedef struct LDKRendererView
  {
    LDKRendererViewId id;
    Mat4 view;
    Mat4 projection;
    Vec4 frustum_planes[6];
    u32 width;
    u32 height;
    u32 requested_width;
    u32 requested_height;
    bool extent_override;
    LDKRendererTarget target;
    LDKRendererTarget overlay_target;
    LDKRHITexture post_process_texture;
    LDKRHIBindings post_process_bindings;
    LDKRHITexture post_process_input_texture;
    LDKRHITexture post_process_blurred_texture;
    LDKRHITexture post_process_blur_textures[2];
    LDKRHIBindings post_process_blur_bindings[2];
    LDKRHITexture post_process_blur_input_textures[2];
    LDKRHIFormat post_process_blur_format;
    LDKRendererPostProcessing post_processing;
    rgba32 clear_color;
    bool clear_color_set;
    bool separate_overlay;
    Vec3 grid_center;
    float grid_extent;
    float grid_spacing;
    bool grid_submitted;
    LDKResourceSkybox skybox;
    bool submitted;
    bool frustum_valid;
  } LDKRendererView;

  typedef enum LDKRendererTextureFlag
  {
    LDK_RENDERER_TEXTURE_FLAG_NONE       = 0,
    LDK_RENDERER_TEXTURE_FLAG_SRGB       = 1 << 0,
    LDK_RENDERER_TEXTURE_FLAG_RENDERABLE = 1 << 1
  } LDKRendererTextureFlag;

  typedef struct LDKRendererTextureOptions
  {
    u32 flags;
    bool generate_mipmaps;
    LDKRHIFilter min_filter;
    LDKRHIFilter mag_filter;
    LDKRHIFilter mip_filter;
    LDKRHIWrap wrap_u;
    LDKRHIWrap wrap_v;
    LDKRHIWrap wrap_w;
  } LDKRendererTextureOptions;

  typedef struct LDKRendererTextureDesc
  {
    u32 width;
    u32 height;
    u32 channel_count;
    /* Optional explicit storage format. INVALID derives an 8-bit format from
     * channel_count and SRGB options for backwards compatibility. */
    LDKRHIFormat format;
    u32 flags;
    void const* pixels;
    u64 byte_count;
    LDKRendererTextureOptions const* options;
  } LDKRendererTextureDesc;

  typedef struct LDKRendererSkyboxResource
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    LDKRHIBindings bindings;
    u32 size;
    bool alive;
  } LDKRendererSkyboxResource;

  typedef struct LDKRendererTextureResource
  {
    LDKRHITexture texture;
    LDKRHISampler sampler;
    u32 width;
    u32 height;
    u32 channel_count;
    LDKRHIFormat format;
    u32 flags;
    bool alive;
    struct LDKAssetManager* asset_manager;
    LDKAssetImage image_asset;
    u32 image_references;
  } LDKRendererTextureResource;

#define LDK_RENDERER_TERRAIN_SURFACE_COUNT 8u

  typedef struct LDKRendererTerrainSurfaceDesc
  {
    float min_height;
    float blend_width;
    rgba32 color;
    Vec4 atlas_rect;
    Vec2 uv_scale;
    float texture_weight;
  } LDKRendererTerrainSurfaceDesc;

  typedef struct LDKRendererMaterialDesc
  {
    LDKMaterialType type;
    LDKResourceTexture texture;
    /* Optional borrowed resources for lit materials. */
    LDKResourceTexture normal_map;
    LDKResourceTexture specular_map;
    /* Shader-defined Texture2D slots 3..7. */
    LDKResourceTexture
        additional_textures[LDK_MATERIAL_ADDITIONAL_TEXTURE_COUNT];
    rgba32 color;
    LDKMaterialAlphaMode alpha_mode;
    float alpha_cutoff;
    float specular;
    float shininess;
    float emission;
    /* Optional vertex deformation used by procedural vegetation. */
    float vegetation_curvature;
    float vegetation_wind_strength;
    float vegetation_wind_speed;
    float vegetation_wind_direction;
    float vegetation_interaction_recovery_time;
    bool vegetation;
    /* Optional height-driven terrain appearance. Heights are world-space. */
    bool terrain;
    u32 terrain_surface_count;
    LDKRendererTerrainSurfaceDesc
        terrain_surfaces[LDK_RENDERER_TERRAIN_SURFACE_COUNT];
  } LDKRendererMaterialDesc;

  typedef enum LDKRendererMaterialSelection
  {
    LDK_RENDERER_MATERIAL_SELECTION_INVALID = 0,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_UNLIT_BLEND,
    LDK_RENDERER_MATERIAL_SELECTION_VERTEX_COLOR_BLEND,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_CUTOUT,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_CUTOUT,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_UNLIT_BLEND,
    LDK_RENDERER_MATERIAL_SELECTION_TEXTURED_BLEND,
    LDK_RENDERER_MATERIAL_SELECTION_VEGETATION,
    LDK_RENDERER_MATERIAL_SELECTION_TERRAIN
  } LDKRendererMaterialSelection;

  typedef u64 LDKRendererRenderKey;

  typedef struct LDKRendererMaterialSamplerResource
  {
    LDKRHISamplerDesc desc;
    LDKRHISampler sampler;
  } LDKRendererMaterialSamplerResource;

  typedef struct LDKRendererMaterialResource
  {
    LDKRendererMaterialDesc desc;
    LDKRendererMaterialSelection selection;
    LDKRendererRenderKey render_key;
    LDKRHISampler texture_sampler;
    bool owns_additional_textures;
    Vec2 uv_scale;
    Vec2 uv_offset;
    bool alive;
  } LDKRendererMaterialResource;

  typedef struct LDKRenderer
  {
    LDKRHIContext* rhi;
    LDKRendererUIPass ui_pass;
    LDKRendererTextPass text_pass;
    LDKRendererMeshPass mesh_pass;
    LDKRendererWaterPass water_pass;
    LDKRendererShadowPass shadow_pass;
    LDKRendererGridPass grid_pass;
    LDKRendererSkyboxPass skybox_pass;
    LDKRendererBlurPass blur_pass;
    LDKRendererPostProcessPass post_process_pass;
    LDKUIRenderData const* submitted_ui;
    u32 game_width;
    u32 game_height;
    bool present_game;

    // Render views
    LDKRendererView* views;
    u32 view_count;
    u32 view_capacity;
    LDKRendererViewId game_view;

    // Mesh cache
    LDKRendererMeshResource* meshes;
    u32 mesh_count;
    u32 mesh_capacity;

    // Persistent instance sets
    LDKRendererInstanceSetResource *instance_sets;
    u32 instance_set_count;
    u32 instance_set_capacity;

    // Skybox cache
    LDKRendererSkyboxResource *skyboxes;
    u32 skybox_count;
    u32 skybox_capacity;

    // Texture cache
    LDKRendererTextureResource* textures;
    u32 texture_count;
    u32 texture_capacity;

    // Material cache
    LDKRendererMaterialResource* materials;
    u32 material_count;
    u32 material_capacity;
    LDKRendererMaterialSamplerResource* material_samplers;
    u32 material_sampler_count;
    u32 material_sampler_capacity;
    LDKResourceMaterial default_material;

    // Font atlas cache
    LDKRendererFontPageCacheEntry* font_pages;
    u32 font_page_count;
    u32 font_page_capacity;

    LDKRendererAmbientLight ambient_light;

    LDKResourceTexture vegetation_interaction_texture;
    Vec2 vegetation_interaction_origin;
    float vegetation_interaction_inv_world_size;
    bool vegetation_interaction_enabled;
    LDKRendererLightSubmit *submitted_lights;
    u32 submitted_light_count;
    u32 submitted_light_capacity;
    bool light_limit_reported;

    // Shared line geometry and transient submissions, owned by the renderer.
    LDKResourceMesh line_mesh;
    LDKRendererLineSubmit *submitted_lines;
    u32 submitted_line_count;
    u32 submitted_line_capacity;

    // Transient depth-tested wireframes, primarily used by editor tooling.
    LDKRendererWireframeSubmit *submitted_wireframes;
    u32 submitted_wireframe_count;
    u32 submitted_wireframe_capacity;

    // Transient world-space text geometry and draw submissions.
    LDKRendererTextVertex *submitted_text_vertices;
    u32 submitted_text_vertex_count;
    u32 submitted_text_vertex_capacity;
    u32 *submitted_text_indices;
    u32 submitted_text_index_count;
    u32 submitted_text_index_capacity;
    LDKRendererTextSubmit *submitted_texts;
    u32 submitted_text_count;
    u32 submitted_text_capacity;

    // Submitted meshes
    LDKRendererMeshSubmit* submitted_meshes;
    u32 submitted_mesh_count;
    u32 submitted_mesh_capacity;
    LDKRendererWaterSubmit *submitted_water;
    u32 submitted_water_count;
    u32 submitted_water_capacity;
    Mat4 *submitted_instance_worlds;
    LDKRHIColor *submitted_instance_colors;
    u32 submitted_instance_count;
    u32 submitted_instance_capacity;

    // Per-view opaque mesh sort scratch. Sort items pack a 40-bit state key
    // and a 24-bit index into submitted_meshes.
    u64* mesh_sort_items;
    u64* mesh_sort_scratch;
    u32 mesh_sort_capacity;
    u32* mesh_sort_mesh_ids;
    u32 mesh_sort_mesh_id_capacity;
    u32* mesh_sort_material_ids;
    u32 mesh_sort_material_id_capacity;

    // Frame statistics. last_frame_stats always describes the last fully
    // completed renderer frame; current_frame_stats is internal accumulation.
    LDKRendererFrameStats current_frame_stats;
    LDKRendererFrameStats last_frame_stats;

    u64 animation_start_ticks;
    float animation_time_seconds;

    bool is_initialized;
  } LDKRenderer;

  LDK_API void ldk_renderer_water_desc_defaults(LDKRendererWaterDesc *desc);
  LDK_API bool ldk_renderer_water_desc_is_valid(
      const LDKRendererWaterDesc *desc);
  /**
   * Submit a horizontal world-space grid. XZ positions come from the mesh;
   * the shader supplies Y using water_level and the three waves. Scene depth
   * is sampled after opaque rendering. Meshes and optional textures remain
   * caller-owned and must live until render_frame() completes. Overlapping
   * water surfaces are composited in submission order; this path does not
   * sort stacked water.
   */
  LDK_API bool ldk_renderer_submit_water(LDKRenderer *renderer,
      LDKRendererViewId view_id, LDKResourceMesh mesh,
      const LDKRendererWaterDesc *desc);

  /** Set constant ambient light applied to lit materials.
   * Color uses 0xRRGGBBAA; alpha is ignored. Intensity must be finite and
   * non-negative. Ambient light does not count toward the per-view light limit.
   */
  LDK_API bool ldk_renderer_ambient_light_set(
      LDKRenderer *renderer, u32 color, float intensity);

  /** Bind a world-space interaction field sampled by vegetation shaders. */
  LDK_API bool ldk_renderer_vegetation_interaction_set(
      LDKRenderer *renderer, LDKResourceTexture texture, Vec2 world_origin,
      float world_size);

  /** Disable the vegetation interaction field and release pass bindings. */
  LDK_API void ldk_renderer_vegetation_interaction_clear(
      LDKRenderer *renderer);

  /** Submit an unlit, capped triangular prism from start to end.
   * Thickness is the circumdiameter of its cross-section, in world units.
   * Depth test also enables depth writes; false uses the overlay path.
   * View may be ALL or an ID not yet submitted. Call on the render thread
   * before render_frame; the queue is cleared after that frame is rendered.
   * Zero-length segments succeed without drawing. Invalid/nonfinite data,
   * nonpositive thickness and allocation failures return false.
   */
  LDK_API bool ldk_renderer_draw_line(LDKRenderer *renderer,
      LDKRendererViewId view_id, Vec3 start, Vec3 end, float thickness,
      u32 color, bool depth_test);


  /** Submit UTF-8 text as unlit world-space geometry.
   * Glyph layout, kerning and atlas placement are shared with the UI text path.
   * Local text height is normalized by the font instance pixel height, so an
   * identity world transform produces approximately one world unit per font
   * height. The local origin is the top-left of the first line and local +Y is
   * up. BILLBOARD preserves translation, XY roll and scale while facing the
   * current camera. Text is alpha blended and does not write depth. Depth
   * testing is enabled by default and can be disabled per submission with
   * LDK_RENDERER_TEXT_SUBMIT_FLAG_NO_DEPTH_TEST. Views need not exist yet. The
   * transient queue is cleared after the frame is rendered.
   */
  LDK_API bool ldk_renderer_submit_text(LDKRenderer *renderer,
      LDKRendererViewId view_id, LDKFontInstance *font, char const *text,
      Mat4 world, u32 color, u32 flags);

  /** Submit transient world-space lighting. Views need not exist yet.
   * The first 16 matching lights illuminate each view, in submission order.
   * Returns false for invalid data or allocation failure.
   */
  LDK_API bool ldk_renderer_submit_light(LDKRenderer *renderer,
      const LDKRendererLightSubmit *light);


  /**
   * @brief Initialize the renderer.
   *
   * The renderer stores the supplied RHI context, creates its internal rendering
   * passes, and allocates the GPU resources required for built-in rendering.
   *
   * The RHI context must already be initialized and must remain alive until
   * ldk_renderer_terminate() is called.
   *
   * @param renderer Renderer instance to initialize.
   * @param config Renderer configuration.
   * @return true if the renderer was initialized successfully, false otherwise.
   */
  // Call between frames. Resolution: power of two, 256..8192;
  // distance: finite and positive. Failure preserves existing resources.
  LDK_API bool ldk_renderer_shadow_settings_set(
      LDKRenderer *renderer, u32 resolution, float distance);

  LDK_API bool ldk_renderer_initialize(
      LDKRenderer* renderer,
      LDKRendererConfig const* config);

  /**
   * @brief Change the default game render-target resolution.
   *
   * Views using the default game extent release their current render targets
   * when the dimensions change. Views with an explicit per-view extent are not
   * affected. New targets are created when those views are next submitted or
   * rendered.
   *
   * @param renderer Renderer instance.
   * @param width New game render-target width in pixels.
   * @param height New game render-target height in pixels.
   * @return true when the resolution is valid and was accepted.
   */
  LDK_API bool ldk_renderer_game_resolution_set(
      LDKRenderer* renderer,
      u32 width,
      u32 height);

  /**
   * @brief Terminate the renderer and release renderer-owned resources.
   *
   * This destroys all renderer-owned GPU resources, including mesh buffers,
   * texture resources, font page textures, render targets, pass buffers,
   * bindings, pipelines, samplers, and shader modules.
   *
   * CPU-side assets are not destroyed by this function. Assets remain owned by
   * the asset manager or by whoever created them.
   *
   * The RHI context must still be alive when this function is called.
   *
   * @param renderer Renderer instance to terminate.
   */
  LDK_API void ldk_renderer_terminate(
      LDKRenderer* renderer);

  /**
   * @brief Render one frame using the currently submitted renderer data.
   *
   * The renderer begins an RHI frame, renders the submitted scene meshes when a
   * view is available, optionally presents the scene target, renders submitted
   * UI data, and ends the RHI frame.
   *
   * Submitted per-frame data is consumed by this call. After rendering, the
   * renderer clears transient submissions such as submitted meshes, submitted UI,
   * and the active camera/view state.
   *
   * @param renderer Renderer instance.
   * @param desc Frame description containing framebuffer size and clear color.
   */
  LDK_API void ldk_renderer_render_frame(
      LDKRenderer* renderer,
      LDKRendererFrameDesc const* desc);

  /**
   * @brief Return renderer statistics for the last fully completed frame.
   *
   * CPU time measures the elapsed CPU-side duration of
   * ldk_renderer_render_frame(), including time spent inside RHI calls. It is
   * not GPU time and may include waits performed by the backend.
   *
   * Rendered work is also split by destination. game contains work rendered
   * into the configured game view (and direct game presentation/UI when
   * present_game is enabled). non_game contains all other views and host UI. A
   * VIEW_ALL submission is counted independently in each destination where it
   * is actually rendered. mesh_submit_count remains global because a single
   * submission may participate in both domains.
   *
   * @param renderer Renderer instance.
   * @return A snapshot of the previous completed renderer frame. A zeroed
   *         snapshot is returned for a null renderer or before the first frame.
   */
  LDK_API LDKRendererFrameStats ldk_renderer_last_frame_stats_get(
      LDKRenderer const* renderer);

  /**
   * @brief Return the game render target color texture for UI rendering.
   *
   * The returned texture is owned by the renderer and remains valid until the
   * game render target is recreated or the renderer is terminated.
   *
   * @param renderer Renderer instance.
   * @return Game color texture handle, or an invalid handle when unavailable.
   */
  LDK_API LDKUITextureHandle ldk_renderer_game_texture_get(
      LDKRenderer const* renderer);

  /**
   * @brief Return the color texture rendered for a submitted view.
   *
   * The view identifier is supplied by the caller when the view is submitted.
   * The returned texture is owned by the renderer and remains valid until the
   * view is removed, the game resolution changes, or the renderer terminates.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of the submitted view.
   * @return View color texture handle, or an invalid handle when unavailable.
   */
  LDK_API LDKUITextureHandle ldk_renderer_view_texture_get(
      LDKRenderer const* renderer,
      LDKRendererViewId view_id);

  /**
   * @brief Set the render-target extent requested for a view.
   *
   * The request is applied the next time the view is submitted. Until then the
   * current render target remains unchanged, allowing editor resize operations
   * to defer GPU resource churn until interaction has finished.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of an existing renderer view.
   * @param width Requested render-target width in pixels.
   * @param height Requested render-target height in pixels.
   * @return true when the request was accepted, false otherwise.
   */
  LDK_API bool ldk_renderer_view_extent_set(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      u32 width,
      u32 height);

  /**
   * @brief Get the effective extent that will be used by a renderer view.
   *
   * Views without an explicit extent use the game resolution. For an explicit
   * view, this returns its requested extent, including a request that will be
   * applied by the next ldk_renderer_submit_view() call.
   *
   * @param renderer Renderer instance.
   * @param view_id View identifier. The view does not need to exist yet.
   * @param out_width Receives the effective width in pixels.
   * @param out_height Receives the effective height in pixels.
   * @return true when a valid effective extent was returned.
   */
  LDK_API bool ldk_renderer_view_extent_get(
      LDKRenderer const* renderer,
      LDKRendererViewId view_id,
      u32* out_width,
      u32* out_height);

  /**
   * @brief Return the UV rectangle used to display renderer view textures.
   *
   * View textures are render targets and their storage origin may differ from
   * ordinary image textures. Callers that display a view texture through the UI
   * should use this rectangle instead of assuming a texture origin.
   *
   * @param renderer Renderer instance that owns the view texture.
   * @return Normalized UV rectangle for sampling a renderer view texture.
   */
  LDK_API LDKUIRect ldk_renderer_view_texture_uv_get(
      LDKRenderer const* renderer);
  /**
   * @brief Set post-processing configuration for a submitted view.
   *
   * The configuration is transient and must be supplied again after the view is
   * submitted on each frame. Disabled post-processing bypasses the post-process
   * pass and exposes the scene color target directly.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of a view submitted for the current frame.
   * @param post_processing Post-processing configuration for the view.
   * @return true when the configuration was accepted, false otherwise.
   */
  LDK_API bool ldk_renderer_view_post_processing_set(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKRendererPostProcessing const* post_processing);

  /* Route overlay meshes to a transparent texture for this frame. Request
   * before rendering and compose the texture above other viewer UI layers. */
  LDK_API LDKUITextureHandle ldk_renderer_view_overlay_texture_request(
      LDKRenderer* renderer, LDKRendererViewId view_id);

  // ---------------------------------------------------------------------------
  // Mesh Resource
  // ---------------------------------------------------------------------------
  /**
   * @brief Return an invalid mesh resource handle.
   *
   * This is the renderer mesh equivalent of a null handle. It can be used
   * to initialize mesh fields or to represent the absence of a mesh.
   *
   * @return Invalid mesh resource handle.
   */
  LDK_API LDKResourceMesh ldk_renderer_mesh_null(void);

  /**
   * @brief Check whether a mesh resource handle refers to a live renderer mesh.
   *
   * The handle is validated against the renderer that owns the mesh resource.
   * A non-zero handle is not enough to prove validity; the resource must still
   * exist in the renderer mesh cache.
   *
   * @param renderer Renderer that owns the mesh resource.
   * @param mesh Mesh resource handle to validate.
   * @return true if the mesh is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_mesh_is_valid(
      LDKRenderer* renderer,
      LDKResourceMesh mesh);

  /**
   * @brief Create a renderer-owned mesh resource.
   *
   * The renderer uploads the supplied CPU-side vertex and index data into
   * renderer-owned RHI buffers, stores those buffers in the renderer mesh cache,
   * and returns a renderer resource handle.
   *
   * The source vertex and index arrays are only needed during creation. After the
   * mesh is created, the returned resource does not depend on the original CPU
   * buffers.
   *
   * @param renderer Renderer that will own the mesh resource.
   * @param desc Mesh creation description containing vertices and indices.
   * @return Mesh resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceMesh ldk_renderer_mesh_create(
      LDKRenderer* renderer,
      LDKRendererMeshDesc const* desc);

  /**
   * @brief Replace the contents of an existing renderer mesh resource.
   *
   * The mesh handle is resolved through the renderer mesh cache. The old
   * renderer-owned RHI vertex and index buffers are destroyed, then new buffers
   * are created from the supplied mesh description.
   *
   * The source vertex and index arrays are only needed during the update. After
   * the update succeeds, the mesh does not depend on the original CPU buffers.
   *
   * @param renderer Renderer that owns the mesh resource.
   * @param mesh Mesh resource handle to update.
   * @param desc New mesh data description.
   * @return true if the mesh was updated, false otherwise.
   */
  LDK_API bool ldk_renderer_mesh_update(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKRendererMeshDesc const* desc);

  /**
   * @brief Destroy a renderer-owned mesh resource.
   *
   * The renderer resolves the mesh handle, destroys the underlying RHI vertex and
   * index buffers, and releases the renderer mesh cache slot.
   *
   * Destroying an invalid or already-dead mesh handle is a no-op. Mesh resources
   * are also destroyed automatically when the renderer is terminated.
   *
   * @param renderer Renderer that ownss the mesh resource.
   * @param mesh Mesh resource handle to destroy.
   */
  LDK_API void ldk_renderer_mesh_destroy(
      LDKRenderer* renderer,
      LDKResourceMesh mesh);

  // ---------------------------------------------------------------------------
  // Texture Resource
  // ---------------------------------------------------------------------------
  /**
   * @brief Initialize renderer texture options with default values.
   *
   * Defaults preserve the renderer's existing texture behavior: nearest filtering,
   * clamp-to-edge wrapping, and no generated mipmaps.
   *
   * @param options Texture options to initialize.
   */
  LDK_API void ldk_renderer_texture_options_defaults(
      LDKRendererTextureOptions* options);

  /**
   * @brief Return an invalid texture resource handle.
   *
   * This is the renderer texture equivalent of a null handle. It can be used
   * to initialize texture fields or to represent the absence of a texture.
   *
   * @return Invalid texture resource handle.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_null(void);

  /**
   * @brief Check whether a texture resource handle refers to a live renderer texture.
   *
   * The handle is validated against the renderer that owns the texture resource.
   * A non-zero handle is not enough to prove validity; the resource must still
   * exist in the renderer texture cache.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to validate.
   * @return true if the texture is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_texture_is_valid(
      LDKRenderer* renderer,
      LDKResourceTexture texture);

  /**
   * @brief Resolve a renderer texture resource to a UI draw texture handle.
   *
   * The returned handle is a frame-local draw token suitable for UI draw
   * commands. The UI system must treat it as opaque and must not store it as a
   * persistent engine resource.
   *
   * The renderer owns the original texture resource. If the resource is invalid
   * or no longer alive, this function returns an invalid UI texture handle.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to resolve.
   * @return UI texture handle for draw commands, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_texture_ui_handle(
      LDKRenderer* renderer,
      LDKResourceTexture texture);

  /**
   * @brief Create a renderer-owned texture resource.
   *
   * The renderer translates the high-level texture description into the
   * appropriate RHI texture description, creates the underlying GPU texture,
   * stores it in the renderer texture cache, and returns a renderer resource
   * handle.
   *
   * The source pixel data is only needed during creation. After the texture is
   * created, the returned resource does not depend on the original pixel buffer.
   *
   * @param renderer Renderer that will own the texture resource.
   * @param desc Texture creation description.
   * @return Texture resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_create(
      LDKRenderer* renderer,
      LDKRendererTextureDesc const* desc);

  /**
   * @brief Update the pixel contents of an existing renderer texture resource.
   *
   * The texture handle is resolved through the renderer texture cache, then the
   * underlying RHI texture is updated with the supplied pixel data.
   *
   * The texture dimensions, format, channel count, and flags are not changed by
   * this call. The supplied byte count must match the expected data size for the
   * existing texture.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to update.
   * @param pixels New pixel data.
   * @param byte_count Size of the pixel data in bytes.
   * @return true if the texture was updated, false otherwise.
   */
  LDK_API bool ldk_renderer_texture_update(
      LDKRenderer* renderer,
      LDKResourceTexture texture,
      void const* pixels,
      u64 byte_count);

  /**
   * @brief Destroy a renderer-owned texture resource.
   *
   * The renderer resolves the texture handle, destroys the underlying RHI texture,
   * and releases the renderer texture cache slot.
   *
   * Destroying an invalid or already-dead texture handle is a no-op. Texture
   * resources are also destroyed automatically when the renderer is terminated.
   *
   * @param renderer Renderer that owns the texture resource.
   * @param texture Texture resource handle to destroy.
   */
  LDK_API void ldk_renderer_texture_destroy(
      LDKRenderer* renderer,
      LDKResourceTexture texture);


  /**
   * @brief Create a renderer texture resource from an image with explicit options.
   *
   * Passing NULL for options uses ldk_renderer_texture_options_defaults().
   *
   * @param renderer Renderer that will own the texture resource.
   * @param image Source CPU-side image.
   * @param options Explicit texture sampling and mipmap options, or NULL.
   * @return Texture resource handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceTexture ldk_renderer_texture_create_from_image(
      LDKRenderer* renderer,
      LDKImage const* image,
      LDKRendererTextureOptions const* options);

  // ---------------------------------------------------------------------------
  // Material Resource
  // ---------------------------------------------------------------------------

  /**
   * @brief Return an invalid material resource handle.
   * @return Invalid material resource handle.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_null(void);

  /* Acquire a shared GPU snapshot of a live image asset. The shared texture
   * includes a full mip chain; sampler state decides whether mip levels are
   * used. Identity includes manager, asset index and generation. Release
   * once per acquisition; do not destroy the borrowed texture directly.
   * In-place image edits/hot reload are not tracked by this snapshot cache.
   */
  LDK_API LDKResourceTexture ldk_renderer_image_acquire(
      LDKRenderer* renderer, struct LDKAssetManager* assets,
      LDKAssetImage image);
  LDK_API void ldk_renderer_image_release(
      LDKRenderer* renderer, LDKResourceTexture texture);

  /**
   * @brief Resolve an authored material description into renderer resources.
   *
   * Textured materials acquire a shared renderer texture from the supplied
   * asset manager. If the authored image is unavailable, the renderer uses
   * the shared missing-image fallback. Shader-defined slots 3..7 are acquired
   * the same way and are owned by the resolved material. Lit normal/specular
   * maps are acquired when available; missing maps resolve to renderer-owned
   * neutral fallbacks.
   * Existing output resources are replaced only after the new material has
   * been created successfully.
   *
   * The caller owns the returned material resource and one acquisition of each
   * non-null returned image texture. A later successful resolve replaces those
   * resources. Additional slots 3..7 are released by material destruction.
   * The caller must destroy/release the remaining resources when its owner
   * terminates.
   *
   * @param renderer Renderer that owns the runtime resources.
   * @param assets Asset manager used to resolve authored image assets. Required
   * for textured materials and for lit materials that reference maps.
   * @param material_desc Authored material description.
   * @param renderer_material In/out renderer material resource.
   * @param renderer_texture In/out acquired albedo texture resource.
   * @param renderer_normal_map In/out acquired normal-map texture resource.
   * @param renderer_specular_map In/out acquired specular-map texture resource.
   * @return true when the material was resolved successfully.
   */
  LDK_API bool ldk_renderer_material_resolve(LDKRenderer *renderer,
      struct LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
      LDKResourceMaterial *renderer_material,
      LDKResourceTexture *renderer_texture,
      LDKResourceTexture *renderer_normal_map,
      LDKResourceTexture *renderer_specular_map);

  /** Resolve a normal authored material using TerrainSystem surface rules. */
  LDK_API bool ldk_renderer_terrain_material_resolve(LDKRenderer *renderer,
      struct LDKAssetManager *assets, LDKMaterialDesc const *material_desc,
      const LDKRendererTerrainSurfaceDesc *surfaces, u32 surface_count,
      LDKResourceMaterial *renderer_material,
      LDKResourceTexture *renderer_texture,
      LDKResourceTexture *renderer_normal_map,
      LDKResourceTexture *renderer_specular_map);

  /**
   * @brief Check whether a material handle refers to a live renderer material.
   * @param renderer Renderer that owns the material resource.
   * @param material Material resource handle to validate.
   * @return true if the material is valid and alive, false otherwise.
   */
  LDK_API bool ldk_renderer_material_is_valid(
      LDKRenderer* renderer,
      LDKResourceMaterial material);

  /**
   * @brief Create a renderer-owned material resource.
   *
   * Textured materials require a live renderer texture. The referenced texture
   * must remain alive until the material is destroyed. Textured materials may
   * use opaque or cutout alpha mode; cutout thresholds must be finite and in
   * [0, 1]. Lit normal/specular maps and shader-defined slots 3..7 are optional
   * borrowed texture resources and must remain alive while the material exists;
   * null map handles use renderer-owned neutral fallbacks. Vertex-color
   * materials
   * ignore texture and
   * alpha fields. Unlit materials ignore map handles. Lit surface values must be
   * finite and non-negative. A zero shininess uses the default value (32).
   *
   * @param renderer Renderer that will own the material resource.
   * @param desc Resolved renderer material description.
   * @return New material handle, or an invalid handle on failure.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_create(
      LDKRenderer* renderer,
      LDKRendererMaterialDesc const* desc);

  /**
   * @brief Destroy a renderer-owned material resource.
   *
   * Destroying an invalid or already-dead material is a no-op. Material
   * resources are also destroyed automatically when the renderer terminates.
   *
   * @param renderer Renderer that owns the material resource.
   * @param material Material resource to destroy.
   */
  LDK_API void ldk_renderer_material_destroy(
      LDKRenderer* renderer,
      LDKResourceMaterial material);

  /**
   * @brief Return the renderer-owned default mesh material.
   *
   * The default material is a white vertex-color material created during
   * renderer initialization and destroyed during renderer termination.
   *
   * @param renderer Renderer that owns the default material.
   * @return Default material handle, or an invalid handle when unavailable.
   */
  LDK_API LDKResourceMaterial ldk_renderer_material_default_get(
      LDKRenderer* renderer);

  // ---------------------------------------------------------------------------
  // Font cache Resources
  // ---------------------------------------------------------------------------
  /**
   * @brief Get or create the renderer texture for a font atlas page.
   *
   * The UI system renders text using font atlas pages generated by LDKFontInstance.
   * This function returns a texture handle for the requested page, creating the
   * underlying renderer/RHI texture on demand and caching it for later calls.
   *
   * The returned handle is intended for UI draw commands. It is owned by the
   * renderer and must not be destroyed by the caller. Cached font page textures
   * are released when the renderer is terminated.
   *
   * @param renderer Renderer that owns the font page texture cache.
   * @param font Font instance that owns the requested atlas page.
   * @param page_index Index of the font atlas page.
   * @return UI texture handle for the atlas page, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture(LDKRenderer* renderer, LDKFontInstance* font, u32 page_index);

  /**
   * @brief Callback wrapper for ldk_renderer_get_font_page_texture().
   *
   * This function matches the callback shape expected by the UI/font rendering
   * code. The user pointer must be an LDKRenderer*.
   *
   * @param user Renderer pointer passed as opaque callback user data.
   * @param font Font instance that owns the requested atlas page.
   * @param page_index Index of the font atlas page.
   * @return UI texture handle for the atlas page, or an invalid handle on failure.
   */
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index);

  // ---------------------------------------------------------------------------
  // Primitive Submission
  // ---------------------------------------------------------------------------
  /**
   * @brief Submit a camera view used for scene rendering this frame.
   *
   * The renderer stores the view and projection matrices as transient frame
   * state. Submitted meshes will be rendered using this view when
   * ldk_renderer_render_frame() is called.
   *
   * The submitted view is consumed for the current frame only. After rendering,
   * the renderer clears the active view state.
   *
   * @param renderer Renderer instance.
   * @param view_id Stable non-zero identifier supplied by the caller.
   * @param view View matrix.
   * @param projection Projection matrix.
   * @return true if the view was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      Mat4 view,
      Mat4 projection);

  /**
   * @brief Override the clear color used by a submitted view this frame.
   *
   * This is transient view state and must be set again after submit_view on
   * each frame. Alpha is preserved, allowing transparent camera backgrounds.
   */
  LDK_API bool ldk_renderer_view_clear_color_set(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      rgba32 clear_color);

  /**
   * @brief Select the submitted view used as the game output.
   *
   * The selected view is returned by ldk_renderer_game_texture_get() and is
   * presented directly when the renderer was configured with present_game.
   * The selection is transient and must be set again on every frame.
   *
   * @param renderer Renderer instance.
   * @param view_id Identifier of a view submitted for the current frame.
   * @return true if the submitted view exists, false otherwise.
   */
  LDK_API bool ldk_renderer_game_view_set(
      LDKRenderer* renderer,
      LDKRendererViewId view_id);

  /**
   * @brief Submit UI render data for the current frame.
   *
   * The renderer stores a pointer to the supplied UI render data and renders it
   * during ldk_renderer_render_frame().
   *
   * The render data is not copied, so it must remain valid until the frame is
   * rendered. After rendering, the renderer clears the submitted UI pointer.
   *
   * @param renderer Renderer instance.
   * @param render_data UI render data to draw this frame.
   */
  LDK_API void ldk_renderer_submit_ui(
      LDKRenderer* renderer,
      LDKUIRenderData const* render_data);

  /** Explicit instancing, one submission per mesh/range and matrix array.
   * Matrices are copied during this call and may be released immediately.
   * parent_world transforms each supplied local matrix into world space;
   * pass identity when supplying final world matrices. view_id accepts ALL.
   * index_count must be nonzero. Zero instances is a successful no-op with
   * valid resources/range. Invalid data/allocation failure queues nothing.
   * Ordinary submits never use instancing. Unsupported instancing falls back
   * to one ordinary draw per instance, including ordered overlays.
   */
  LDK_API bool ldk_renderer_submit_mesh_instances(LDKRenderer *renderer,
      LDKRendererViewId view_id, LDKResourceMesh mesh,
      LDKResourceMaterial material, u32 first_index, u32 index_count,
      Mat4 parent_world, const Mat4 *instances, u32 instance_count, u32 flags);

  /** Explicit instancing with one optional RGBA tint per instance. Colors use
   * 0xRRGGBBAA and are copied together with the instance transforms. A NULL
   * color array means opaque white for every instance.
   */
  LDK_API bool ldk_renderer_submit_mesh_instances_colored(
      LDKRenderer *renderer, LDKRendererViewId view_id, LDKResourceMesh mesh,
      LDKResourceMaterial material, u32 first_index, u32 index_count,
      Mat4 parent_world, const Mat4 *instances, const u32 *instance_colors,
      u32 instance_count, u32 flags);

  /** Persistent world-space instances uploaded only when the set changes. */
  LDK_API LDKResourceInstanceSet ldk_renderer_instance_set_create(
      LDKRenderer *renderer, const Mat4 *instances, u32 instance_count);
  LDK_API bool ldk_renderer_instance_set_update(LDKRenderer *renderer,
      LDKResourceInstanceSet instance_set, const Mat4 *instances,
      u32 instance_count);
  LDK_API void ldk_renderer_instance_set_destroy(LDKRenderer *renderer,
      LDKResourceInstanceSet instance_set);
  LDK_API LDKResourceInstanceSet ldk_renderer_instance_set_null(void);
  LDK_API bool ldk_renderer_instance_set_is_valid(
      LDKRenderer *renderer, LDKResourceInstanceSet instance_set);

  /** Submit one persistent instance set without copying its transforms. */
  LDK_API bool ldk_renderer_submit_mesh_instance_set(LDKRenderer *renderer,
      LDKRendererViewId view_id, LDKResourceMesh mesh,
      LDKResourceMaterial material, u32 first_index, u32 index_count,
      LDKResourceInstanceSet instance_set, u32 flags);

  /** Submit a contiguous range of a persistent instance set. */
  LDK_API bool ldk_renderer_submit_mesh_instance_set_range(
      LDKRenderer *renderer, LDKRendererViewId view_id,
      LDKResourceMesh mesh, LDKResourceMaterial material,
      u32 first_index, u32 index_count,
      LDKResourceInstanceSet instance_set, u32 first_instance,
      u32 instance_count, u32 flags);

  /** True when an AABB is visible to the selected submitted view. VIEW_ALL
   * tests every submitted view and succeeds when any view intersects it.
   * Invalid bounds or unavailable frusta fail open and return true.
   */
  LDK_API bool ldk_renderer_view_bounds_visible(
      const LDKRenderer *renderer, LDKRendererViewId view_id,
      Vec3 bounds_min, Vec3 bounds_max);

  /**
   * @brief Submit a mesh instance for scene rendering this frame.
   *
   * The mesh and material handles are validated against their renderer caches,
   * then queued with the supplied world transform. The queued mesh is rendered
   * during ldk_renderer_render_frame() for every submitted view.
   *
   * Submitted mesh instances are transient frame data. After rendering, the
   * renderer clears the submitted mesh queue.
   *
   * @param renderer Renderer instance.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);

  /* Explicit flags for scene/procedural submissions. The convenience mesh
   * submission functions enable CAST_SHADOWS; overlay submissions never cast.
   * These forms allow disabling casting without changing material lighting.
   */
  LDK_API bool ldk_renderer_submit_mesh_with_flags(LDKRenderer *renderer,
      LDKResourceMesh mesh, LDKResourceMaterial material, Mat4 world,
      u32 flags);
  LDK_API bool ldk_renderer_submit_mesh_range_with_flags(LDKRenderer *renderer,
      LDKResourceMesh mesh, LDKResourceMaterial material, u32 first_index,
      u32 index_count, Mat4 world, u32 flags);

  /**
   * @brief Submit a contiguous index range of a mesh for scene rendering.
   *
   * This is the ranged form of ldk_renderer_submit_mesh(). The mesh resource
   * remains independent of material topology; callers use this function to
   * submit an individual submesh with its resolved material.
   *
   * @param renderer Renderer instance.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param first_index First mesh index to draw.
   * @param index_count Number of indices to draw.
   * @param world World transform for this mesh instance.
   * @return true if the mesh range was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh_range(
      LDKRenderer* renderer,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      u32 first_index,
      u32 index_count,
      Mat4 world);

  /**
   * @brief Submit a mesh instance to a single render view for this frame.
   *
   * Unlike ldk_renderer_submit_mesh(), this mesh is only drawn while rendering
   * the view identified by view_id. This is intended for view-local scene
   * content that must not appear in other views.
   *
   * The view does not need to have been submitted before this call. If no view
   * with the supplied ID is submitted during the frame, the mesh is not drawn.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw this mesh.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);


  // ---------------------------------------------------------------------------
  // Skybox Resource
  // ---------------------------------------------------------------------------

  LDK_API LDKResourceSkybox ldk_renderer_skybox_null(void);
  LDK_API bool ldk_renderer_skybox_is_valid(
      LDKRenderer *renderer, LDKResourceSkybox skybox);
  LDK_API LDKResourceSkybox ldk_renderer_skybox_create(
      LDKRenderer *renderer, struct LDKAssetManager *assets,
      const LDKSkyboxDesc *desc);
  LDK_API void ldk_renderer_skybox_destroy(
      LDKRenderer *renderer, LDKResourceSkybox skybox);
  LDK_API bool ldk_renderer_submit_skybox_to_view(
      LDKRenderer *renderer, LDKRendererViewId view_id,
      LDKResourceSkybox skybox);

  /**
   * @brief Submit a procedural ground grid to a single render view.
   *
   * The grid is rendered after regular scene meshes and before overlay meshes,
   * with depth testing enabled and depth writes disabled. Its visible area is
   * centered on grid_center and fades before reaching grid_extent.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw the grid.
   * @param grid_center Center of the visible grid area on the ground plane.
   * @param grid_extent Radius of the visible grid area in world units.
   * @param grid_spacing Distance between adjacent grid lines in world units.
   * @return true if the grid was submitted, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_grid_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      Vec3 grid_center,
      float grid_extent,
      float grid_spacing);

  /**
   * @brief Submit a mesh as a view-local overlay for this frame.
   *
   * Overlay meshes are rendered after regular meshes for the selected view,
   * with depth testing and depth writes disabled. This makes them independent
   * of the scene depth buffer and suitable for editor gizmos.
   *
   * @param renderer Renderer instance.
   * @param view_id ID of the only view that should draw this mesh.
   * @param mesh Mesh resource handle to render.
   * @param material Material resource handle to use.
   * @param world World transform for this mesh instance.
   * @return true if the mesh was queued, false otherwise.
   */
  LDK_API bool ldk_renderer_submit_overlay_mesh_to_view(
      LDKRenderer* renderer,
      LDKRendererViewId view_id,
      LDKResourceMesh mesh,
      LDKResourceMaterial material,
      Mat4 world);

  /**
   * @brief Submit a depth-tested wireframe mesh to one render view.
   *
   * The mesh is drawn after scene meshes and before editor overlays. It uses
   * the scene depth buffer without writing depth, so hidden geometry remains
   * occluded by the rendered scene. Color uses 0xRRGGBBAA and supports alpha
   * blending. line_width is requested in pixels and may be clamped by the RHI
   * backend. flags accepts LDK_RENDERER_MESH_SUBMIT_FLAG_BILLBOARD only.
   */
  LDK_API bool ldk_renderer_submit_wireframe_mesh_to_view(
      LDKRenderer *renderer, LDKRendererViewId view_id,
      LDKResourceMesh mesh, Mat4 world, u32 color, float line_width,
      u32 flags);


#ifdef __cplusplus
}
#endif

#endif
