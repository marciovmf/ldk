#include <ldk_common.h>
#include <ldk_gl.h>
#include "ldk_ui_renderer.h"

#include <stddef.h>
#include <string.h>

static char const* LDK_UI_RENDERER_VERTEX_SHADER =
  "#version 330 core\n"
  "layout(location = 0) in vec2 a_position;\n"
  "layout(location = 1) in vec2 a_uv;\n"
  "layout(location = 2) in vec4 a_color;\n"
  "out vec2 v_uv;\n"
  "out vec4 v_color;\n"
  "uniform vec2 u_viewport_size;\n"
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

static char const* LDK_UI_RENDERER_FRAGMENT_SHADER =
  "#version 330 core\n"
  "in vec2 v_uv;\n"
  "in vec4 v_color;\n"
  "out vec4 out_color;\n"
  "uniform sampler2D u_texture;\n"
  "void main()\n"
  "{\n"
  "  vec4 tex = texture(u_texture, v_uv);\n"
  "  out_color = tex * v_color;\n"
  "}\n";

static bool ldk_ui_renderer_compile_shader(u32* shader, GLenum type, char const* source)
{
  GLint compiled = 0;

  *shader = glCreateShader(type);

  if (*shader == 0)
  {
    return false;
  }

  glShaderSource(*shader, 1, &source, NULL);
  glCompileShader(*shader);
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);

  if (compiled == GL_FALSE)
  {
    glDeleteShader(*shader);
    *shader = 0;
    return false;
  }

  return true;
}

static bool ldk_ui_renderer_create_program(LDKUIRenderer* renderer)
{
  GLint linked = 0;

  if (!ldk_ui_renderer_compile_shader(&renderer->vertex_shader, GL_VERTEX_SHADER, LDK_UI_RENDERER_VERTEX_SHADER))
  {
    return false;
  }

  if (!ldk_ui_renderer_compile_shader(&renderer->fragment_shader, GL_FRAGMENT_SHADER, LDK_UI_RENDERER_FRAGMENT_SHADER))
  {
    glDeleteShader(renderer->vertex_shader);
    renderer->vertex_shader = 0;
    return false;
  }

  renderer->program = glCreateProgram();

  if (renderer->program == 0)
  {
    glDeleteShader(renderer->vertex_shader);
    glDeleteShader(renderer->fragment_shader);
    renderer->vertex_shader = 0;
    renderer->fragment_shader = 0;
    return false;
  }

  glAttachShader(renderer->program, renderer->vertex_shader);
  glAttachShader(renderer->program, renderer->fragment_shader);
  glLinkProgram(renderer->program);
  glGetProgramiv(renderer->program, GL_LINK_STATUS, &linked);

  if (linked == GL_FALSE)
  {
    glDeleteProgram(renderer->program);
    glDeleteShader(renderer->vertex_shader);
    glDeleteShader(renderer->fragment_shader);
    renderer->program = 0;
    renderer->vertex_shader = 0;
    renderer->fragment_shader = 0;
    return false;
  }

  renderer->viewport_size_uniform = glGetUniformLocation(renderer->program, "u_viewport_size");

  glUseProgram(renderer->program);

  GLint texture_uniform = glGetUniformLocation(renderer->program, "u_texture");

  if (texture_uniform >= 0)
  {
    glUniform1i(texture_uniform, 0);
  }

  glUseProgram(0);

  return true;
}

static bool ldk_ui_renderer_create_white_texture(LDKUIRenderer* renderer)
{
  u32 pixel = 0xffffffffu;

  glGenTextures(1, &renderer->white_texture);

  if (renderer->white_texture == 0)
  {
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, renderer->white_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

static bool ldk_ui_renderer_create_buffers(LDKUIRenderer* renderer)
{
  glGenVertexArrays(1, &renderer->vao);
  glGenBuffers(1, &renderer->vbo);
  glGenBuffers(1, &renderer->ebo);

  if (renderer->vao == 0 || renderer->vbo == 0 || renderer->ebo == 0)
  {
    return false;
  }

  glBindVertexArray(renderer->vao);

  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(
    GL_ARRAY_BUFFER,
    (GLsizeiptr)(renderer->vertex_capacity * sizeof(LDKUIVertex)),
    NULL,
    GL_DYNAMIC_DRAW
  );

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER,
    (GLsizeiptr)(renderer->index_capacity * sizeof(u32)),
    NULL,
    GL_DYNAMIC_DRAW
  );

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
    0,
    2,
    GL_FLOAT,
    GL_FALSE,
    sizeof(LDKUIVertex),
    (void*)offsetof(LDKUIVertex, x)
  );

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
    1,
    2,
    GL_FLOAT,
    GL_FALSE,
    sizeof(LDKUIVertex),
    (void*)offsetof(LDKUIVertex, u)
  );

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
    2,
    4,
    GL_UNSIGNED_BYTE,
    GL_TRUE,
    sizeof(LDKUIVertex),
    (void*)offsetof(LDKUIVertex, color)
  );

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return true;
}

