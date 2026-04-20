#ifndef LDK_UI_RENDERER_H
#define LDK_UI_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include "ldk_ui.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct LDKUIRendererConfig
{
  u32 initial_vertex_capacity;
  u32 initial_index_capacity;
} LDKUIRendererConfig;

typedef struct LDKUIRenderer
{
  u32 program;
  u32 vertex_shader;
  u32 fragment_shader;
  u32 vao;
  u32 vbo;
  u32 ebo;
  u32 white_texture;
  i32 viewport_size_uniform;
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
