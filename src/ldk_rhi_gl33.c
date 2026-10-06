#include "ldk_rhi_gl33.h"
#include <module/ldk_renderer.h>
#include "ldk_gl.h"

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
//"  vec4 tex = texture(LDK_TEXTURE_1, uv_flipped);\n"
"  vec4 tex = texture(LDK_TEXTURE_1, v_uv);\n"
"  out_color = tex * v_color;\n"
"}\n";

static char const *LDK_RHI_GL33_TEXT_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view;\n"
    "  mat4 u_projection;\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_1\n"
    "{\n"
    "  mat4 u_world;\n"
    "  vec4 u_options;\n"
    "};\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main()\n"
    "{\n"
    "  mat4 world = u_world;\n"
    "  if (u_options.x > 0.5)\n"
    "  {\n"
    "    vec3 camera_right = normalize(vec3(\n"
    "        u_view[0][0], u_view[1][0], u_view[2][0]));\n"
    "    vec3 camera_up = normalize(vec3(\n"
    "        u_view[0][1], u_view[1][1], u_view[2][1]));\n"
    "    vec3 camera_back = normalize(vec3(\n"
    "        u_view[0][2], u_view[1][2], u_view[2][2]));\n"
    "    vec3 axis_x = camera_right * world[0].x +\n"
    "        camera_up * world[0].y;\n"
    "    vec3 axis_y = camera_right * world[1].x +\n"
    "        camera_up * world[1].y;\n"
    "    float z_scale = length(world[2].xyz);\n"
    "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n"
    "        vec4(camera_back * z_scale, 0.0), world[3]);\n"
    "  }\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "  gl_Position = u_projection * u_view * world *\n"
    "      vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_TEXT_PASS_FRAGMENT_SHADER =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "out vec4 out_color;\n"
    "uniform sampler2D LDK_TEXTURE_2;\n"
    "void main()\n"
    "{\n"
    "  out_color = texture(LDK_TEXTURE_2, v_uv) * v_color;\n"
    "}\n";

#define LDK_GL33_VEGETATION_GLSL                                            \
  "uniform sampler2D LDK_VEGETATION_TEXTURE;\n"                            \
  "vec2 ldk_vegetation_interaction(mat4 world, vec4 interaction)\n"      \
  "{\n"                                                                    \
  "  if (interaction.z <= 0.0 || interaction.w <= 0.0)\n"               \
  "    return vec2(0.0);\n"                                               \
  "  vec2 field_uv = (world[3].xz - interaction.xy) * interaction.z;\n" \
  "  if (any(lessThan(field_uv, vec2(0.0))) ||\n"                        \
  "      any(greaterThan(field_uv, vec2(1.0))))\n"                       \
  "    return vec2(0.0);\n"                                               \
  "  vec3 encoded = texture(LDK_VEGETATION_TEXTURE, field_uv).rgb;\n"   \
  "  vec2 bend = (encoded.rg - vec2(128.0 / 255.0)) *\n"                 \
  "      (255.0 / 127.0);\n"                                              \
  "  float age = encoded.b * 16.0;\n"                                       \
  "  float t = clamp(age / interaction.w, 0.0, 1.0);\n"                  \
  "  float recovery = 1.0 - t * t * (3.0 - 2.0 * t);\n"                   \
  "  return bend * recovery;\n"                                           \
  "}\n"                                                                    \
  "vec3 ldk_vegetation_position(mat4 world, vec3 local_position,\n"       \
  "    vec2 uv, vec4 tangent, vec4 vegetation, vec4 interaction,\n"       \
  "    float time, out vec3 bend, out float height)\n"                    \
  "{\n"                                                                    \
  "  vec4 world_position = world * vec4(local_position, 1.0);\n"            \
  "  height = clamp(uv.y, 0.0, 1.0);\n"                                 \
  "  float weight = height * height;\n"                                    \
  "  vec2 interaction_bend = ldk_vegetation_interaction(world, interaction);\n" \
  "  bend = vec3(0.0);\n"                                                  \
  "  if (weight > 0.0 && (vegetation.x > 0.0 || vegetation.y > 0.0 ||\n" \
  "      dot(interaction_bend, interaction_bend) > 0.0))\n"               \
  "  {\n"                                                                  \
  "    mat3 basis = mat3(world);\n"                                       \
  "    vec3 curve_direction = basis * tangent.xyz;\n"                     \
  "    float curve_length = length(curve_direction);\n"                      \
  "    if (curve_length <= 1e-6)\n"                                      \
  "      curve_direction = basis * vec3(0.0, 0.0, 1.0);\n"              \
  "    curve_direction /= max(length(curve_direction), 1e-6);\n"             \
  "    float blade_height = max(length(basis[1]), 1e-6);\n"               \
  "    vec2 wind_direction = vec2(cos(vegetation.w), sin(vegetation.w));\n" \
  "    float phase = time * vegetation.z * 6.28318530718 +\n"             \
  "        dot(world[3].xz, wind_direction) * 0.75;\n"                      \
  "    float sway = 0.65 * sin(phase) +\n"                               \
  "        0.35 * sin(phase * 1.73 + 1.2);\n"                              \
  "    vec3 wind = vec3(wind_direction.x, 0.0, wind_direction.y);\n"      \
  "    bend = curve_direction * vegetation.x +\n"                        \
  "        wind * (vegetation.y * sway) +\n"                               \
  "        vec3(interaction_bend.x, 0.0, interaction_bend.y);\n"           \
  "    world_position.xyz += bend * (blade_height * weight);\n"             \
  "  }\n"                                                                  \
  "  return world_position.xyz;\n"                                         \
  "}\n"                                                                    \
  "vec3 ldk_vegetation_normal(mat4 world, vec3 local_normal,\n"            \
  "    vec3 bend, float height, float bend_normal_weight)\n"                  \
  "{\n"                                                                    \
  "  mat3 basis = mat3(world);\n"                                         \
  "  float blade_height = max(length(basis[1]), 1e-6);\n"                 \
  "  vec3 world_normal = basis[0] * local_normal.x +\n"                    \
  "      basis[1] * (local_normal.y / (blade_height * blade_height)) +\n" \
  "      basis[2] * local_normal.z;\n"                                    \
  "  if (height > 0.0 && bend_normal_weight > 0.0 &&\n"                     \
  "      dot(bend, bend) > 0.0)\n"                                       \
  "  {\n"                                                                  \
  "    vec3 horizontal = local_normal.x * basis[2] -\n"                    \
  "        local_normal.z * basis[0];\n"                                  \
  "    world_normal += 2.0 * height * bend_normal_weight *\n"                 \
  "        cross(bend, horizontal);\n"                                        \
  "  }\n"                                                                  \
  "  return world_normal;\n"                                               \
  "}\n"

static char const *LDK_RHI_GL33_MESH_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in vec4 a_color;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "layout(location = 9) in vec4 i_instance_color;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view;\n"
    "  mat4 u_projection;\n"
    "  vec4 u_camera_position;\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_1\n"
    "{\n"
    "  mat4 u_world;\n"
    "  vec4 u_instance_color;\n"
    "  vec4 u_object_options;\n"
    "};\n"
    "out vec3 v_normal;\n"
    "out vec4 v_tangent;\n"
    "out vec3 v_world_position;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "out vec4 v_instance_color;\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  if (u_object_options.x > 0.5)\n"
    "  {\n"
    "    vec3 camera_right = normalize(vec3(\n"
    "        u_view[0][0], u_view[1][0], u_view[2][0]));\n"
    "    vec3 camera_up = normalize(vec3(\n"
    "        u_view[0][1], u_view[1][1], u_view[2][1]));\n"
    "    vec3 camera_back = normalize(vec3(\n"
    "        u_view[0][2], u_view[1][2], u_view[2][2]));\n"
    "    vec3 axis_x = camera_right * world[0].x +\n"
    "        camera_up * world[0].y;\n"
    "    vec3 axis_y = camera_right * world[1].x +\n"
    "        camera_up * world[1].y;\n"
    "    float z_scale = length(world[2].xyz);\n"
    "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n"
    "        vec4(camera_back * z_scale, 0.0), world[3]);\n"
    "  }\n"
    "  vec4 world_position = world * vec4(a_position, 1.0);\n"
    "  mat3 basis = mat3(world);\n"
    "  float basis_determinant = determinant(basis);\n"
    "  mat3 normal_basis = abs(basis_determinant) > 1e-8\n"
    "      ? transpose(inverse(basis)) : basis;\n"
    "  v_normal = normal_basis * a_normal;\n"
    "  v_tangent = vec4(0.0);\n"
    "  if (abs(a_tangent.w) > 0.5)\n"
    "  {\n"
    "    vec3 n = v_normal / max(length(v_normal), 1e-6);\n"
    "    vec3 tangent = basis * a_tangent.xyz;\n"
    "    tangent -= n * dot(n, tangent);\n"
    "    float tangent_length = length(tangent);\n"
    "    if (tangent_length > 1e-6)\n"
    "    {\n"
    "      float mirror_sign = basis_determinant < 0.0 ? -1.0 : 1.0;\n"
    "      float handedness = a_tangent.w < 0.0 ? -1.0 : 1.0;\n"
    "      v_tangent = vec4(tangent / tangent_length,\n"
    "          handedness * mirror_sign);\n"
    "    }\n"
    "  }\n"
    "  v_world_position = world_position.xyz;\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "#ifdef LDK_INSTANCED\n"
    "  v_instance_color = i_instance_color;\n"
    "#else\n"
    "  v_instance_color = u_instance_color;\n"
    "#endif\n"
    "  gl_Position = u_projection * u_view * world_position;\n"
    "}\n";

