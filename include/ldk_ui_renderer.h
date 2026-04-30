#ifndef LDK_UI_RENDERER_H
#define LDK_UI_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include "ldk_rhi.h"
#include "ldk_ui.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct LDKUIRendererConfig
{
  LDKRHIContext* rhi;
  u32 initial_vertex_capacity;
  u32 initial_index_capacity;
} LDKUIRendererConfig;

typedef struct LDKUIRendererBindingsCacheEntry
{
  LDKRHITexture texture;
  LDKRHIBindings bindings;
} LDKUIRendererBindingsCacheEntry;

typedef struct LDKUIRenderer
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
  LDKUIRendererBindingsCacheEntry* bindings_cache;
  u32 bindings_cache_count;
  u32 bindings_cache_capacity;
  u32 vertex_capacity;
  u32 index_capacity;
  bool is_initialized;
} LDKUIRenderer;

bool ldk_ui_renderer_initialize(LDKUIRenderer* renderer, LDKUIRendererConfig const* config);
void ldk_ui_renderer_terminate(LDKUIRenderer* renderer);

void ldk_ui_renderer_draw(
  LDKUIRenderer* renderer,
  LDKUIRenderData const* render_data,
  i32 framebuffer_width,
  i32 framebuffer_height
);

#ifdef __cplusplus
}
#endif

#endif
