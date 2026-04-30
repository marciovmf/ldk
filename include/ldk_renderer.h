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
#include "ldk_rhi.h"
#include "ldk_ui.h"

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
    LDK_SHADER_UI_PASS
  } LDKShader;

  typedef struct LDKRendererConfig
  {
    LDKRHIContext* rhi;
    u32 initial_ui_vertex_capacity;
    u32 initial_ui_index_capacity;
  } LDKRendererConfig;

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

  typedef struct LDKRenderer
  {
    LDKRHIContext* rhi;
    LDKRendererUIPass ui_pass;
    LDKUIRenderData const* submitted_ui;
    bool is_initialized;
  } LDKRenderer;

  bool ldk_renderer_initialize(LDKRenderer* renderer, LDKRendererConfig const* config);
  void ldk_renderer_terminate(LDKRenderer* renderer);
  void ldk_renderer_submit_ui(LDKRenderer* renderer, LDKUIRenderData const* render_data);
  void ldk_renderer_render_frame(LDKRenderer* renderer, LDKRendererFrameDesc const* desc);

#ifdef __cplusplus
}
#endif

#endif
