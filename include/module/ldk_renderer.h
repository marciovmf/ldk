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
#include <ldk_mesh.h>
#include <ldk_resource.h>
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
    LDK_SHADER_MESH_PASS,
    LDK_SHADER_PRESENT_PASS,
    LDK_SHADER_MESH_PASS_INSTANCED
  } LDKShader;

  typedef struct LDKRendererMeshDesc
  {
    const LDKMeshVertex* vertices;
    u32 vertex_count;
    const u32* indices;
    u32 index_count;
  } LDKRendererMeshDesc;

  typedef struct LDKRendererMeshResource
  {
    LDKRHIBuffer vertex_buffer;
    LDKRHIBuffer index_buffer;
    u32 vertex_count;
    u32 index_count;
    bool alive;
  } LDKRendererMeshResource;

  typedef struct LDKRendererMeshSubmit
  {
    LDKResourceMesh mesh;
    Mat4 world;
  } LDKRendererMeshSubmit;

  typedef struct LDKRendererConfig
  {
    LDKRHIContext* rhi;
    u32 initial_ui_vertex_capacity;
    u32 initial_ui_index_capacity;
  } LDKRendererConfig;

  typedef struct LDKRendererView
  {
    Mat4 view;
    Mat4 projection;
  } LDKRendererView;

  typedef struct LDKRendererFrameDesc
  {
    i32 framebuffer_width;
    i32 framebuffer_height;
    rgba32 clear_color;
    bool clear_color_enabled;
  } LDKRendererFrameDesc;

  typedef struct LDKRendererBindingsCacheEntry
  {
    LDKRHITexture texture;
    LDKRHIBindings bindings;
  } LDKRendererBindingsCacheEntry;

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

  typedef struct LDKRendererMeshPass
  {
    LDKRHIContext* rhi;
    LDKRHIShaderModule vertex_shader_module;
    LDKRHIShaderModule fragment_shader_module;
    LDKRHIBindingsLayout bindings_layout;
    LDKRHIPipeline pipeline;
    LDKRHIBuffer camera_buffer;
    LDKRHIBuffer object_buffer;
    LDKRHIBindings bindings;
    bool is_initialized;
  } LDKRendererMeshPass;

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

  typedef enum LDKRendererTextureFlag
  {
    LDK_RENDERER_TEXTURE_FLAG_NONE       = 0,
    LDK_RENDERER_TEXTURE_FLAG_SRGB       = 1 << 0,
    LDK_RENDERER_TEXTURE_FLAG_RENDERABLE = 1 << 1
  } LDKRendererTextureFlag;

  typedef struct LDKRendererTextureDesc
  {
    u32 width;
    u32 height;
    u32 channel_count;
    u32 flags;
    void const* pixels;
    u64 byte_count;
  } LDKRendererTextureDesc;

  typedef struct LDKRendererTextureResource
  {
    LDKRHITexture texture;
    u32 width;
    u32 height;
    u32 channel_count;
    LDKRHIFormat format;
    u32 flags;
    bool alive;
  } LDKRendererTextureResource;

  typedef struct LDKRenderer
  {
    LDKRHIContext* rhi;
    LDKRendererUIPass ui_pass;
    LDKRendererMeshPass mesh_pass;
    LDKUIRenderData const* submitted_ui;
    LDKRendererTarget scene_target;

    // Mesh cache
    LDKRendererMeshResource* meshes;
    u32 mesh_count;
    u32 mesh_capacity;

    // Texture cache
    LDKRendererTextureResource* textures;
    u32 texture_count;
    u32 texture_capacity;

    // Font atlas cache
    LDKRendererFontPageCacheEntry* font_pages;
    u32 font_page_count;
    u32 font_page_capacity;

    // Submitted meshes
    LDKRendererMeshSubmit* submitted_meshes;
    u32 submitted_mesh_count;
    u32 submitted_mesh_capacity;

    // global state
    Mat4 camera_view;
    Mat4 camera_projection;
    bool has_camera;

    bool is_initialized;
  } LDKRenderer;

  LDK_API bool ldk_renderer_initialize(LDKRenderer* renderer, LDKRendererConfig const* config);
  LDK_API void ldk_renderer_terminate(LDKRenderer* renderer);
  LDK_API void ldk_renderer_render_frame(LDKRenderer* renderer, LDKRendererFrameDesc const* desc);

  // ---------------------------------------------------------------------------
  // Mesh Resource
  // ---------------------------------------------------------------------------
  LDK_API LDKResourceMesh ldk_renderer_mesh_null(void);
  LDK_API bool ldk_renderer_mesh_is_valid(LDKRenderer* renderer, LDKResourceMesh mesh);
  LDK_API LDKResourceMesh ldk_renderer_mesh_create(LDKRenderer* renderer, LDKRendererMeshDesc const* desc);
  LDK_API bool ldk_renderer_mesh_update(LDKRenderer* renderer, LDKResourceMesh mesh, LDKRendererMeshDesc const* desc);
  LDK_API void ldk_renderer_mesh_destroy(LDKRenderer* renderer, LDKResourceMesh mesh);

  // ---------------------------------------------------------------------------
  // Texture Resource
  // ---------------------------------------------------------------------------
  LDK_API LDKResourceTexture ldk_renderer_texture_null(void);
  LDK_API bool ldk_renderer_texture_is_valid(LDKRenderer* renderer, LDKResourceTexture texture);
  LDK_API LDKResourceTexture ldk_renderer_texture_create(LDKRenderer* renderer, LDKRendererTextureDesc const* desc);
  LDK_API bool ldk_renderer_texture_update(LDKRenderer* renderer, LDKResourceTexture texture, void const* pixels, u64 byte_count);
  LDK_API void ldk_renderer_texture_destroy(LDKRenderer* renderer, LDKResourceTexture texture);
  LDK_API LDKResourceTexture ldk_renderer_texture_create_from_image(LDKRenderer* renderer, LDKImage const* image, u32 flags);

  // ---------------------------------------------------------------------------
  // Font cache Resources
  // ---------------------------------------------------------------------------
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture(LDKRenderer* renderer, LDKFontInstance* font, u32 page_index);
  LDK_API LDKUITextureHandle ldk_renderer_get_font_page_texture_callback(void* user, LDKFontInstance* font, u32 page_index);

  // ---------------------------------------------------------------------------
  // Primitive Submission
  // --------------------------------------------------------------------
  LDK_API bool ldk_renderer_submit_view(LDKRenderer* renderer, Mat4 view, Mat4 projection);
  LDK_API void ldk_renderer_submit_ui(LDKRenderer* renderer, LDKUIRenderData const* render_data);
  LDK_API bool ldk_renderer_submit_mesh(LDKRenderer* renderer, LDKResourceMesh mesh, Mat4 world);


#ifdef __cplusplus
}
#endif

#endif