static char const *LDK_RHI_GL33_TERRAIN_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in vec4 a_color;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "layout(location = 9) in vec4 i_instance_color;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "  vec4 u_vegetation_interaction;\n"
    "  vec4 u_terrain_info;\n"
    "  vec4 u_terrain_range[8];\n"
    "  vec4 u_terrain_color[8];\n"
    "  vec4 u_terrain_atlas_rect[8];\n"
    "  vec4 u_terrain_uv_scale[8];\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view;\n"
    "  mat4 u_projection;\n"
    "  vec4 u_camera_position;\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_1\n"
    "{\n"
    "  mat4 u_world;\n"
    "  vec4 u_instance_color;\n"
    "  vec4 u_object_options;\n"
    "};\n"
    "out vec3 v_normal;\n"
    "out vec4 v_tangent;\n"
    "out vec3 v_world_position;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "out vec4 v_instance_color;\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  if (u_object_options.x > 0.5)\n"
    "  {\n"
    "    vec3 camera_right = normalize(vec3(\n"
    "        u_view[0][0], u_view[1][0], u_view[2][0]));\n"
    "    vec3 camera_up = normalize(vec3(\n"
    "        u_view[0][1], u_view[1][1], u_view[2][1]));\n"
    "    vec3 camera_back = normalize(vec3(\n"
    "        u_view[0][2], u_view[1][2], u_view[2][2]));\n"
    "    vec3 axis_x = camera_right * world[0].x +\n"
    "        camera_up * world[0].y;\n"
    "    vec3 axis_y = camera_right * world[1].x +\n"
    "        camera_up * world[1].y;\n"
    "    float z_scale = length(world[2].xyz);\n"
    "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n"
    "        vec4(camera_back * z_scale, 0.0), world[3]);\n"
    "  }\n"
    "  vec4 world_position = world * vec4(a_position, 1.0);\n"
    "  mat3 basis = mat3(world);\n"
    "  float basis_determinant = determinant(basis);\n"
    "  mat3 normal_basis = abs(basis_determinant) > 1e-8\n"
    "      ? transpose(inverse(basis)) : basis;\n"
    "  v_normal = normal_basis * a_normal;\n"
    "  v_tangent = vec4(0.0);\n"
    "  if (abs(a_tangent.w) > 0.5)\n"
    "  {\n"
    "    vec3 n = v_normal / max(length(v_normal), 1e-6);\n"
    "    vec3 tangent = basis * a_tangent.xyz;\n"
    "    tangent -= n * dot(n, tangent);\n"
    "    float tangent_length = length(tangent);\n"
    "    if (tangent_length > 1e-6)\n"
    "    {\n"
    "      float mirror_sign = basis_determinant < 0.0 ? -1.0 : 1.0;\n"
    "      float handedness = a_tangent.w < 0.0 ? -1.0 : 1.0;\n"
    "      v_tangent = vec4(tangent / tangent_length,\n"
    "          handedness * mirror_sign);\n"
    "    }\n"
    "  }\n"
    "  v_world_position = world_position.xyz;\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "#ifdef LDK_INSTANCED\n"
    "  v_instance_color = i_instance_color;\n"
    "#else\n"
    "  v_instance_color = u_instance_color;\n"
    "#endif\n"
    "  gl_Position = u_projection * u_view * world_position;\n"
    "}\n";

static char const *LDK_RHI_GL33_VEGETATION_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "#define LDK_VEGETATION_TEXTURE LDK_TEXTURE_8\n"
    LDK_GL33_VEGETATION_GLSL
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in vec4 a_color;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "layout(location = 9) in vec4 i_instance_color;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "  vec4 u_vegetation_interaction;\n"
    "};\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view;\n"
    "  mat4 u_projection;\n"
    "  vec4 u_camera_position;\n"
    "};\n"
    "#ifndef LDK_INSTANCED\n"
    "layout(std140) uniform LDK_UBO_1\n"
    "{\n"
    "  mat4 u_world;\n"
    "  vec4 u_instance_color;\n"
    "};\n"
    "#endif\n"
    "out vec3 v_normal;\n"
    "out vec3 v_world_position;\n"
    "out vec4 v_color;\n"
    "out vec4 v_instance_color;\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  vec3 vegetation_bend;\n"
    "  float vegetation_height;\n"
    "  vec3 position = ldk_vegetation_position(world, a_position, a_uv,\n"
    "      a_tangent, u_vegetation, u_vegetation_interaction,\n"
    "      u_camera_position.w, vegetation_bend, vegetation_height);\n"
    "  v_normal = ldk_vegetation_normal(world, a_normal, vegetation_bend,\n"
    "      vegetation_height, abs(a_tangent.w));\n"
    "  v_world_position = position;\n"
    "  v_color = a_color;\n"
    "#ifdef LDK_INSTANCED\n"
    "  v_instance_color = i_instance_color;\n"
    "#else\n"
    "  v_instance_color = u_instance_color;\n"
    "#endif\n"
    "  gl_Position = u_projection * u_view * vec4(position, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_SHADOW_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_light_view_projection;\n"
    "  vec4 u_animation;\n"
    "};\n"
    "#ifndef LDK_INSTANCED\n"
    "layout(std140) uniform LDK_UBO_1 { mat4 u_world; };\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_cutout;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform;\n"
    "};\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  vec3 position = (world * vec4(a_position, 1.0)).xyz;\n"
    "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_SHADOW_PASS_VEGETATION_VERTEX_SHADER =
    "#version 330 core\n"
    "#define LDK_VEGETATION_TEXTURE LDK_TEXTURE_4\n"
    LDK_GL33_VEGETATION_GLSL
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_light_view_projection;\n"
    "  vec4 u_animation;\n"
    "};\n"
    "#ifndef LDK_INSTANCED\n"
    "layout(std140) uniform LDK_UBO_1 { mat4 u_world; };\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_cutout;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform;\n"
    "  vec4 u_vegetation_interaction;\n"
    "};\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  vec3 vegetation_bend;\n"
    "  float vegetation_height;\n"
    "  vec3 position = ldk_vegetation_position(world, a_position, a_uv,\n"
    "      a_tangent, u_vegetation, u_vegetation_interaction,\n"
    "      u_animation.x, vegetation_bend, vegetation_height);\n"
    "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_SHADOW_PASS_FRAGMENT_SHADER =
    "#version 330 core\n"
    "void main() {}\n";

static char const *LDK_RHI_GL33_SHADOW_PASS_CUTOUT_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 8) in vec4 a_tangent;\n"
    "#ifdef LDK_INSTANCED\n"
    "layout(location = 4) in vec4 i_world_0;\n"
    "layout(location = 5) in vec4 i_world_1;\n"
    "layout(location = 6) in vec4 i_world_2;\n"
    "layout(location = 7) in vec4 i_world_3;\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_light_view_projection;\n"
    "  vec4 u_animation;\n"
    "};\n"
    "#ifndef LDK_INSTANCED\n"
    "layout(std140) uniform LDK_UBO_1 { mat4 u_world; };\n"
    "#endif\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_cutout;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform;\n"
    "};\n"
    "out vec2 v_uv;\n"
    "void main()\n"
    "{\n"
    "#ifdef LDK_INSTANCED\n"
    "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n"
    "#else\n"
    "  mat4 world = u_world;\n"
    "#endif\n"
    "  v_uv = a_uv * u_uv_transform.xy + u_uv_transform.zw;\n"
    "  vec3 position = (world * vec4(a_position, 1.0)).xyz;\n"
    "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_SHADOW_PASS_CUTOUT_FRAGMENT_SHADER =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_cutout;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform;\n"
    "};\n"
    "uniform sampler2D LDK_TEXTURE_3;\n"
    "void main()\n"
    "{\n"
    "  float alpha = texture(LDK_TEXTURE_3, v_uv).a * u_cutout.x;\n"
    "  if (alpha < u_cutout.y)\n"
    "    discard;\n"
    "}\n";

LDK_STATIC_ASSERT(LDK_RENDERER_MAX_LIGHTS_PER_VIEW == 16, gl33_light_count);