static void ldk_ui_renderer_ensure_vertex_capacity(LDKUIRenderer* renderer, u32 vertex_count)
{
  if (vertex_count <= renderer->vertex_capacity)
  {
    return;
  }

  while (renderer->vertex_capacity < vertex_count)
  {
    renderer->vertex_capacity *= 2;
  }

  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(
    GL_ARRAY_BUFFER,
    (GLsizeiptr)(renderer->vertex_capacity * sizeof(LDKUIVertex)),
    NULL,
    GL_DYNAMIC_DRAW
  );
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void ldk_ui_renderer_ensure_index_capacity(LDKUIRenderer* renderer, u32 index_count)
{
  if (index_count <= renderer->index_capacity)
  {
    return;
  }

  while (renderer->index_capacity < index_count)
  {
    renderer->index_capacity *= 2;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER,
    (GLsizeiptr)(renderer->index_capacity * sizeof(u32)),
    NULL,
    GL_DYNAMIC_DRAW
  );
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

bool ldk_ui_renderer_initialize(LDKUIRenderer* renderer, LDKUIRendererConfig const* config)
{
  memset(renderer, 0, sizeof(*renderer));

  renderer->vertex_capacity = config->initial_vertex_capacity > 0 ? config->initial_vertex_capacity : 1024;
  renderer->index_capacity = config->initial_index_capacity > 0 ? config->initial_index_capacity : 2048;

  if (!ldk_ui_renderer_create_program(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_white_texture(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  if (!ldk_ui_renderer_create_buffers(renderer))
  {
    ldk_ui_renderer_terminate(renderer);
    return false;
  }

  renderer->is_initialized = true;
  return true;
}

void ldk_ui_renderer_terminate(LDKUIRenderer* renderer)
{
  if (renderer->ebo != 0)
  {
    glDeleteBuffers(1, &renderer->ebo);
  }

  if (renderer->vbo != 0)
  {
    glDeleteBuffers(1, &renderer->vbo);
  }

  if (renderer->vao != 0)
  {
    glDeleteVertexArrays(1, &renderer->vao);
  }

  if (renderer->white_texture != 0)
  {
    glDeleteTextures(1, &renderer->white_texture);
  }

  if (renderer->program != 0)
  {
    glDeleteProgram(renderer->program);
  }

  if (renderer->vertex_shader != 0)
  {
    glDeleteShader(renderer->vertex_shader);
  }

  if (renderer->fragment_shader != 0)
  {
    glDeleteShader(renderer->fragment_shader);
  }

  memset(renderer, 0, sizeof(*renderer));
}

void ldk_ui_renderer_draw(
  LDKUIRenderer* renderer,
  LDKUIRenderData const* render_data,
  i32 framebuffer_width,
  i32 framebuffer_height
)
{
  if (!renderer->is_initialized)
  {
    return;
  }

  if (render_data == NULL)
  {
    return;
  }

  if (framebuffer_width <= 0 || framebuffer_height <= 0)
  {
    return;
  }

  if (render_data->vertex_count == 0 || render_data->index_count == 0 || render_data->command_count == 0)
  {
    return;
  }

  ldk_ui_renderer_ensure_vertex_capacity(renderer, render_data->vertex_count);
  ldk_ui_renderer_ensure_index_capacity(renderer, render_data->index_count);

  glUseProgram(renderer->program);
  glUniform2f(renderer->viewport_size_uniform, (float)framebuffer_width, (float)framebuffer_height);

  glBindVertexArray(renderer->vao);

  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferSubData(
    GL_ARRAY_BUFFER,
    0,
    (GLsizeiptr)(render_data->vertex_count * sizeof(LDKUIVertex)),
    render_data->vertices
  );

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);
  glBufferSubData(
    GL_ELEMENT_ARRAY_BUFFER,
    0,
    (GLsizeiptr)(render_data->index_count * sizeof(u32)),
    render_data->indices
  );

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  for (u32 i = 0; i < render_data->command_count; ++i)
  {
    LDKUIDrawCmd const* cmd = &render_data->commands[i];
    i32 scissor_x = (i32)cmd->clip_rect.x;
    i32 scissor_y = framebuffer_height - (i32)(cmd->clip_rect.y + cmd->clip_rect.h);
    i32 scissor_w = (i32)cmd->clip_rect.w;
    i32 scissor_h = (i32)cmd->clip_rect.h;
    u32 texture = cmd->texture != 0 ? (u32)cmd->texture : renderer->white_texture;
    void const* index_offset_ptr = (void*)(uintptr_t)(cmd->index_offset * sizeof(u32));

    if (scissor_w <= 0 || scissor_h <= 0 || cmd->index_count == 0)
    {
      continue;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glScissor(scissor_x, scissor_y, scissor_w, scissor_h);

    glDrawElements(
      GL_TRIANGLES,
      (GLsizei)cmd->index_count,
      GL_UNSIGNED_INT,
      index_offset_ptr
    );
  }

  glDisable(GL_SCISSOR_TEST);
  glBindVertexArray(0);
  glUseProgram(0);
}