#define LDK_GL33_LIGHTING_GLSL                                                 \
  "in vec3 v_world_position;\n"                                                \
  "layout(std140) uniform LDK_UBO_0\n"                                         \
  "{\n"                                                                        \
  "  mat4 u_view;\n"                                                          \
  "  mat4 u_projection;\n"                                                    \
  "  vec4 u_camera_position;\n"                                                \
  "};\n"                                                                       \
  "struct LDKLight\n"                                                          \
  "{\n"                                                                        \
  "  vec4 position_type;\n"                                                    \
  "  vec4 direction_range;\n"                                                  \
  "  vec4 color_intensity;\n"                                                  \
  "  vec4 cone;\n"                                                             \
  "};\n"                                                                       \
  "layout(std140) uniform LDK_UBO_4\n"                                         \
  "{\n"                                                                        \
  "  ivec4 u_light_count;\n"                                                   \
  "  vec4 u_ambient;\n"                                                        \
  "  LDKLight u_lights[16];\n"                                                 \
  "  mat4 u_shadow_view_projection;\n"                                         \
  "  vec4 u_shadow_params;\n"                                                  \
  "};\n"                                                                       \
  "uniform sampler2D LDK_TEXTURE_5;\n"                                         \
  "float ldk_shadow_visibility(vec3 n, vec3 to_light)\n"                       \
  "{\n"                                                                        \
  "  vec4 clip = u_shadow_view_projection * vec4(v_world_position, 1.0);\n"    \
  "  vec3 p = clip.xyz / clip.w * 0.5 + 0.5;\n"                               \
  "  if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0))))\n"     \
  "    return 1.0;\n"                                                          \
  "  float bias = max(u_shadow_params.x,\n"                                    \
  "      u_shadow_params.y * (1.0 - max(dot(n, to_light), 0.0)));\n"           \
  "  float receiver_depth = p.z - bias;\n"                                     \
  "  vec2 texel_size = 1.0 / vec2(textureSize(LDK_TEXTURE_5, 0));\n"           \
  "  float visibility = 0.0;\n"                                                \
  "  for (int y = -1; y <= 1; ++y)\n"                                         \
  "  {\n"                                                                      \
  "    for (int x = -1; x <= 1; ++x)\n"                                       \
  "    {\n"                                                                    \
  "      vec2 offset = vec2(float(x), float(y)) * texel_size;\n"                \
  "      float depth = texture(LDK_TEXTURE_5, p.xy + offset).r;\n"              \
  "      visibility += receiver_depth <= depth ? 1.0 : 0.0;\n"                 \
  "    }\n"                                                                    \
  "  }\n"                                                                      \
  "  return visibility / 9.0;\n"                                               \
  "}\n"                                                                        \
  "void ldk_lighting(vec3 normal, float specular_strength,\n"                    \
  "    float shininess, out vec3 diffuse, out vec3 specular)\n"                   \
  "{\n"                                                                        \
  "  vec3 n = normal / max(length(normal), 1e-6);\n"                           \
  "  vec3 view_delta = u_camera_position.xyz - v_world_position;\n"             \
  "  vec3 view_direction = view_delta / max(length(view_delta), 1e-6);\n"        \
  "  diffuse = u_ambient.rgb * u_ambient.a;\n"                                \
  "  specular = vec3(0.0);\n"                                                  \
  "  for (int i = 0; i < min(u_light_count.x, 16); ++i)\n"                     \
  "  {\n"                                                                      \
  "    LDKLight light = u_lights[i];\n"                                        \
  "    int type = int(light.position_type.w);\n"                               \
  "    vec3 to_light;\n"                                                       \
  "    float attenuation = 1.0;\n"                                             \
  "    if (type == 2)\n"                                                       \
  "    {\n"                                                                    \
  "      to_light = -light.direction_range.xyz;\n"                             \
  "    }\n"                                                                    \
  "    else\n"                                                                 \
  "    {\n"                                                                    \
  "      vec3 delta = light.position_type.xyz - v_world_position;\n"           \
  "      float distance_to_light = length(delta);\n"                           \
  "      to_light = delta / max(distance_to_light, 1e-6);\n"                   \
  "      float falloff = max(1.0 - distance_to_light / "                       \
  "light.direction_range.w, 0.0);\n"                                           \
  "      attenuation = falloff * falloff;\n"                                   \
  "      if (type == 1)\n"                                                     \
  "      {\n"                                                                  \
  "        float cosine = dot(-to_light, light.direction_range.xyz);\n"        \
  "        float width = light.cone.x - light.cone.y;\n"                       \
  "        attenuation *= width > 1e-6\n"                                      \
  "            ? smoothstep(light.cone.y, light.cone.x, cosine)\n"             \
  "            : step(light.cone.y, cosine);\n"                                \
  "      }\n"                                                                  \
  "    }\n"                                                                    \
  "    if (i == u_light_count.y)\n"                                            \
  "      attenuation *= ldk_shadow_visibility(n, to_light);\n"                 \
  "    float ndotl = max(dot(n, to_light), 0.0);\n"                           \
  "    vec3 energy = light.color_intensity.rgb * light.color_intensity.a\n"    \
  "        * attenuation;\n"                                                    \
  "    diffuse += energy * ndotl;\n"                                           \
  "    if (ndotl > 0.0 && specular_strength > 0.0)\n"                         \
  "    {\n"                                                                    \
  "      vec3 reflected = reflect(-to_light, n);\n"                            \
  "      float highlight = pow(max(dot(reflected, view_direction), 0.0),\n"     \
  "          max(shininess, 1.0));\n"                                           \
  "      specular += energy * highlight * specular_strength;\n"                    \
  "    }\n"                                                                    \
  "  }\n"                                                                      \
  "}\n"

#define LDK_GL33_SURFACE_GLSL                                                  \
  "in vec3 v_normal;\n"                                                        \
  "in vec4 v_tangent;\n"                                                       \
  "in vec2 v_uv;\n"                                                            \
  "uniform sampler2D LDK_TEXTURE_6;\n"                                         \
  "uniform sampler2D LDK_TEXTURE_7;\n"                                         \
  "vec3 ldk_surface_normal(vec2 uv)\n"                                                \
  "{\n"                                                                        \
  "  vec3 n = v_normal / max(length(v_normal), 1e-6);\n"                       \
  "  if (abs(v_tangent.w) < 0.5)\n"                                            \
  "    return n;\n"                                                            \
  "  vec3 t = v_tangent.xyz - n * dot(n, v_tangent.xyz);\n"                    \
  "  float tangent_length = length(t);\n"                                      \
  "  if (tangent_length <= 1e-6)\n"                                            \
  "    return n;\n"                                                            \
  "  t /= tangent_length;\n"                                                   \
  "  vec3 b = cross(n, t) * v_tangent.w;\n"                                    \
  "  vec3 map_normal = texture(LDK_TEXTURE_6, uv).xyz * 2.0 - 1.0;\n"        \
  "  return normalize(mat3(t, b, n) * map_normal);\n"                          \
  "}\n"                                                                        \
  "float ldk_surface_specular(float strength, vec2 uv)\n"                               \
  "{\n"                                                                        \
  "  return strength * texture(LDK_TEXTURE_7, uv).r;\n"                      \
  "}\n"

static char const *LDK_RHI_GL33_MESH_PASS_FRAGMENT_SHADER =
    "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL
    "in vec4 v_color;\n"
    "in vec4 v_instance_color;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  vec3 diffuse;\n"
    "  vec3 specular;\n"
    "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n"
    "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n"
    "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, "
    "specular);\n"
    "  vec4 color = v_color * u_material_color * v_instance_color;\n"
    "  vec3 emission = color.rgb * u_surface.z;\n"
    "  out_color = vec4(color.rgb * diffuse + specular + emission, color.a);\n"
    "}\n";

static char const *LDK_RHI_GL33_TERRAIN_PASS_FRAGMENT_SHADER =
    "#version 330 core\n" LDK_GL33_LIGHTING_GLSL
    "in vec3 v_normal;\n"
    "in vec2 v_uv;\n"
    "in vec4 v_instance_color;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "  vec4 u_vegetation_interaction;\n"
    "  vec4 u_terrain_info;\n"
    "  vec4 u_terrain_range[8];\n"
    "  vec4 u_terrain_color[8];\n"
    "  vec4 u_terrain_atlas_rect[8];\n"
    "  vec4 u_terrain_uv_scale[8];\n"
    "};\n"
    "uniform sampler2D LDK_TEXTURE_3;\n"
    "out vec4 out_color;\n"
    "vec4 ldk_terrain_surface(int index)\n"
    "{\n"
    "  vec4 rect = u_terrain_atlas_rect[index];\n"
    "  vec2 scale = u_terrain_uv_scale[index].xy;\n"
    "  vec2 tile_uv = fract((v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * scale);\n"
    "  vec2 atlas_uv = mix(rect.xy, rect.zw, tile_uv);\n"
    "  vec4 atlas = texture(LDK_TEXTURE_3, atlas_uv);\n"
    "  float weight = clamp(u_terrain_range[index].z, 0.0, 1.0);\n"
    "  return u_terrain_color[index] * mix(vec4(1.0), atlas, weight);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "  int count = clamp(int(u_terrain_info.x + 0.5), 1, 8);\n"
    "  vec4 color = ldk_terrain_surface(0);\n"
    "  float height = v_world_position.y;\n"
    "  for (int i = 1; i < 8; ++i)\n"
    "  {\n"
    "    if (i >= count) break;\n"
    "    float threshold = u_terrain_range[i].x;\n"
    "    float width = u_terrain_range[i].y;\n"
    "    float t = width > 0.000001\n"
    "        ? smoothstep(threshold - width, threshold, height)\n"
    "        : step(threshold, height);\n"
    "    color = mix(color, ldk_terrain_surface(i), t);\n"
    "  }\n"
    "  color *= u_material_color * v_instance_color;\n"
    "  vec3 diffuse;\n"
    "  vec3 specular;\n"
    "  vec3 normal = v_normal / max(length(v_normal), 1e-6);\n"
    "  ldk_lighting(normal, u_surface.x, u_surface.y, diffuse, specular);\n"
    "  vec3 emission = color.rgb * u_surface.z;\n"
    "  out_color = vec4(color.rgb * diffuse + specular + emission, color.a);\n"
    "}\n";

static char const *LDK_RHI_GL33_VEGETATION_PASS_FRAGMENT_SHADER =
    "#version 330 core\n" LDK_GL33_LIGHTING_GLSL
    "in vec3 v_normal;\n"
    "in vec4 v_color;\n"
    "in vec4 v_instance_color;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "  vec4 u_vegetation_interaction;\n"
    "};\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  vec3 normal = v_normal / max(length(v_normal), 1e-6);\n"
    "  if (!gl_FrontFacing)\n"
    "    normal = -normal;\n"
    "  vec3 diffuse;\n"
    "  vec3 specular;\n"
    "  ldk_lighting(normal, u_surface.x, u_surface.y, diffuse, specular);\n"
    "  vec4 color = v_color * u_material_color * v_instance_color;\n"
    "  vec3 emission = color.rgb * u_surface.z;\n"
    "  out_color = vec4(color.rgb * diffuse + specular + emission, color.a);\n"
    "}\n";

static char const* LDK_RHI_GL33_MESH_PASS_UNLIT_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec4 v_color;\n"
"in vec4 v_instance_color;\n"
"layout(std140) uniform LDK_UBO_2\n"
"{\n"
"  vec4 u_material_color;\n"
"  vec4 u_surface;\n"
"  vec4 u_vegetation;\n"
"  vec4 u_uv_transform[8];\n"
"};\n"
"out vec4 out_color;\n"
"void main()\n"
"{\n"
"  out_color = v_color * u_material_color * v_instance_color;\n"
"}\n";

static char const *LDK_RHI_GL33_MESH_PASS_SOLID_UNLIT_FRAGMENT_SHADER =
    "#version 330 core\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  out_color = u_material_color;\n"
    "}\n";

static char const *LDK_RHI_GL33_MESH_PASS_TEXTURED_FRAGMENT_SHADER =
    "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL
    "in vec4 v_instance_color;\n"

    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "uniform sampler2D LDK_TEXTURE_3;\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  vec3 diffuse;\n"
    "  vec3 specular;\n"
    "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n"
    "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n"
    "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, "
    "specular);\n"
    "  vec4 color = texture(LDK_TEXTURE_3, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * u_material_color *\n"
    "      v_instance_color;\n"
    "  vec3 emission = color.rgb * u_surface.z;\n"
    "  out_color = vec4(color.rgb * diffuse + specular + emission, color.a);\n"
    "}\n";

static char const* LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"in vec4 v_instance_color;\n"
"layout(std140) uniform LDK_UBO_2\n"
"{\n"
"  vec4 u_material_color;\n"
"  vec4 u_surface;\n"
"  vec4 u_vegetation;\n"
"  vec4 u_uv_transform[8];\n"
"};\n"
"uniform sampler2D LDK_TEXTURE_3;\n"
"out vec4 out_color;\n"
"void main()\n"
"{\n"
"  out_color = texture(LDK_TEXTURE_3, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * u_material_color *\n"
"      v_instance_color;\n"
"}\n";

static char const *LDK_RHI_GL33_MESH_PASS_TEXTURED_CUTOUT_FRAGMENT_SHADER =
    "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL
    "in vec4 v_instance_color;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "uniform sampler2D LDK_TEXTURE_3;\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  vec4 color = texture(LDK_TEXTURE_3, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * u_material_color *\n"
    "      v_instance_color;\n"
    "  if (color.a < u_surface.w)\n"
    "    discard;\n"
    "  vec3 diffuse;\n"
    "  vec3 specular;\n"
    "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n"
    "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n"
    "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, "
    "specular);\n"
    "  vec3 emission = color.rgb * u_surface.z;\n"
    "  out_color = vec4(color.rgb * diffuse + specular + emission, color.a);\n"
    "}\n";

static char const *LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_CUTOUT_FRAGMENT_SHADER =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "in vec4 v_instance_color;\n"
    "layout(std140) uniform LDK_UBO_2\n"
    "{\n"
    "  vec4 u_material_color;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_vegetation;\n"
    "  vec4 u_uv_transform[8];\n"
    "};\n"
    "uniform sampler2D LDK_TEXTURE_3;\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "  vec4 color = texture(LDK_TEXTURE_3, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * u_material_color *\n"
    "      v_instance_color;\n"
    "  if (color.a < u_surface.w)\n"
    "    discard;\n"
    "  out_color = color;\n"
    "}\n";

static char const* LDK_RHI_GL33_GRID_PASS_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec3 a_position;\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  mat4 u_view;\n"
"  mat4 u_projection;\n"
"  vec4 u_grid_center_extent;\n"
"  vec4 u_grid_settings;\n"
"};\n"
"out vec2 v_world_xz;\n"
"void main()\n"
"{\n"
"  v_world_xz = u_grid_center_extent.xz +\n"
"      a_position.xz * u_grid_center_extent.w;\n"
"  vec3 world_position = vec3(v_world_xz.x,\n"
"      u_grid_center_extent.y, v_world_xz.y);\n"
"  gl_Position = u_projection * u_view * vec4(world_position, 1.0);\n"
"}\n";

static char const* LDK_RHI_GL33_GRID_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  mat4 u_view;\n"
"  mat4 u_projection;\n"
"  vec4 u_grid_center_extent;\n"
"  vec4 u_grid_settings;\n"
"};\n"
"in vec2 v_world_xz;\n"
"out vec4 out_color;\n"
"float grid_coverage(vec2 coordinate)\n"
"{\n"
"  vec2 derivative = max(fwidth(coordinate), vec2(0.00001));\n"
"  vec2 distance_to_line =\n"
"      abs(fract(coordinate - 0.5) - 0.5) / derivative;\n"
"  float nearest_line = min(distance_to_line.x, distance_to_line.y);\n"
"  return 1.0 - smoothstep(0.0, 1.0, nearest_line);\n"
"}\n"
"float axis_coverage(float coordinate)\n"
"{\n"
"  float derivative = max(fwidth(coordinate), 0.00001);\n"
"  return 1.0 - smoothstep(0.0, 1.5, abs(coordinate) / derivative);\n"
"}\n"
"void main()\n"
"{\n"
"  float spacing = max(u_grid_settings.x, 0.0001);\n"
"  vec2 grid_coordinate = v_world_xz / spacing;\n"
"  vec2 grid_derivative = max(fwidth(grid_coordinate), vec2(0.00001));\n"
"  float pixels_per_cell = 1.0 /\n"
"      max(grid_derivative.x, grid_derivative.y);\n"
"  float detail_fade = smoothstep(1.0, 3.0, pixels_per_cell);\n"
"  float grid_alpha =\n"
"      grid_coverage(grid_coordinate) * detail_fade * 0.38;\n"
"  float x_axis_alpha = axis_coverage(v_world_xz.y) * 0.9;\n"
"  float z_axis_alpha = axis_coverage(v_world_xz.x) * 0.9;\n"
"  float distance_to_center =\n"
"      length(v_world_xz - u_grid_center_extent.xz);\n"
"  float extent = max(u_grid_center_extent.w, 0.0001);\n"
"  float distance_fade = 1.0 - smoothstep(\n"
"      extent * 0.65, extent, distance_to_center);\n"
"  float alpha = max(grid_alpha, max(x_axis_alpha, z_axis_alpha)) *\n"
"      distance_fade;\n"
"  if (alpha <= 0.001)\n"
"  {\n"
"    discard;\n"
"  }\n"
"  vec3 color = vec3(0.5);\n"
"  color = mix(color, vec3(1.0, 0.25, 0.25), x_axis_alpha);\n"
"  color = mix(color, vec3(0.25, 0.38, 1.0), z_axis_alpha);\n"
"  out_color = vec4(color, alpha);\n"
"}\n";

static char const* LDK_RHI_GL33_PRESENT_PASS_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec2 a_position;\n"
"layout(location = 1) in vec2 a_uv;\n"
"out vec2 v_uv;\n"
"void main()\n"
"{\n"
"  v_uv = a_uv;\n"
"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
"}\n";

static char const* LDK_RHI_GL33_PRESENT_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"out vec4 out_color;\n"
"uniform sampler2D LDK_TEXTURE_0;\n"
"void main()\n"
"{\n"
"  out_color = texture(LDK_TEXTURE_0, v_uv);\n"
"}\n";

static char const* LDK_RHI_GL33_POST_PROCESS_PASS_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec2 a_position;\n"
"layout(location = 1) in vec2 a_uv;\n"
"out vec2 v_uv;\n"
"void main()\n"
"{\n"
"  v_uv = a_uv;\n"
"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
"}\n";

static char const* LDK_RHI_GL33_POST_PROCESS_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"out vec4 out_color;\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  vec4 u_tonemapping;\n"
"  vec4 u_color_adjustments;\n"
"  vec4 u_blur_focus;\n"
"  vec4 u_blur_options;\n"
"  vec4 u_vignette;\n"
"  vec4 u_lens;\n"
"  vec4 u_heat;\n"
"  vec4 u_stylization_options;\n"
"  vec4 u_retro;\n"
"  vec4 u_drunk_primary;\n"
"  vec4 u_drunk_secondary;\n"
"  vec4 u_animation;\n"
"};\n"
"uniform sampler2D LDK_TEXTURE_1;\n"
"uniform sampler2D LDK_TEXTURE_2;\n"
"const float LDK_RETRO_PATTERN[16] = float[16](\n"
"    12.0, 5.0, 6.0, 13.0,\n"
"    4.0, 0.0, 1.0, 7.0,\n"
"    11.0, 3.0, 2.0, 8.0,\n"
"    15.0, 10.0, 9.0, 14.0);\n"
"vec3 ldk_aces_tonemap(vec3 color)\n"
"{\n"
"  const float a = 2.51;\n"
"  const float b = 0.03;\n"
"  const float c = 2.43;\n"
"  const float d = 0.59;\n"
"  const float e = 0.14;\n"
"  return clamp((color * (a * color + b)) /\n"
"      (color * (c * color + d) + e), 0.0, 1.0);\n"
"}\n"
"vec2 ldk_distort_uv(vec2 uv)\n"
"{\n"
"  if (u_lens.y <= 0.5)\n"
"    return uv;\n"
"  float aspect = max(u_blur_options.z, 1e-6);\n"
"  vec2 delta = uv - vec2(0.5);\n"
"  vec2 radial = vec2(delta.x * aspect, delta.y);\n"
"  float radius_squared = dot(radial, radial);\n"
"  radial *= 1.0 + u_lens.x * radius_squared;\n"
"  return vec2(radial.x / aspect, radial.y) + vec2(0.5);\n"
"}\n"
"vec2 ldk_heat_uv(vec2 uv, vec2 texture_size)\n"
"{\n"
"  if (u_stylization_options.x <= 0.5 || u_heat.y <= 0.0)\n"
"    return uv;\n"
"  float scale = max(u_heat.z, 0.01);\n"
"  float time = u_animation.x * max(u_heat.w, 0.0);\n"
"  float wave_x = sin(uv.y * (90.0 / scale) + time * 4.0) +\n"
"      sin(uv.y * (157.0 / scale) - time * 2.3) * 0.5;\n"
"  float wave_y = sin((uv.x * 71.0 + uv.y * 31.0) / scale +\n"
"      time * 1.9) * 0.25;\n"
"  vec2 texel_size = 1.0 / max(texture_size, vec2(1.0));\n"
"  return uv + vec2(wave_x, wave_y) * texel_size * u_heat.y;\n"
"}\n"
"vec2 ldk_retro_uv(vec2 uv, vec2 texture_size)\n"
"{\n"
"  if (u_stylization_options.w <= 0.5)\n"
"    return uv;\n"
"  float pixel_size = max(floor(u_retro.x + 0.5), 1.0);\n"
"  vec2 size = max(texture_size, vec2(1.0));\n"
"  vec2 pixel = floor(uv * size / pixel_size) * pixel_size +\n"
"      vec2(pixel_size * 0.5);\n"
"  vec2 min_uv = vec2(0.5) / size;\n"
"  vec2 max_uv = (size - vec2(0.5)) / size;\n"
"  return clamp(pixel / size, min_uv, max_uv);\n"
"}\n"
"vec4 ldk_sample_color(sampler2D source, vec2 uv)\n"
"{\n"
"  vec4 center = texture(source, uv);\n"
"  if (u_lens.w <= 0.5 || u_lens.z <= 0.0)\n"
"    return center;\n"
"  float aspect = max(u_blur_options.z, 1e-6);\n"
"  vec2 delta = uv - vec2(0.5);\n"
"  vec2 radial = vec2(delta.x * aspect, delta.y);\n"
"  float radius = length(radial);\n"
"  if (radius <= 1e-6)\n"
"    return center;\n"
"  vec2 direction = radial / radius;\n"
"  direction.x /= aspect;\n"
"  vec2 offset = direction * (u_lens.z * 0.02 * radius);\n"
"  float red = texture(source, uv + offset).r;\n"
"  float blue = texture(source, uv - offset).b;\n"
"  return vec4(red, center.g, blue, center.a);\n"
"}\n"
"vec4 ldk_sample_post_source(sampler2D source, vec2 uv)\n"
"{\n"
"  vec2 texture_size = vec2(textureSize(source, 0));\n"
"  vec2 base_uv = ldk_retro_uv(uv, texture_size);\n"
"  vec4 base = ldk_sample_color(source, base_uv);\n"
"  if (u_stylization_options.x <= 0.5 || u_heat.x <= 0.0 ||\n"
"      u_heat.y <= 0.0)\n"
"    return base;\n"
"  vec2 heat_uv = ldk_heat_uv(uv, texture_size);\n"
"  heat_uv = ldk_retro_uv(heat_uv, texture_size);\n"
"  vec4 distorted = ldk_sample_color(source, heat_uv);\n"
"  return mix(base, distorted, clamp(u_heat.x, 0.0, 1.0));\n"
"}\n"
"float ldk_drunk_hash(float value)\n"
"{\n"
"  return fract(sin(value * 91.3458 + 17.123) * 47453.5453);\n"
"}\n"
"vec2 ldk_drunk_target(float index)\n"
"{\n"
"  vec2 value = vec2(\n"
"      ldk_drunk_hash(index * 1.371 + 11.0),\n"
"      ldk_drunk_hash(index * 2.173 + 37.0));\n"
"  return value * 2.0 - vec2(1.0);\n"
"}\n"
"vec2 ldk_drunk_path(float time, out vec2 direction, out float motion)\n"
"{\n"
"  float segment = floor(time);\n"
"  float phase = fract(time);\n"
"  vec2 from = ldk_drunk_target(segment);\n"
"  vec2 to = ldk_drunk_target(segment + 1.0);\n"
"  vec2 delta = to - from;\n"
"  if (dot(delta, delta) < 0.16)\n"
"  {\n"
"    to = -to;\n"
"    delta = to - from;\n"
"  }\n"
"  float eased = phase * phase * (3.0 - 2.0 * phase);\n"
"  direction = delta;\n"
"  motion = 4.0 * phase * (1.0 - phase);\n"
"  return mix(from, to, eased);\n"
"}\n"
"vec2 ldk_drunk_uv(vec2 uv)\n"
"{\n"
"  if (u_drunk_primary.w <= 0.5 || u_drunk_primary.x <= 0.0 ||\n"
"      u_drunk_primary.y <= 0.0 || u_drunk_primary.z <= 0.0)\n"
"    return uv;\n"
"  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n"
"  float movement = clamp(u_drunk_primary.z, 0.0, 3.0) * strength;\n"
"  float time = u_animation.x * max(u_drunk_primary.y, 0.0) * 0.75;\n"
"  vec2 path_direction;\n"
"  float path_motion;\n"
"  vec2 path = ldk_drunk_path(time, path_direction, path_motion);\n"
"  float aspect = max(u_blur_options.z, 1e-6);\n"
"  vec2 drift = path * vec2(0.028 / aspect, 0.028) * movement;\n"
"  float roll = clamp(path_direction.x, -1.0, 1.0) *\n"
"      0.008 * path_motion * movement;\n"
"  vec2 delta = uv - vec2(0.5);\n"
"  float zoom = 1.0 + 0.12 * movement;\n"
"  delta /= zoom;\n"
"  float s = sin(roll);\n"
"  float c = cos(roll);\n"
"  delta = mat2(c, -s, s, c) * delta;\n"
"  return vec2(0.5) + delta + drift;\n"
"}\n"
"vec2 ldk_drunk_offset(vec2 uv)\n"
"{\n"
"  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n"
"  float ghosting = clamp(u_drunk_secondary.x, 0.0, 1.0) * strength;\n"
"  float time = u_animation.x * max(u_drunk_primary.y, 0.0) * 0.85;\n"
"  vec2 direction = vec2(\n"
"      sin(time * 0.91 + uv.y * 3.0),\n"
"      cos(time * 0.73 + uv.x * 2.0 + 0.4));\n"
"  float len = max(length(direction), 1e-5);\n"
"  direction /= len;\n"
"  return direction * (0.004 + 0.018 * ghosting);\n"
"}\n"
"vec4 ldk_sample_drunk_source(sampler2D source, vec2 uv)\n"
"{\n"
"  if (u_drunk_primary.w <= 0.5 || u_drunk_primary.x <= 0.0)\n"
"    return ldk_sample_post_source(source, uv);\n"
"  vec2 drunk_uv = ldk_drunk_uv(uv);\n"
"  vec4 color = ldk_sample_post_source(source, drunk_uv);\n"
"  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n"
"  float ghosting = clamp(u_drunk_secondary.x, 0.0, 1.0) * strength;\n"
"  vec2 offset = ldk_drunk_offset(drunk_uv);\n"
"  if (ghosting > 0.0)\n"
"  {\n"
"    vec4 ghost_a = ldk_sample_post_source(source, drunk_uv + offset);\n"
"    vec4 ghost_b = ldk_sample_post_source(source, drunk_uv - offset);\n"
"    float ghost_weight = ghosting * 0.45;\n"
"    color = (color + (ghost_a + ghost_b) * ghost_weight) /\n"
"        (1.0 + ghost_weight * 2.0);\n"
"  }\n"
"  float chromatic = clamp(u_drunk_secondary.y, 0.0, 1.0) * strength;\n"
"  if (chromatic > 0.0)\n"
"  {\n"
"    float aspect = max(u_blur_options.z, 1e-6);\n"
"    vec2 radial = drunk_uv - vec2(0.5);\n"
"    radial.x *= aspect;\n"
"    float radius = length(radial);\n"
"    vec2 ca_offset = offset * (0.35 + radius * 0.9) * chromatic;\n"
"    float red = ldk_sample_post_source(source, drunk_uv + ca_offset).r;\n"
"    float blue = ldk_sample_post_source(source, drunk_uv - ca_offset).b;\n"
"    color.rgb = mix(color.rgb, vec3(red, color.g, blue), chromatic);\n"
"  }\n"
"  return color;\n"
"}\n"
"float ldk_blur_mask(vec2 uv)\n"

"{\n"
"  vec2 delta = uv - u_blur_focus.xy;\n"
"  float aspect = max(u_blur_options.z, 1e-6);\n"
"  delta.x *= aspect;\n"
"  float half_diagonal = 0.5 * length(vec2(aspect, 1.0));\n"
"  float distance_to_focus = length(delta) / max(half_diagonal, 1e-6);\n"
"  float size = clamp(u_blur_focus.z, 0.0, 1.0);\n"
"  float feather = max(clamp(u_blur_focus.w, 0.0, 1.0), 1e-5);\n"
"  float mask = smoothstep(size, size + feather, distance_to_focus);\n"
"  if (u_blur_options.y > 0.5)\n"
"    mask = 1.0 - mask;\n"
"  return mask;\n"
"}\n"
"float ldk_retro_threshold()\n"
"{\n"
"  float pattern_scale = max(floor(u_retro.w + 0.5), 1.0);\n"
"  vec2 cell = floor(gl_FragCoord.xy / pattern_scale);\n"
"  ivec2 pattern_coord = ivec2(mod(cell, 4.0));\n"
"  int index = pattern_coord.y * 4 + pattern_coord.x;\n"
"  return (LDK_RETRO_PATTERN[index] + 0.5) / 16.0;\n"
"}\n"
"vec3 ldk_retro_dither(vec3 color)\n"
"{\n"
"  float levels = max(floor(u_retro.y + 0.5), 2.0);\n"
"  float max_level = levels - 1.0;\n"
"  vec3 scaled = clamp(color, 0.0, 1.0) * max_level;\n"
"  vec3 lower = floor(scaled);\n"
"  vec3 fraction = scaled - lower;\n"
"  float threshold = mix(0.5, ldk_retro_threshold(),\n"
"      clamp(u_retro.z, 0.0, 1.0));\n"
"  vec3 quantized = lower + step(vec3(threshold), fraction);\n"
"  return clamp(quantized / max_level, 0.0, 1.0);\n"
"}\n"
"void main()\n"
"{\n"
"  vec2 sample_uv = ldk_distort_uv(v_uv);\n"
"  vec4 original = ldk_sample_drunk_source(LDK_TEXTURE_1, sample_uv);\n"
"  vec4 color = original;\n"
"  if (u_blur_options.x > 0.5)\n"
"  {\n"
"    vec4 blurred = ldk_sample_drunk_source(LDK_TEXTURE_2, sample_uv);\n"
"    color = mix(original, blurred, ldk_blur_mask(v_uv));\n"
"  }\n"
"  if (u_tonemapping.y > 0.5)\n"
"  {\n"
"    color.rgb *= exp2(u_tonemapping.x);\n"
"    color.rgb = ldk_aces_tonemap(color.rgb);\n"
"  }\n"
"  if (u_color_adjustments.w > 0.5)\n"
"  {\n"
"    color.rgb += vec3(u_color_adjustments.x);\n"
"    color.rgb = (color.rgb - vec3(0.5)) * u_color_adjustments.y +\n"
"        vec3(0.5);\n"
"    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
"    color.rgb = mix(vec3(luminance), color.rgb, u_color_adjustments.z);\n"
"  }\n"
"  if (u_stylization_options.z > 0.5)\n"
"  {\n"
"    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
"    color.rgb = vec3(luminance);\n"
"  }\n"
"  if (u_stylization_options.y > 0.5)\n"
"    color.rgb = vec3(1.0) - clamp(color.rgb, 0.0, 1.0);\n"
"  if (u_vignette.w > 0.5)\n"
"  {\n"
"    float aspect = max(u_blur_options.z, 1e-6);\n"
"    vec2 delta = v_uv - vec2(0.5);\n"
"    delta.x *= aspect;\n"
"    float half_diagonal = 0.5 * length(vec2(aspect, 1.0));\n"
"    float distance_to_center =\n"
"        length(delta) / max(half_diagonal, 1e-6);\n"
"    float radius = clamp(u_vignette.y, 0.0, 1.0);\n"
"    float softness = max(clamp(u_vignette.z, 0.0, 1.0), 1e-5);\n"
"    float amount = smoothstep(\n"
"        radius, radius + softness, distance_to_center);\n"
"    color.rgb *= 1.0 - amount * clamp(u_vignette.x, 0.0, 1.0);\n"
"  }\n"
"  if (u_stylization_options.w > 0.5)\n"
"    color.rgb = ldk_retro_dither(color.rgb);\n"
"  out_color = color;\n"
"}\n";

static char const* LDK_RHI_GL33_SKYBOX_PASS_VERTEX_SHADER =
"#version 330 core\n"
"layout(location = 0) in vec3 a_position;\n"
"out vec3 v_direction;\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  mat4 u_view;\n"
"  mat4 u_projection;\n"
"};\n"
"void main()\n"
"{\n"
"  v_direction = a_position;\n"
"  vec4 position = u_projection * mat4(mat3(u_view)) *\n"
"      vec4(a_position, 1.0);\n"
"  gl_Position = position.xyww;\n"
"}\n";

static char const* LDK_RHI_GL33_SKYBOX_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec3 v_direction;\n"
"out vec4 out_color;\n"
"uniform samplerCube LDK_TEXTURE_1;\n"
"void main()\n"
"{\n"
"  out_color = texture(LDK_TEXTURE_1, normalize(v_direction));\n"
"}\n";

static char const* LDK_RHI_GL33_BLUR_PASS_FRAGMENT_SHADER =
"#version 330 core\n"
"in vec2 v_uv;\n"
"out vec4 out_color;\n"
"layout(std140) uniform LDK_UBO_0\n"
"{\n"
"  vec4 u_direction;\n"
"};\n"
"uniform sampler2D LDK_TEXTURE_1;\n"
"void main()\n"
"{\n"
"  vec2 direction = u_direction.xy;\n"
"  vec4 color = texture(LDK_TEXTURE_1, v_uv) * 0.2270270270;\n"
"  color += texture(LDK_TEXTURE_1, v_uv + direction * 1.3846153846) *\n"
"      0.3162162162;\n"
"  color += texture(LDK_TEXTURE_1, v_uv - direction * 1.3846153846) *\n"
"      0.3162162162;\n"
"  color += texture(LDK_TEXTURE_1, v_uv + direction * 3.2307692308) *\n"
"      0.0702702703;\n"
"  color += texture(LDK_TEXTURE_1, v_uv - direction * 3.2307692308) *\n"
"      0.0702702703;\n"
"  out_color = color;\n"
"}\n";
static uint32_t ldk_rhi_gl33_cstr_size(char const* cstr)
{
  return (uint32_t)strlen(cstr);
}

/* Builtin water shaders. */

static char const *LDK_RHI_GL33_WATER_PASS_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view_projection;\n"
    "  mat4 u_inverse_view_projection;\n"
    "  vec4 u_camera_time;\n"
    "  vec4 u_viewport;\n"
    "  vec4 u_shallow_color;\n"
    "  vec4 u_deep_color;\n"
    "  vec4 u_foam_color;\n"
    "  vec4 u_waves[3];\n"
    "  vec4 u_speeds;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_foam;\n"
    "  vec4 u_shore;\n"
    "  vec4 u_sun_direction;\n"
    "  vec4 u_sun_color;\n"
    "  vec4 u_ambient;\n"
    "  vec4 u_texture_flags;\n"
    "  vec4 u_noise;\n"
    "  vec4 u_foam_detail;\n"
    "  vec4 u_caustics;\n"
    "};\n"
    "const float LDK_WATER_TAU = 6.283185307179586;\n"
    "float ldk_water_height(vec2 p)\n"
    "{\n"
    "  float h = u_viewport.z;\n"
    "  for (int i = 0; i < 3; ++i)\n"
    "  {\n"
    "    float k = LDK_WATER_TAU / u_waves[i].w;\n"
    "    float phase = k * (dot(p, u_waves[i].xy) -\n"
    "        u_speeds[i] * u_camera_time.w);\n"
    "    h += u_waves[i].z * sin(phase);\n"
    "  }\n"
    "  return h;\n"
    "}\n"
    "vec2 ldk_water_gradient(vec2 p)\n"
    "{\n"
    "  vec2 gradient = vec2(0.0);\n"
    "  for (int i = 0; i < 3; ++i)\n"
    "  {\n"
    "    float k = LDK_WATER_TAU / u_waves[i].w;\n"
    "    float phase = k * (dot(p, u_waves[i].xy) -\n"
    "        u_speeds[i] * u_camera_time.w);\n"
    "    gradient += u_waves[i].xy * (u_waves[i].z * k * cos(phase));\n"
    "  }\n"
    "  return gradient;\n"
    "}\n"
    "layout(location = 0) in vec3 a_position;\n"
    "out vec3 v_world_position;\n"
    "void main()\n"
    "{\n"
    "  v_world_position = vec3(a_position.x, ldk_water_height(a_position.xz),\n"
    "      a_position.z);\n"
    "  gl_Position = u_view_projection * vec4(v_world_position, 1.0);\n"
    "}\n";

static char const *LDK_RHI_GL33_WATER_PASS_FRAGMENT_SHADER =
    "#version 330 core\n"
    "layout(std140) uniform LDK_UBO_0\n"
    "{\n"
    "  mat4 u_view_projection;\n"
    "  mat4 u_inverse_view_projection;\n"
    "  vec4 u_camera_time;\n"
    "  vec4 u_viewport;\n"
    "  vec4 u_shallow_color;\n"
    "  vec4 u_deep_color;\n"
    "  vec4 u_foam_color;\n"
    "  vec4 u_waves[3];\n"
    "  vec4 u_speeds;\n"
    "  vec4 u_surface;\n"
    "  vec4 u_foam;\n"
    "  vec4 u_shore;\n"
    "  vec4 u_sun_direction;\n"
    "  vec4 u_sun_color;\n"
    "  vec4 u_ambient;\n"
    "  vec4 u_texture_flags;\n"
    "  vec4 u_noise;\n"
    "  vec4 u_foam_detail;\n"
    "  vec4 u_caustics;\n"
    "};\n"
    "const float LDK_WATER_TAU = 6.283185307179586;\n"
    "uniform sampler2D LDK_TEXTURE_1;  // Opaque depth, nearest/clamp.\n"
    "uniform sampler2D LDK_TEXTURE_6;  // Tangent-space normal, RGB.\n"
    "uniform sampler2D LDK_TEXTURE_9;  // Foam mask, R.\n"
    "uniform sampler2D LDK_TEXTURE_10; // Noise, R.\n"
    "uniform sampler2D LDK_TEXTURE_11; // Caustics mask, R.\n"
    "in vec3 v_world_position;\n"
    "out vec4 out_color;\n"
    "\n"
    "vec2 ldk_water_gradient(vec2 p)\n"
    "{\n"
    "  vec2 gradient = vec2(0.0);\n"
    "  for (int i = 0; i < 3; ++i)\n"
    "  {\n"
    "    float k = LDK_WATER_TAU / u_waves[i].w;\n"
    "    float phase = k * (dot(p, u_waves[i].xy) -\n"
    "        u_speeds[i] * u_camera_time.w);\n"
    "    gradient += u_waves[i].xy * (u_waves[i].z * k * cos(phase));\n"
    "  }\n"
    "  return gradient;\n"
    "}\n"
    "\n"
    "vec2 ldk_water_noise(vec2 world_xz)\n"
    "{\n"
    "  if (u_texture_flags.z < 0.5)\n"
    "    return vec2(0.0);\n"
    "  vec2 uv = (world_xz + u_camera_time.w * u_noise.y * vec2(0.8, -0.6)) /\n"
    "      u_noise.x;\n"
    "  return vec2(texture(LDK_TEXTURE_10, uv).r,\n"
    "      texture(LDK_TEXTURE_10, uv + vec2(0.37, 0.71)).r) - 0.5;\n"
    "}\n"
    "\n"
    "vec2 ldk_water_detail(vec2 world_xz, vec2 distortion)\n"
    "{\n"
    "  float time = u_camera_time.w * u_speeds.w / u_surface.x;\n"
    "  if (u_texture_flags.x > 0.5)\n"
    "  {\n"
    "    vec2 uv = world_xz / u_surface.x;\n"
    "    vec3 a = texture(LDK_TEXTURE_6,\n"
    "        uv + time * vec2(0.8, 0.6) + distortion).xyz * 2.0 - 1.0;\n"
    "    vec3 b = texture(LDK_TEXTURE_6,\n"
    "        uv * 1.37 - time * vec2(0.6, 0.8) - distortion).xyz * 2.0 - 1.0;\n"
    "    // Map XY is the XZ tangent plane; map Z points out of the water.\n"
    "    return -0.5 * u_surface.y *\n"
    "        (a.xy / max(a.z, 0.2) + b.xy / max(b.z, 0.2));\n"
    "  }\n"
    "  vec2 p = world_xz * (LDK_WATER_TAU / u_surface.x);\n"
    "  time *= LDK_WATER_TAU;\n"
    "  vec2 a = vec2(cos(p.x + time) * cos(p.y * 0.77 - time),\n"
    "      -0.77 * sin(p.x + time) * sin(p.y * 0.77 - time));\n"
    "  vec2 b = vec2(0.63 * cos(p.x * 0.63 - time * 0.8),\n"
    "      0.91 * cos(p.y * 0.91 + time * 0.6));\n"
    "  return u_surface.y * (a + b * 0.5);\n"
    "}\n"
    "\n"
    "vec3 ldk_water_scene_position(vec2 screen_uv, float depth)\n"
    "{\n"
    "  if (depth >= 1.0)\n"
    "    return v_world_position - vec3(0.0, u_viewport.w, 0.0);\n"
    "  screen_uv = clamp(screen_uv, u_viewport.xy * 0.5,\n"
    "      vec2(1.0) - u_viewport.xy * 0.5);\n"
    "  vec4 h = u_inverse_view_projection *\n"
    "      vec4(screen_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);\n"
    "  return h.xyz / h.w;\n"
    "}\n"
    "\n"
    "// Probe the opaque depth along the water plane, including beyond the\n"
    "// submerged wall's screen footprint. All distances remain in world XZ.\n"
    "vec4 ldk_water_contact_sample(vec3 surface)\n"
    "{\n"
    "  vec4 clip = u_view_projection * vec4(surface, 1.0);\n"
    "  if (clip.w <= 0.0)\n"
    "    return vec4(0.0, 0.0, 0.0, -1.0);\n"
    "  vec3 ndc = clip.xyz / clip.w;\n"
    "  vec2 uv = ndc.xy * 0.5 + 0.5;\n"
    "  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) ||\n"
    "      ndc.z < -1.0 || ndc.z > 1.0)\n"
    "    return vec4(0.0, 0.0, 0.0, -1.0);\n"
    "  uv = (floor(uv / u_viewport.xy) + 0.5) * u_viewport.xy;\n"
    "  float depth = textureLod(LDK_TEXTURE_1, uv, 0.0).r;\n"
    "  vec3 position = ldk_water_scene_position(uv, depth);\n"
    "  float side = u_camera_time.y >= surface.y ? 1.0 : -1.0;\n"
    "  // Reconstruct at the sampled texel centre before testing the water "
    "plane.\n"
    "  // Comparing snapped scene depth with an unsnapped probe causes false "
    "foam.\n"
    "  bool blocked = depth < 1.0 && (position.y - surface.y) * side > 0.0;\n"
    "  return vec4(position, blocked ? 1.0 : 0.0);\n"
    "}\n"
    "\n"
    "float ldk_water_contact_distance(float range, float height_tolerance)\n"
    "{\n"
    "  float distance = range;\n"
    "  if (range <= 0.0)\n"
    "    return distance;\n"
    "  const vec2 directions[8] = vec2[8](\n"
    "      vec2(1.0, 0.0), vec2(0.707107, 0.707107),\n"
    "      vec2(0.0, 1.0), vec2(-0.707107, 0.707107),\n"
    "      vec2(-1.0, 0.0), vec2(-0.707107, -0.707107),\n"
    "      vec2(0.0, -1.0), vec2(0.707107, -0.707107));\n"
    "  for (int i = 0; i < 8; ++i)\n"
    "  {\n"
    "    vec3 direction = vec3(directions[i].x, 0.0, directions[i].y);\n"
    "    float high = range;\n"
    "    vec4 hit = ldk_water_contact_sample(v_world_position + direction * "
    "high);\n"
    "    if (hit.w < 0.5)\n"
    "    {\n"
    "      high = range * 0.5;\n"
    "      hit = ldk_water_contact_sample(v_world_position + direction * "
    "high);\n"
    "      if (hit.w < 0.5)\n"
    "        continue;\n"
    "    }\n"
    "    float low = 0.0;\n"
    "    for (int j = 0; j < 6; ++j)\n"
    "    {\n"
    "      float middle = (low + high) * 0.5;\n"
    "      vec4 probe = ldk_water_contact_sample(v_world_position + direction "
    "* middle);\n"
    "      if (probe.w > 0.5)\n"
    "      {\n"
    "        high = middle;\n"
    "        hit = probe;\n"
    "      }\n"
    "      else\n"
    "        low = middle;\n"
    "    }\n"
    "    // An occluder silhouette above the surface is not a water contact.\n"
    "    if (abs(hit.y - v_world_position.y) <= height_tolerance)\n"
    "      distance = min(distance, length(hit.xz - v_world_position.xz));\n"
    "  }\n"
    "  return distance;\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "  vec2 screen_uv = gl_FragCoord.xy * u_viewport.xy;\n"
    "  float scene_depth = texture(LDK_TEXTURE_1, screen_uv).r;\n"
    "  float left_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(-1, "
    "0)).r;\n"
    "  float right_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(1, "
    "0)).r;\n"
    "  float down_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(0, "
    "-1)).r;\n"
    "  float up_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(0, "
    "1)).r;\n"
    "  float neighbour_depth = max(max(left_depth, right_depth),\n"
    "      max(down_depth, up_depth));\n"
    "  bool has_bottom = scene_depth < 1.0;\n"
    "  vec3 bottom = ldk_water_scene_position(screen_uv, scene_depth);\n"
    "  // Central differences use the same neighbours for every pixel, "
    "avoiding\n"
    "  // the 2x2 normal blocks produced by quad derivatives of scene depth.\n"
    "  vec3 dx = ldk_water_scene_position(screen_uv + vec2(u_viewport.x, "
    "0.0),\n"
    "      right_depth) - ldk_water_scene_position(\n"
    "      screen_uv - vec2(u_viewport.x, 0.0), left_depth);\n"
    "  vec3 dy = ldk_water_scene_position(screen_uv + vec2(0.0, "
    "u_viewport.y),\n"
    "      up_depth) - ldk_water_scene_position(\n"
    "      screen_uv - vec2(0.0, u_viewport.y), down_depth);\n"
    "  vec3 bottom_cross = cross(dx, dy);\n"
    "  vec3 bottom_normal = bottom_cross /\n"
    "      max(length(bottom_cross), 1e-6);\n"
    "  if (bottom_normal.y < 0.0)\n"
    "    bottom_normal = -bottom_normal;\n"
    "  bool valid_contact = has_bottom && neighbour_depth < 1.0;\n"
    "  float water_depth = has_bottom\n"
    "      ? max(v_world_position.y - bottom.y, 0.0) : u_viewport.w;\n"
    "  float depth_mix = smoothstep(0.0, u_viewport.w, water_depth);\n"
    "  vec4 color = mix(u_shallow_color, u_deep_color, depth_mix);\n"
    "\n"
    "  vec2 noise = ldk_water_noise(v_world_position.xz);\n"
    "  vec2 distortion = noise * u_noise.z;\n"
    "  vec2 gradient = ldk_water_gradient(v_world_position.xz) +\n"
    "      ldk_water_detail(v_world_position.xz, distortion);\n"
    "  vec3 normal = normalize(vec3(-gradient.x, 1.0, -gradient.y));\n"
    "  vec3 view_direction = normalize(u_camera_time.xyz - v_world_position);\n"
    "  vec3 sun_direction = u_sun_color.a > 0.0\n"
    "      ? normalize(-u_sun_direction.xyz) : vec3(0.0, 1.0, 0.0);\n"
    "  vec3 sun = u_sun_color.rgb * u_sun_color.a;\n"
    "  vec3 light = u_ambient.rgb * u_ambient.a +\n"
    "      sun * (0.35 + 0.65 * max(dot(normal, sun_direction), 0.0));\n"
    "  vec3 half_vector = sun_direction + view_direction;\n"
    "  vec3 half_direction = half_vector / max(length(half_vector), 1e-6);\n"
    "  float highlight = pow(max(dot(normal, half_direction), 0.0),\n"
    "      u_surface.w) * u_surface.z;\n"
    "  float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 5.0);\n"
    "  color.rgb = mix(color.rgb, u_shallow_color.rgb, fresnel * 0.35);\n"
    "  color.rgb = color.rgb * light + sun * highlight;\n"
    "  if (has_bottom && u_foam.w > 0.0)\n"
    "    color.a *= smoothstep(0.0, u_foam.w, water_depth);\n"
    "\n"
    "  // Project on the reconstructed seabed, not on the moving water plane.\n"
    "  // This is an additive transmitted-light approximation, not ray "
    "tracing.\n"
    "  vec2 bottom_distortion = ldk_water_noise(bottom.xz) * u_noise.z;\n"
    "  vec2 caustics_uv = bottom.xz / u_caustics.x;\n"
    "  float caustics_time = u_camera_time.w * u_caustics.z / u_caustics.x;\n"
    "  float caustics_a = texture(LDK_TEXTURE_11,\n"
    "      caustics_uv + caustics_time * vec2(0.8, 0.6) + "
    "bottom_distortion).r;\n"
    "  float caustics_b = texture(LDK_TEXTURE_11,\n"
    "      caustics_uv * 1.17 - caustics_time * vec2(0.6, 0.8) -\n"
    "      bottom_distortion).r;\n"
    "  float caustics_fade = 1.0 - smoothstep(0.0, u_caustics.w, "
    "water_depth);\n"
    "  float caustics = valid_contact && u_texture_flags.w > 0.5\n"
    "      ? min(caustics_a, caustics_b) * u_caustics.y * caustics_fade *\n"
    "          smoothstep(0.0, 0.08, water_depth) * max(bottom_normal.y, 0.0)\n"
    "      : 0.0;\n"
    "  vec3 transmitted_light = sun * caustics * (1.0 - color.a);\n"
    "\n"
    "  // Shore waves use vertical depth; contact foam follows the "
    "intersection\n"
    "  // contour on the water surface, with world-space width and texture "
    "UVs.\n"
    "  float shore_slope = smoothstep(0.02, 0.10, length(bottom_normal.xz));\n"
    "  vec2 foam_uv = (v_world_position.xz +\n"
    "      u_camera_time.w * u_noise.w * vec2(0.8, -0.6)) / u_foam.z + "
    "distortion;\n"
    "  float foam_sample = u_texture_flags.y > 0.5\n"
    "      ? texture(LDK_TEXTURE_9, foam_uv).r : 1.0;\n"
    "  float foam_aa = clamp(fwidth(foam_sample), 0.006, 0.10);\n"
    "  float contact_range = max(u_foam.x, 1e-4) * (1.0 + noise.x * 0.5);\n"
    "  float surface_pattern = smoothstep(u_foam_detail.x - foam_aa,\n"
    "      u_foam_detail.x + foam_aa, foam_sample);\n"
    "  float phase = LDK_WATER_TAU *\n"
    "      (water_depth + u_camera_time.w * u_shore.z) / u_shore.y + noise.x;\n"
    "  float crest = 0.5 + 0.5 * cos(phase);\n"
    "  float crest_aa = clamp(fwidth(crest), 0.01, 0.15);\n"
    "  float band = smoothstep(0.78 - crest_aa, 0.96 + crest_aa, crest);\n"
    "  float pixel_size = max(length(dFdx(v_world_position)),\n"
    "      length(dFdy(v_world_position)));\n"
    "  float view_slope = abs(view_direction.y) /\n"
    "      max(length(view_direction.xz), 0.125);\n"
    "  float search_range = max(u_foam.x, 0.0) * 1.25;\n"
    "  float height_tolerance = max(0.01,\n"
    "      search_range * 0.05 + pixel_size * view_slope * 0.5);\n"
    "  // All derivatives/implicit texture reads precede this varying "
    "discard.\n"
    "  // The contour probes use explicit LOD and cannot affect opaque depth.\n"
    "  if (gl_FragCoord.z > scene_depth)\n"
    "    discard;\n"
    "  float contact_distance = search_range;\n"
    "  if (u_foam.y > 0.0)\n"
    "    contact_distance = ldk_water_contact_distance(search_range, "
    "height_tolerance);\n"
    "  float cutoff = max(0.05, u_foam_detail.x *\n"
    "      mix(0.25, 1.0, clamp(contact_distance / contact_range, 0.0, "
    "1.0)));\n"
    "  float contact_pattern = smoothstep(cutoff - foam_aa, cutoff + foam_aa,\n"
    "      foam_sample);\n"
    "  float contact = u_foam.x > 0.0\n"
    "      ? (1.0 - smoothstep(contact_range * 0.55, contact_range, "
    "contact_distance)) *\n"
    "          contact_pattern * u_foam.y : 0.0;\n"
    "  float surf = u_shore.x > 0.0\n"
    "      ? band * (1.0 - smoothstep(0.0, u_shore.x, water_depth)) *\n"
    "          surface_pattern * u_shore.w * shore_slope *\n"
    "          smoothstep(0.2, 0.6, bottom_normal.y) : 0.0;\n"
    "  float surface_foam = u_texture_flags.y > 0.5\n"
    "      ? surface_pattern * u_foam_detail.y : 0.0;\n"
    "  float foam = clamp(max(surface_foam, max(contact,\n"
    "      valid_contact ? surf : 0.0)), 0.0, 1.0);\n"
    "  vec3 foam_light = u_ambient.rgb * u_ambient.a + sun;\n"
    "\n"
    "  // Premultiplied blending keeps transmitted light visible in clear "
    "water.\n"
    "  float foam_alpha = foam * u_foam_color.a;\n"
    "  float alpha = foam_alpha + color.a * (1.0 - foam_alpha);\n"
    "  color.rgb = ((color.rgb * color.a + transmitted_light) *\n"
    "      (1.0 - foam_alpha) + u_foam_color.rgb * foam_light * foam_alpha);\n"
    "  color.a = alpha;\n"
    "  out_color = color;\n"
    "}\n";

static char const* ldk_rhi_gl33_builtin_shader_source(uint32_t shader, uint32_t stage)
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

  if (shader == LDK_SHADER_BLUR_PASS &&
      stage == LDK_RHI_SHADER_STAGE_FRAGMENT)
  {
    return LDK_RHI_GL33_BLUR_PASS_FRAGMENT_SHADER;
  }

  return NULL;
}

static LDKRHIShaderModule ldk_rhi_gl33_shader_module_create_with_prefix(void* backend_user_data, const LDKRHIShaderModuleDesc* desc, char const* prefix);

LDKRHIShaderModule ldk_rhi_create_builtin_shader_module(LDKRHIContext* rhi, uint32_t shader, uint32_t stage)
{
  if (rhi == NULL)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  char const* code = ldk_rhi_gl33_builtin_shader_source(shader, stage);
  if (code == NULL)
  {
    return LDK_RHI_INVALID_RESOURCE;
  }

  LDKRHIShaderModuleDesc desc = {0};
  ldk_rhi_shader_module_desc_defaults(&desc);
  desc.stage = stage;
  desc.code_format = LDK_RHI_SHADER_CODE_FORMAT_GLSL;
  desc.code = code;
  desc.code_size = ldk_rhi_gl33_cstr_size(code);

  if (stage == LDK_RHI_SHADER_STAGE_VERTEX &&
      (shader == LDK_SHADER_MESH_PASS_INSTANCED ||
          shader == LDK_SHADER_SHADOW_PASS_INSTANCED ||
          shader == LDK_SHADER_SHADOW_PASS_CUTOUT_INSTANCED ||
          shader == LDK_SHADER_VEGETATION_PASS_INSTANCED ||
          shader == LDK_SHADER_SHADOW_PASS_VEGETATION_INSTANCED))
  {
    return ldk_rhi_gl33_shader_module_create_with_prefix(
        rhi->backend_user_data, &desc, "#define LDK_INSTANCED\n");
  }

  return ldk_rhi_shader_module_create(rhi, &desc);
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

static LDKRHIShaderModule ldk_rhi_gl33_shader_module_create_with_prefix(void* backend_user_data, const LDKRHIShaderModuleDesc* desc, char const* prefix)
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

  if (prefix != NULL)
  {
    char const* version = "#version 330 core\n";
    uint32_t version_size = ldk_rhi_gl33_cstr_size(version);
    if (desc->code_size >= version_size && memcmp(desc->code, version, version_size) == 0)
    {
      const GLchar* sources[3] = {0};
      GLint lengths[3] = {0};

      sources[0] = (const GLchar*)desc->code;
      lengths[0] = (GLint)version_size;
      sources[1] = (const GLchar*)prefix;
      lengths[1] = (GLint)ldk_rhi_gl33_cstr_size(prefix);
      sources[2] = (const GLchar*)desc->code + version_size;
      lengths[2] = (GLint)(desc->code_size - version_size);

      glShaderSource(shader, 3, sources, lengths);
    }
    else
    {
      const GLchar* sources[2] = {0};
      GLint lengths[2] = {0};

      sources[0] = (const GLchar*)prefix;
      lengths[0] = (GLint)ldk_rhi_gl33_cstr_size(prefix);
      sources[1] = source;
      lengths[1] = length;

      glShaderSource(shader, 2, sources, lengths);
    }
  }
  else
  {
    glShaderSource(shader, 1, &source, &length);
  }

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

static LDKRHIShaderModule ldk_rhi_gl33_shader_module_create(void* backend_user_data, const LDKRHIShaderModuleDesc* desc)
{
  return ldk_rhi_gl33_shader_module_create_with_prefix(backend_user_data, desc, NULL);
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
