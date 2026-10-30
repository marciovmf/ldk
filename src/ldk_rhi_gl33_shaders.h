#ifndef LDK_RHI_GL33_SHADERS_H
#define LDK_RHI_GL33_SHADERS_H

/* Private GL33 shader source macros. Included only by ldk_rhi_gl33.c. */

#define LDK_RHI_GL33_UI_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec2 a_position;\n" \
  "layout(location = 1) in vec2 a_uv;\n" \
  "layout(location = 2) in vec4 a_color;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  vec2 u_viewport_size;\n" \
  "};\n" \
  "out vec2 v_uv;\n" \
  "out vec4 v_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec2 ndc = vec2(\n" \
  "    (a_position.x / u_viewport_size.x) * 2.0 - 1.0,\n" \
  "    1.0 - (a_position.y / u_viewport_size.y) * 2.0\n" \
  "  );\n" \
  "  v_uv = a_uv;\n" \
  "  v_color = a_color;\n" \
  "  gl_Position = vec4(ndc, 0.0, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_UI_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "in vec4 v_color;\n" \
  "out vec4 out_color;\n" \
  "uniform sampler2D LDK_TEXTURE_1;\n" \
  "void main()\n" \
  "{\n" \
  "  vec4 tex = texture(LDK_TEXTURE_1, v_uv);\n" \
  "  out_color = tex * v_color;\n" \
  "}\n"

#define LDK_RHI_GL33_TEXT_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec2 a_position;\n" \
  "layout(location = 1) in vec2 a_uv;\n" \
  "layout(location = 2) in vec4 a_color;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_1\n" \
  "{\n" \
  "  mat4 u_world;\n" \
  "  vec4 u_options;\n" \
  "};\n" \
  "out vec2 v_uv;\n" \
  "out vec4 v_color;\n" \
  "void main()\n" \
  "{\n" \
  "  mat4 world = u_world;\n" \
  "  if (u_options.x > 0.5)\n" \
  "  {\n" \
  "    vec3 camera_right = normalize(vec3(\n" \
  "        u_view[0][0], u_view[1][0], u_view[2][0]));\n" \
  "    vec3 camera_up = normalize(vec3(\n" \
  "        u_view[0][1], u_view[1][1], u_view[2][1]));\n" \
  "    vec3 camera_back = normalize(vec3(\n" \
  "        u_view[0][2], u_view[1][2], u_view[2][2]));\n" \
  "    vec3 axis_x = camera_right * world[0].x +\n" \
  "        camera_up * world[0].y;\n" \
  "    vec3 axis_y = camera_right * world[1].x +\n" \
  "        camera_up * world[1].y;\n" \
  "    float z_scale = length(world[2].xyz);\n" \
  "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n" \
  "        vec4(camera_back * z_scale, 0.0), world[3]);\n" \
  "  }\n" \
  "  v_uv = a_uv;\n" \
  "  vec3 lower = a_color.rgb / 12.92;\n" \
  "  vec3 upper = pow((a_color.rgb + 0.055) / 1.055, vec3(2.4));\n" \
  "  v_color = vec4(mix(lower, upper,\n" \
  "      step(vec3(0.04045), a_color.rgb)), a_color.a);\n" \
  "  gl_Position = u_projection * u_view * world *\n" \
  "      vec4(a_position, 0.0, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_TEXT_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "in vec4 v_color;\n" \
  "out vec4 out_color;\n" \
  "uniform sampler2D LDK_TEXTURE_2;\n" \
  "void main()\n" \
  "{\n" \
  "  out_color = texture(LDK_TEXTURE_2, v_uv) * v_color;\n" \
  "}\n"

#define LDK_GL33_VEGETATION_GLSL \
  "uniform sampler2D LDK_VEGETATION_TEXTURE;\n" \
  "vec2 ldk_vegetation_interaction(mat4 world, vec4 interaction)\n" \
  "{\n" \
  "  if (interaction.z <= 0.0 || interaction.w <= 0.0)\n" \
  "    return vec2(0.0);\n" \
  "  vec2 field_uv = (world[3].xz - interaction.xy) * interaction.z;\n" \
  "  if (any(lessThan(field_uv, vec2(0.0))) ||\n" \
  "      any(greaterThan(field_uv, vec2(1.0))))\n" \
  "    return vec2(0.0);\n" \
  "  vec3 encoded = texture(LDK_VEGETATION_TEXTURE, field_uv).rgb;\n" \
  "  vec2 bend = (encoded.rg - vec2(128.0 / 255.0)) *\n" \
  "      (255.0 / 127.0);\n" \
  "  float age = encoded.b * 16.0;\n" \
  "  float t = clamp(age / interaction.w, 0.0, 1.0);\n" \
  "  float recovery = 1.0 - t * t * (3.0 - 2.0 * t);\n" \
  "  return bend * recovery;\n" \
  "}\n" \
  "vec3 ldk_vegetation_position(mat4 world, vec3 local_position,\n" \
  "    vec2 uv, vec4 tangent, vec4 vegetation, vec4 interaction,\n" \
  "    float time, out vec3 bend, out float height)\n" \
  "{\n" \
  "  vec4 world_position = world * vec4(local_position, 1.0);\n" \
  "  height = clamp(uv.y, 0.0, 1.0);\n" \
  "  bend = vec3(0.0);\n" \
  "  if (height > 0.0)\n" \
  "  {\n" \
  "    mat3 basis = mat3(world);\n" \
  "    vec3 up = basis[1] / max(length(basis[1]), 1e-6);\n" \
  "    vec3 curve_direction = basis * tangent.xyz;\n" \
  "    if (length(curve_direction) <= 1e-6)\n" \
  "      curve_direction = basis * vec3(0.0, 0.0, 1.0);\n" \
  "    curve_direction /= max(length(curve_direction), 1e-6);\n" \
  "    vec2 wind_direction = vec2(cos(vegetation.w), sin(vegetation.w));\n" \
  "    float phase = time * vegetation.z * 6.28318530718 +\n" \
  "        dot(world[3].xz, wind_direction) * 0.75;\n" \
  "    float sway = 0.65 * sin(phase) +\n" \
  "        0.35 * sin(phase * 1.73 + 1.2);\n" \
  "    vec3 wind = vec3(wind_direction.x, 0.0, wind_direction.y);\n" \
  "    vec2 interaction_bend = ldk_vegetation_interaction(world, interaction);\n" \
  "    vec3 displacement = curve_direction * vegetation.x +\n" \
  "        wind * (vegetation.y * sway) +\n" \
  "        vec3(interaction_bend.x, 0.0, interaction_bend.y);\n" \
  "    vec3 horizontal = displacement - up * dot(displacement, up);\n" \
  "    float strength = length(horizontal);\n" \
  "    if (strength > 1e-5)\n" \
  "    {\n" \
  "      vec3 direction = horizontal / strength;\n" \
  "      float angle = min(strength * 2.0, 2.4);\n" \
  "      float arc_angle = angle * height;\n" \
  "      float radius = length(basis[1]) / angle;\n" \
  "      world_position.xyz += direction * (radius * (1.0 - cos(arc_angle))) +\n" \
  "          up * (radius * sin(arc_angle) - length(basis[1]) * height);\n" \
  "      bend = direction * angle;\n" \
  "    }\n" \
  "  }\n" \
  "  return world_position.xyz;\n" \
  "}\n" \
  "vec3 ldk_vegetation_normal(mat4 world, vec3 local_normal,\n" \
  "    vec3 bend, float height, float bend_normal_weight)\n" \
  "{\n" \
  "  mat3 basis = mat3(world);\n" \
  "  float blade_height = max(length(basis[1]), 1e-6);\n" \
  "  vec3 world_normal = basis[0] * local_normal.x +\n" \
  "      basis[1] * (local_normal.y / (blade_height * blade_height)) +\n" \
  "      basis[2] * local_normal.z;\n" \
  "  float angle = length(bend) * height * bend_normal_weight;\n" \
  "  if (angle > 1e-5)\n" \
  "  {\n" \
  "    vec3 up = basis[1] / blade_height;\n" \
  "    vec3 direction = bend / max(length(bend), 1e-6);\n" \
  "    vec3 axis = cross(up, direction);\n" \
  "    axis /= max(length(axis), 1e-6);\n" \
  "    float cosine = cos(angle);\n" \
  "    float sine = sin(angle);\n" \
  "    world_normal = world_normal * cosine +\n" \
  "        cross(axis, world_normal) * sine +\n" \
  "        axis * dot(axis, world_normal) * (1.0 - cosine);\n" \
  "  }\n" \
  "  return world_normal;\n" \
  "}\n"

#define LDK_GL33_COLOR_GLSL                                                 \
  "vec3 ldk_srgb_to_linear(vec3 color)\n"                                   \
  "{\n"                                                                     \
  "  vec3 lower = color / 12.92;\n"                                         \
  "  vec3 upper = pow((color + 0.055) / 1.055, vec3(2.4));\n"               \
  "  return mix(lower, upper, step(vec3(0.04045), color));\n"               \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_VERTEX_SHADER \
  "#version 330 core\n" LDK_GL33_COLOR_GLSL \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 1) in vec3 a_normal;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 3) in vec4 a_color;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "layout(location = 9) in vec4 i_instance_color;\n" \
  "layout(location = 10) in vec4 i_uv_rect;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_camera_position;\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_1\n" \
  "{\n" \
  "  mat4 u_world;\n" \
  "  vec4 u_instance_color;\n" \
  "  vec4 u_object_options;\n" \
  "  vec4 u_uv_rect;\n" \
  "};\n" \
  "out vec3 v_normal;\n" \
  "out vec4 v_tangent;\n" \
  "out vec3 v_world_position;\n" \
  "out vec2 v_uv;\n" \
  "out vec4 v_color;\n" \
  "out vec4 v_instance_color;\n" \
  "out vec4 v_atlas_rect;\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  if (u_object_options.x > 0.5)\n" \
  "  {\n" \
  "    vec3 camera_right = normalize(vec3(\n" \
  "        u_view[0][0], u_view[1][0], u_view[2][0]));\n" \
  "    vec3 camera_up = normalize(vec3(\n" \
  "        u_view[0][1], u_view[1][1], u_view[2][1]));\n" \
  "    vec3 camera_back = normalize(vec3(\n" \
  "        u_view[0][2], u_view[1][2], u_view[2][2]));\n" \
  "    vec3 axis_x = camera_right * world[0].x +\n" \
  "        camera_up * world[0].y;\n" \
  "    vec3 axis_y = camera_right * world[1].x +\n" \
  "        camera_up * world[1].y;\n" \
  "    float z_scale = length(world[2].xyz);\n" \
  "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n" \
  "        vec4(camera_back * z_scale, 0.0), world[3]);\n" \
  "  }\n" \
  "  vec4 world_position = world * vec4(a_position, 1.0);\n" \
  "  mat3 basis = mat3(world);\n" \
  "  float basis_determinant = determinant(basis);\n" \
  "  mat3 normal_basis = abs(basis_determinant) > 1e-8\n" \
  "      ? transpose(inverse(basis)) : basis;\n" \
  "  v_normal = normal_basis * a_normal;\n" \
  "  v_tangent = vec4(0.0);\n" \
  "  if (abs(a_tangent.w) > 0.5)\n" \
  "  {\n" \
  "    vec3 n = v_normal / max(length(v_normal), 1e-6);\n" \
  "    vec3 tangent = basis * a_tangent.xyz;\n" \
  "    tangent -= n * dot(n, tangent);\n" \
  "    float tangent_length = length(tangent);\n" \
  "    if (tangent_length > 1e-6)\n" \
  "    {\n" \
  "      float mirror_sign = basis_determinant < 0.0 ? -1.0 : 1.0;\n" \
  "      float handedness = a_tangent.w < 0.0 ? -1.0 : 1.0;\n" \
  "      v_tangent = vec4(tangent / tangent_length,\n" \
  "          handedness * mirror_sign);\n" \
  "    }\n" \
  "  }\n" \
  "  v_world_position = world_position.xyz;\n" \
  "  v_uv = a_uv;\n" \
  "  v_color = vec4(ldk_srgb_to_linear(a_color.rgb), a_color.a);\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  v_instance_color = i_instance_color;\n" \
  "  v_atlas_rect = i_uv_rect;\n" \
  "#else\n" \
  "  v_instance_color = u_instance_color;\n" \
  "  v_atlas_rect = u_uv_rect;\n" \
  "#endif\n" \
  "  gl_Position = u_projection * u_view * world_position;\n" \
  "}\n"

#define LDK_RHI_GL33_TERRAIN_PASS_VERTEX_SHADER \
  "#version 330 core\n" LDK_GL33_COLOR_GLSL \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 1) in vec3 a_normal;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 3) in vec4 a_color;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "layout(location = 9) in vec4 i_instance_color;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "  vec4 u_terrain_info;\n" \
  "  vec4 u_terrain_range[8];\n" \
  "  vec4 u_terrain_color[8];\n" \
  "  vec4 u_terrain_atlas_rect[8];\n" \
  "  vec4 u_terrain_uv_scale[8];\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_camera_position;\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_1\n" \
  "{\n" \
  "  mat4 u_world;\n" \
  "  vec4 u_instance_color;\n" \
  "  vec4 u_object_options;\n" \
  "};\n" \
  "out vec3 v_normal;\n" \
  "out vec4 v_tangent;\n" \
  "out vec3 v_world_position;\n" \
  "out vec2 v_uv;\n" \
  "out vec4 v_color;\n" \
  "out vec4 v_instance_color;\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  if (u_object_options.x > 0.5)\n" \
  "  {\n" \
  "    vec3 camera_right = normalize(vec3(\n" \
  "        u_view[0][0], u_view[1][0], u_view[2][0]));\n" \
  "    vec3 camera_up = normalize(vec3(\n" \
  "        u_view[0][1], u_view[1][1], u_view[2][1]));\n" \
  "    vec3 camera_back = normalize(vec3(\n" \
  "        u_view[0][2], u_view[1][2], u_view[2][2]));\n" \
  "    vec3 axis_x = camera_right * world[0].x +\n" \
  "        camera_up * world[0].y;\n" \
  "    vec3 axis_y = camera_right * world[1].x +\n" \
  "        camera_up * world[1].y;\n" \
  "    float z_scale = length(world[2].xyz);\n" \
  "    world = mat4(vec4(axis_x, 0.0), vec4(axis_y, 0.0),\n" \
  "        vec4(camera_back * z_scale, 0.0), world[3]);\n" \
  "  }\n" \
  "  vec4 world_position = world * vec4(a_position, 1.0);\n" \
  "  mat3 basis = mat3(world);\n" \
  "  float basis_determinant = determinant(basis);\n" \
  "  mat3 normal_basis = abs(basis_determinant) > 1e-8\n" \
  "      ? transpose(inverse(basis)) : basis;\n" \
  "  v_normal = normal_basis * a_normal;\n" \
  "  v_tangent = vec4(0.0);\n" \
  "  if (abs(a_tangent.w) > 0.5)\n" \
  "  {\n" \
  "    vec3 n = v_normal / max(length(v_normal), 1e-6);\n" \
  "    vec3 tangent = basis * a_tangent.xyz;\n" \
  "    tangent -= n * dot(n, tangent);\n" \
  "    float tangent_length = length(tangent);\n" \
  "    if (tangent_length > 1e-6)\n" \
  "    {\n" \
  "      float mirror_sign = basis_determinant < 0.0 ? -1.0 : 1.0;\n" \
  "      float handedness = a_tangent.w < 0.0 ? -1.0 : 1.0;\n" \
  "      v_tangent = vec4(tangent / tangent_length,\n" \
  "          handedness * mirror_sign);\n" \
  "    }\n" \
  "  }\n" \
  "  v_world_position = world_position.xyz;\n" \
  "  v_uv = a_uv;\n" \
  "  v_color = vec4(ldk_srgb_to_linear(a_color.rgb), a_color.a);\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  v_instance_color = i_instance_color;\n" \
  "#else\n" \
  "  v_instance_color = u_instance_color;\n" \
  "#endif\n" \
  "  gl_Position = u_projection * u_view * world_position;\n" \
  "}\n"

#define LDK_RHI_GL33_VEGETATION_PASS_VERTEX_SHADER \
  "#version 330 core\n" LDK_GL33_COLOR_GLSL \
  "#define LDK_VEGETATION_TEXTURE LDK_TEXTURE_8\n" \
  LDK_GL33_VEGETATION_GLSL \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 1) in vec3 a_normal;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 3) in vec4 a_color;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "layout(location = 9) in vec4 i_instance_color;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "};\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_camera_position;\n" \
  "};\n" \
  "#ifndef LDK_INSTANCED\n" \
  "layout(std140) uniform LDK_UBO_1\n" \
  "{\n" \
  "  mat4 u_world;\n" \
  "  vec4 u_instance_color;\n" \
  "};\n" \
  "#endif\n" \
  "out vec3 v_normal;\n" \
  "out vec3 v_world_position;\n" \
  "out vec4 v_color;\n" \
  "out vec4 v_instance_color;\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  vec3 vegetation_bend;\n" \
  "  float vegetation_height;\n" \
  "  vec3 position = ldk_vegetation_position(world, a_position, a_uv,\n" \
  "      a_tangent, u_vegetation, u_vegetation_interaction,\n" \
  "      u_camera_position.w, vegetation_bend, vegetation_height);\n" \
  "  v_normal = ldk_vegetation_normal(world, a_normal, vegetation_bend,\n" \
  "      vegetation_height, abs(a_tangent.w));\n" \
  "  v_world_position = position;\n" \
  "  v_color = vec4(ldk_srgb_to_linear(a_color.rgb), a_color.a);\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  v_instance_color = i_instance_color;\n" \
  "#else\n" \
  "  v_instance_color = u_instance_color;\n" \
  "#endif\n" \
  "  gl_Position = u_projection * u_view * vec4(position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_SHADOW_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_light_view_projection;\n" \
  "  vec4 u_animation;\n" \
  "};\n" \
  "#ifndef LDK_INSTANCED\n" \
  "layout(std140) uniform LDK_UBO_1 { mat4 u_world; };\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_cutout;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform;\n" \
  "};\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  vec3 position = (world * vec4(a_position, 1.0)).xyz;\n" \
  "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_SHADOW_PASS_VEGETATION_VERTEX_SHADER \
  "#version 330 core\n" \
  "#define LDK_VEGETATION_TEXTURE LDK_TEXTURE_4\n" \
  LDK_GL33_VEGETATION_GLSL \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_light_view_projection;\n" \
  "  vec4 u_animation;\n" \
  "};\n" \
  "#ifndef LDK_INSTANCED\n" \
  "layout(std140) uniform LDK_UBO_1 { mat4 u_world; };\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_cutout;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform;\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "};\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  vec3 vegetation_bend;\n" \
  "  float vegetation_height;\n" \
  "  vec3 position = ldk_vegetation_position(world, a_position, a_uv,\n" \
  "      a_tangent, u_vegetation, u_vegetation_interaction,\n" \
  "      u_animation.x, vegetation_bend, vegetation_height);\n" \
  "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_SHADOW_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "void main() {}\n"

#define LDK_RHI_GL33_SHADOW_PASS_CUTOUT_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(location = 2) in vec2 a_uv;\n" \
  "layout(location = 8) in vec4 a_tangent;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "layout(location = 4) in vec4 i_world_0;\n" \
  "layout(location = 5) in vec4 i_world_1;\n" \
  "layout(location = 6) in vec4 i_world_2;\n" \
  "layout(location = 7) in vec4 i_world_3;\n" \
  "layout(location = 10) in vec4 i_uv_rect;\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_light_view_projection;\n" \
  "  vec4 u_animation;\n" \
  "};\n" \
  "#ifndef LDK_INSTANCED\n" \
  "layout(std140) uniform LDK_UBO_1 { mat4 u_world; vec4 u_uv_rect; };\n" \
  "#endif\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_cutout;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform;\n" \
  "};\n" \
  "out vec2 v_uv;\n" \
  "void main()\n" \
  "{\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  mat4 world = mat4(i_world_0, i_world_1, i_world_2, i_world_3);\n" \
  "#else\n" \
  "  mat4 world = u_world;\n" \
  "#endif\n" \
  "  vec2 frame_uv = a_uv * u_uv_transform.xy + u_uv_transform.zw;\n" \
  "#ifdef LDK_INSTANCED\n" \
  "  v_uv = mix(i_uv_rect.xy, i_uv_rect.zw, frame_uv);\n" \
  "#else\n" \
  "  v_uv = mix(u_uv_rect.xy, u_uv_rect.zw, frame_uv);\n" \
  "#endif\n" \
  "  vec3 position = (world * vec4(a_position, 1.0)).xyz;\n" \
  "  gl_Position = u_light_view_projection * vec4(position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_SHADOW_PASS_CUTOUT_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_cutout;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform;\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "void main()\n" \
  "{\n" \
  "  float alpha = texture(LDK_TEXTURE_3, v_uv).a * u_cutout.x;\n" \
  "  if (alpha < u_cutout.y)\n" \
  "    discard;\n" \
  "}\n"

#define LDK_GL33_FOG_FACTOR_GLSL "float ldk_fog_factor(vec3 world_position, vec3 camera_position,\n"\
"    vec4 fog_params)\n"\
"{\n"\
"  float distance = max(length(world_position - camera_position) -\n"\
"      fog_params.x, 0.0);\n"\
"  return 1.0 - exp(-fog_params.y * distance);\n"\
"}\n"

#define LDK_GL33_FOG_GLSL "in vec3 v_world_position;\n"\
"layout(std140) uniform LDK_UBO_0\n"\
"{\n"\
"  mat4 u_view;\n"\
"  mat4 u_projection;\n"\
"  vec4 u_camera_position;\n"\
"};\n"\
"struct LDKLight\n"\
"{\n"\
"  vec4 position_type;\n"\
"  vec4 direction_range;\n"\
"  vec4 color_intensity;\n"\
"  vec4 cone;\n"\
"};\n"\
"layout(std140) uniform LDK_UBO_4\n"\
"{\n"\
"  ivec4 u_light_count;\n"\
"  vec4 u_ambient_sky;\n"\
"  vec4 u_ambient_ground;\n"\
"  LDKLight u_lights[16];\n"\
"  mat4 u_shadow_view_projection;\n"\
"  vec4 u_shadow_params;\n"\
"  vec4 u_fog_color;\n"\
"  vec4 u_fog_params;\n"\
"};\n"

#define LDK_GL33_FOG_APPLY_GLSL "vec3 ldk_apply_fog(vec3 color)\n"\
"{\n"\
"  float amount = ldk_fog_factor(\n"\
"      v_world_position, u_camera_position.xyz, u_fog_params);\n"\
"  return mix(color, u_fog_color.rgb, amount);\n"\
"}\n"

#define LDK_GL33_MESH_FOG_GLSL \
  LDK_GL33_FOG_GLSL LDK_GL33_FOG_FACTOR_GLSL LDK_GL33_FOG_APPLY_GLSL

#define LDK_GL33_LIGHTING_GLSL LDK_GL33_MESH_FOG_GLSL \
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
  "  float sky_weight = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);\n"                 \
  "  vec3 ambient_color = mix(u_ambient_ground.rgb,\n"                         \
  "      u_ambient_sky.rgb, sky_weight);\n"                                     \
  "  diffuse = ambient_color * u_ambient_sky.a;\n"                             \
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
  "      float distance_sq = dot(delta, delta);\n"                            \
  "      float ratio = distance_to_light / light.direction_range.w;\n"        \
  "      float cutoff = clamp(1.0 - pow(ratio, 4.0), 0.0, 1.0);\n"              \
  "      attenuation = cutoff * cutoff / max(distance_sq, 0.01);\n"          \
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
  "      vec3 half_vector = to_light + view_direction;\n"                    \
  "      float half_len_sq = dot(half_vector, half_vector);\n"                \
  "      if (half_len_sq > 1e-8)\n"                                             \
  "      {\n"                                                                   \
  "        vec3 h = half_vector * inversesqrt(half_len_sq);\n"                \
  "        float exponent = max(shininess, 1.0);\n"                          \
  "        float normalization = (exponent + 8.0) / 25.13274123;\n"          \
  "        float highlight = pow(max(dot(n, h), 0.0), exponent);\n"          \
  "        float vdot_h = clamp(dot(view_direction, h), 0.0, 1.0);\n"         \
  "        float fresnel = 0.04 + 0.96 * pow(1.0 - vdot_h, 5.0);\n"           \
  "        specular += energy * ndotl * highlight * normalization\n"         \
  "            * fresnel * specular_strength;\n"                              \
  "      }\n"                                                                   \
  "    }\n"                                                                    \
  "  }\n"                                                                      \
  "  // Stylized reflections of distant bright regions of the environment.\n"  \
  "  if (specular_strength > 0.0 && u_ambient_sky.a > 0.0)\n"                  \
  "  {\n"                                                                      \
  "    vec3 reflection = reflect(-view_direction, n);\n"                       \
  "    float sky = smoothstep(-0.10, 0.35, reflection.y);\n"                   \
  "    float strip_a = (1.0 - smoothstep(0.03, 0.15,\n"                        \
  "        abs(reflection.x + 0.22))) *\n"                                     \
  "        smoothstep(0.05, 0.35, reflection.y) *\n"                           \
  "        (1.0 - smoothstep(0.70, 0.94, reflection.y));\n"                    \
  "    float strip_b = (1.0 - smoothstep(0.02, 0.08,\n"                        \
  "        abs(dot(reflection.xz, vec2(0.6, 0.8)) - 0.45))) *\n"               \
  "        smoothstep(0.16, 0.40, reflection.y) *\n"                           \
  "        (1.0 - smoothstep(0.83, 0.98, reflection.y));\n"                    \
  "    float ndotv = clamp(dot(n, view_direction), 0.0, 1.0);\n"               \
  "    float fresnel = 0.04 + 0.96 * pow(1.0 - ndotv, 5.0);\n"                 \
  "    float environment = 0.22 * sky * fresnel +\n"                           \
  "        (1.8 * strip_a + 1.1 * strip_b) *\n"                                \
  "        (0.35 + 0.65 * fresnel);\n"                                         \
  "    specular += u_ambient_sky.rgb * u_ambient_sky.a *\n"                    \
  "        specular_strength * environment;\n"                                 \
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

#define LDK_RHI_GL33_MESH_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL \
  "in vec4 v_color;\n" \
  "in vec4 v_instance_color;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec3 diffuse;\n" \
  "  vec3 specular;\n" \
  "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n" \
  "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n" \
  "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, " \
  "specular);\n" \
  "  vec4 color = v_color * u_material_color * v_instance_color;\n" \
  "  vec3 emission = color.rgb * u_surface.z;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * diffuse + specular +\n" \
  "      emission), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_TERRAIN_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_LIGHTING_GLSL \
  "in vec3 v_normal;\n" \
  "in vec2 v_uv;\n" \
  "in vec4 v_color;\n" \
  "in vec4 v_instance_color;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "  vec4 u_terrain_info;\n" \
  "  vec4 u_terrain_range[8];\n" \
  "  vec4 u_terrain_color[8];\n" \
  "  vec4 u_terrain_atlas_rect[8];\n" \
  "  vec4 u_terrain_uv_scale[8];\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "out vec4 out_color;\n" \
  "vec4 ldk_terrain_surface(int index)\n" \
  "{\n" \
  "  vec4 rect = u_terrain_atlas_rect[index];\n" \
  "  vec2 scale = u_terrain_uv_scale[index].xy;\n" \
  "  vec2 tile_uv = fract((v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw) * scale);\n" \
  "  vec2 atlas_uv = mix(rect.xy, rect.zw, tile_uv);\n" \
  "  vec4 atlas = texture(LDK_TEXTURE_3, atlas_uv);\n" \
  "  float weight = clamp(u_terrain_range[index].z, 0.0, 1.0);\n" \
  "  return u_terrain_color[index] * mix(vec4(1.0), atlas, weight);\n" \
  "}\n" \
  "void main()\n" \
  "{\n" \
  "  int count = clamp(int(u_terrain_info.x + 0.5), 1, 8);\n" \
  "  vec4 color = ldk_terrain_surface(0);\n" \
  "  float height = v_world_position.y;\n" \
  "  for (int i = 1; i < 8; ++i)\n" \
  "  {\n" \
  "    if (i >= count) break;\n" \
  "    float threshold = u_terrain_range[i].x;\n" \
  "    float width = u_terrain_range[i].y;\n" \
  "    float t = width > 0.000001\n" \
  "        ? smoothstep(threshold - width, threshold, height)\n" \
  "        : step(threshold, height);\n" \
  "    color = mix(color, ldk_terrain_surface(i), t);\n" \
  "  }\n" \
  "  color *= u_material_color * v_instance_color;\n" \
  "  color.rgb *= mix(vec3(1.0), 2.0 * v_color.rgb, v_color.a);\n" \
  "  vec3 diffuse;\n" \
  "  vec3 specular;\n" \
  "  vec3 normal = v_normal / max(length(v_normal), 1e-6);\n" \
  "  ldk_lighting(normal, u_surface.x, u_surface.y, diffuse, specular);\n" \
  "  vec3 emission = color.rgb * u_surface.z;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * diffuse + specular +\n" \
  "      emission), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_VEGETATION_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_LIGHTING_GLSL \
  "in vec3 v_normal;\n" \
  "in vec4 v_color;\n" \
  "in vec4 v_instance_color;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "};\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec3 normal = v_normal / max(length(v_normal), 1e-6);\n" \
  "  if (!gl_FrontFacing)\n" \
  "    normal = -normal;\n" \
  "  vec3 diffuse;\n" \
  "  vec3 specular;\n" \
  "  ldk_lighting(normal, u_surface.x, u_surface.y, diffuse, specular);\n" \
  "  vec4 color = v_color * u_material_color * v_instance_color;\n" \
  "  vec3 emission = color.rgb * u_surface.z;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * diffuse + specular +\n" \
  "      emission), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_VEGETATION_PASS_UNLIT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_LIGHTING_GLSL \
  "in vec4 v_color;\n" \
  "in vec4 v_instance_color;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "  vec4 u_vegetation_interaction;\n" \
  "};\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec4 color = v_color * u_material_color * v_instance_color;\n" \
  "  float shadow = 1.0;\n" \
  "  int shadow_index = u_light_count.y;\n" \
  "  if (shadow_index >= 0 && shadow_index < min(u_light_count.x, 16))\n" \
  "  {\n" \
  "    LDKLight light = u_lights[shadow_index];\n" \
  "    vec3 to_light;\n" \
  "    if (int(light.position_type.w) == 2)\n" \
  "      to_light = normalize(-light.direction_range.xyz);\n" \
  "    else\n" \
  "      to_light = normalize(light.position_type.xyz - v_world_position);\n" \
  "    shadow = ldk_shadow_visibility(vec3(0.0, 1.0, 0.0), to_light);\n" \
  "  }\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * shadow), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_UNLIT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_MESH_FOG_GLSL \
  "in vec4 v_color;\n" \
  "in vec4 v_instance_color;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  out_color = v_color * u_material_color * v_instance_color;\n" \
  "  out_color.rgb = ldk_apply_fog(out_color.rgb);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_SOLID_UNLIT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_MESH_FOG_GLSL \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  out_color = vec4(ldk_apply_fog(u_material_color.rgb),\n" \
  "      u_material_color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_TEXTURED_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL \
  "in vec4 v_instance_color;\n" \
  "in vec4 v_atlas_rect;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec3 diffuse;\n" \
  "  vec3 specular;\n" \
  "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n" \
  "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n" \
  "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, " \
  "specular);\n" \
  "  vec4 color = texture(LDK_TEXTURE_3, mix(v_atlas_rect.xy, v_atlas_rect.zw, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw)) * u_material_color *\n" \
  "      v_instance_color;\n" \
  "  vec3 emission = color.rgb * u_surface.z;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * diffuse + specular +\n" \
  "      emission), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_MESH_FOG_GLSL \
  "in vec2 v_uv;\n" \
  "in vec4 v_instance_color;\n" \
  "in vec4 v_atlas_rect;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  out_color = texture(LDK_TEXTURE_3, mix(v_atlas_rect.xy, v_atlas_rect.zw, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw)) * u_material_color *\n" \
  "      v_instance_color;\n" \
  "  out_color.rgb = ldk_apply_fog(out_color.rgb);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_TEXTURED_CUTOUT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_SURFACE_GLSL LDK_GL33_LIGHTING_GLSL \
  "in vec4 v_instance_color;\n" \
  "in vec4 v_atlas_rect;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec4 color = texture(LDK_TEXTURE_3, mix(v_atlas_rect.xy, v_atlas_rect.zw, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw)) * u_material_color *\n" \
  "      v_instance_color;\n" \
  "  if (color.a < u_surface.w)\n" \
  "    discard;\n" \
  "  vec3 diffuse;\n" \
  "  vec3 specular;\n" \
  "  vec3 normal = ldk_surface_normal(v_uv * u_uv_transform[1].xy + u_uv_transform[1].zw);\n" \
  "  float specular_strength = ldk_surface_specular(u_surface.x, v_uv * u_uv_transform[2].xy + u_uv_transform[2].zw);\n" \
  "  ldk_lighting(normal, specular_strength, u_surface.y, diffuse, " \
  "specular);\n" \
  "  vec3 emission = color.rgb * u_surface.z;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb * diffuse + specular +\n" \
  "      emission), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_MESH_PASS_TEXTURED_UNLIT_CUTOUT_FRAGMENT_SHADER \
  "#version 330 core\n" LDK_GL33_MESH_FOG_GLSL \
  "in vec2 v_uv;\n" \
  "in vec4 v_instance_color;\n" \
  "in vec4 v_atlas_rect;\n" \
  "layout(std140) uniform LDK_UBO_2\n" \
  "{\n" \
  "  vec4 u_material_color;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_vegetation;\n" \
  "  vec4 u_uv_transform[8];\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "out vec4 out_color;\n" \
  "void main()\n" \
  "{\n" \
  "  vec4 color = texture(LDK_TEXTURE_3, mix(v_atlas_rect.xy, v_atlas_rect.zw, v_uv * u_uv_transform[0].xy + u_uv_transform[0].zw)) * u_material_color *\n" \
  "      v_instance_color;\n" \
  "  if (color.a < u_surface.w)\n" \
  "    discard;\n" \
  "  out_color = vec4(ldk_apply_fog(color.rgb), color.a);\n" \
  "}\n"

#define LDK_RHI_GL33_GRID_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec3 a_position;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_grid_center_extent;\n" \
  "  vec4 u_grid_settings;\n" \
  "};\n" \
  "out vec2 v_world_xz;\n" \
  "void main()\n" \
  "{\n" \
  "  v_world_xz = u_grid_center_extent.xz +\n" \
  "      a_position.xz * u_grid_center_extent.w;\n" \
  "  vec3 world_position = vec3(v_world_xz.x,\n" \
  "      u_grid_center_extent.y, v_world_xz.y);\n" \
  "  gl_Position = u_projection * u_view * vec4(world_position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_GRID_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_grid_center_extent;\n" \
  "  vec4 u_grid_settings;\n" \
  "};\n" \
  "in vec2 v_world_xz;\n" \
  "out vec4 out_color;\n" \
  "float grid_coverage(vec2 coordinate)\n" \
  "{\n" \
  "  vec2 derivative = max(fwidth(coordinate), vec2(0.00001));\n" \
  "  vec2 distance_to_line =\n" \
  "      abs(fract(coordinate - 0.5) - 0.5) / derivative;\n" \
  "  float nearest_line = min(distance_to_line.x, distance_to_line.y);\n" \
  "  return 1.0 - smoothstep(0.0, 1.0, nearest_line);\n" \
  "}\n" \
  "float axis_coverage(float coordinate)\n" \
  "{\n" \
  "  float derivative = max(fwidth(coordinate), 0.00001);\n" \
  "  return 1.0 - smoothstep(0.0, 1.5, abs(coordinate) / derivative);\n" \
  "}\n" \
  "void main()\n" \
  "{\n" \
  "  float spacing = max(u_grid_settings.x, 0.0001);\n" \
  "  vec2 grid_coordinate = v_world_xz / spacing;\n" \
  "  vec2 grid_derivative = max(fwidth(grid_coordinate), vec2(0.00001));\n" \
  "  float pixels_per_cell = 1.0 /\n" \
  "      max(grid_derivative.x, grid_derivative.y);\n" \
  "  float detail_fade = smoothstep(1.0, 3.0, pixels_per_cell);\n" \
  "  float grid_alpha =\n" \
  "      grid_coverage(grid_coordinate) * detail_fade * 0.38;\n" \
  "  float x_axis_alpha = axis_coverage(v_world_xz.y) * 0.9;\n" \
  "  float z_axis_alpha = axis_coverage(v_world_xz.x) * 0.9;\n" \
  "  float distance_to_center =\n" \
  "      length(v_world_xz - u_grid_center_extent.xz);\n" \
  "  float extent = max(u_grid_center_extent.w, 0.0001);\n" \
  "  float distance_fade = 1.0 - smoothstep(\n" \
  "      extent * 0.65, extent, distance_to_center);\n" \
  "  float alpha = max(grid_alpha, max(x_axis_alpha, z_axis_alpha)) *\n" \
  "      distance_fade;\n" \
  "  if (alpha <= 0.001)\n" \
  "  {\n" \
  "    discard;\n" \
  "  }\n" \
  "  vec3 color = vec3(0.5);\n" \
  "  color = mix(color, vec3(1.0, 0.25, 0.25), x_axis_alpha);\n" \
  "  color = mix(color, vec3(0.25, 0.38, 1.0), z_axis_alpha);\n" \
  "  out_color = vec4(color, alpha);\n" \
  "}\n"

#define LDK_RHI_GL33_PRESENT_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec2 a_position;\n" \
  "layout(location = 1) in vec2 a_uv;\n" \
  "out vec2 v_uv;\n" \
  "void main()\n" \
  "{\n" \
  "  v_uv = a_uv;\n" \
  "  gl_Position = vec4(a_position, 0.0, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_PRESENT_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "out vec4 out_color;\n" \
  "uniform sampler2D LDK_TEXTURE_0;\n" \
  "void main()\n" \
  "{\n" \
  "  out_color = texture(LDK_TEXTURE_0, v_uv);\n" \
  "}\n"

#define LDK_RHI_GL33_POST_PROCESS_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec2 a_position;\n" \
  "layout(location = 1) in vec2 a_uv;\n" \
  "out vec2 v_uv;\n" \
  "void main()\n" \
  "{\n" \
  "  v_uv = a_uv;\n" \
  "  gl_Position = vec4(a_position, 0.0, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_POST_PROCESS_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "out vec4 out_color;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  vec4 u_tonemapping;\n" \
  "  vec4 u_color_adjustments;\n" \
  "  vec4 u_blur_focus;\n" \
  "  vec4 u_blur_options;\n" \
  "  vec4 u_vignette;\n" \
  "  vec4 u_lens;\n" \
  "  vec4 u_heat;\n" \
  "  vec4 u_stylization_options;\n" \
  "  vec4 u_retro;\n" \
  "  vec4 u_drunk_primary;\n" \
  "  vec4 u_drunk_secondary;\n" \
  "  vec4 u_animation;\n" \
  "  vec4 u_bloom_options;\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_1;\n" \
  "uniform sampler2D LDK_TEXTURE_2;\n" \
  "uniform sampler2D LDK_TEXTURE_3;\n" \
  "const float LDK_RETRO_PATTERN[16] = float[16](\n" \
  "    12.0, 5.0, 6.0, 13.0,\n" \
  "    4.0, 0.0, 1.0, 7.0,\n" \
  "    11.0, 3.0, 2.0, 8.0,\n" \
  "    15.0, 10.0, 9.0, 14.0);\n" \
  "vec3 ldk_aces_tonemap(vec3 color)\n" \
  "{\n" \
  "  const float a = 2.51;\n" \
  "  const float b = 0.03;\n" \
  "  const float c = 2.43;\n" \
  "  const float d = 0.59;\n" \
  "  const float e = 0.14;\n" \
  "  return clamp((color * (a * color + b)) /\n" \
  "      (color * (c * color + d) + e), 0.0, 1.0);\n" \
  "}\n" \
  "vec2 ldk_distort_uv(vec2 uv)\n" \
  "{\n" \
  "  if (u_lens.y <= 0.5)\n" \
  "    return uv;\n" \
  "  float aspect = max(u_blur_options.z, 1e-6);\n" \
  "  vec2 delta = uv - vec2(0.5);\n" \
  "  vec2 radial = vec2(delta.x * aspect, delta.y);\n" \
  "  float radius_squared = dot(radial, radial);\n" \
  "  radial *= 1.0 + u_lens.x * radius_squared;\n" \
  "  return vec2(radial.x / aspect, radial.y) + vec2(0.5);\n" \
  "}\n" \
  "vec2 ldk_heat_uv(vec2 uv, vec2 texture_size)\n" \
  "{\n" \
  "  if (u_stylization_options.x <= 0.5 || u_heat.y <= 0.0)\n" \
  "    return uv;\n" \
  "  float scale = max(u_heat.z, 0.01);\n" \
  "  float time = u_animation.x * max(u_heat.w, 0.0);\n" \
  "  float wave_x = sin(uv.y * (90.0 / scale) + time * 4.0) +\n" \
  "      sin(uv.y * (157.0 / scale) - time * 2.3) * 0.5;\n" \
  "  float wave_y = sin((uv.x * 71.0 + uv.y * 31.0) / scale +\n" \
  "      time * 1.9) * 0.25;\n" \
  "  vec2 texel_size = 1.0 / max(texture_size, vec2(1.0));\n" \
  "  return uv + vec2(wave_x, wave_y) * texel_size * u_heat.y;\n" \
  "}\n" \
  "vec2 ldk_retro_uv(vec2 uv, vec2 texture_size)\n" \
  "{\n" \
  "  if (u_stylization_options.w <= 0.5)\n" \
  "    return uv;\n" \
  "  float pixel_size = max(floor(u_retro.x + 0.5), 1.0);\n" \
  "  vec2 size = max(texture_size, vec2(1.0));\n" \
  "  vec2 pixel = floor(uv * size / pixel_size) * pixel_size +\n" \
  "      vec2(pixel_size * 0.5);\n" \
  "  vec2 min_uv = vec2(0.5) / size;\n" \
  "  vec2 max_uv = (size - vec2(0.5)) / size;\n" \
  "  return clamp(pixel / size, min_uv, max_uv);\n" \
  "}\n" \
  "vec4 ldk_sample_color(sampler2D source, vec2 uv)\n" \
  "{\n" \
  "  vec4 center = texture(source, uv);\n" \
  "  if (u_lens.w <= 0.5 || u_lens.z <= 0.0)\n" \
  "    return center;\n" \
  "  float aspect = max(u_blur_options.z, 1e-6);\n" \
  "  vec2 delta = uv - vec2(0.5);\n" \
  "  vec2 radial = vec2(delta.x * aspect, delta.y);\n" \
  "  float radius = length(radial);\n" \
  "  if (radius <= 1e-6)\n" \
  "    return center;\n" \
  "  vec2 direction = radial / radius;\n" \
  "  direction.x /= aspect;\n" \
  "  vec2 offset = direction * (u_lens.z * 0.02 * radius);\n" \
  "  float red = texture(source, uv + offset).r;\n" \
  "  float blue = texture(source, uv - offset).b;\n" \
  "  return vec4(red, center.g, blue, center.a);\n" \
  "}\n" \
  "vec4 ldk_sample_post_source(sampler2D source, vec2 uv)\n" \
  "{\n" \
  "  vec2 texture_size = vec2(textureSize(source, 0));\n" \
  "  vec2 base_uv = ldk_retro_uv(uv, texture_size);\n" \
  "  vec4 base = ldk_sample_color(source, base_uv);\n" \
  "  if (u_stylization_options.x <= 0.5 || u_heat.x <= 0.0 ||\n" \
  "      u_heat.y <= 0.0)\n" \
  "    return base;\n" \
  "  vec2 heat_uv = ldk_heat_uv(uv, texture_size);\n" \
  "  heat_uv = ldk_retro_uv(heat_uv, texture_size);\n" \
  "  vec4 distorted = ldk_sample_color(source, heat_uv);\n" \
  "  return mix(base, distorted, clamp(u_heat.x, 0.0, 1.0));\n" \
  "}\n" \
  "float ldk_drunk_hash(float value)\n" \
  "{\n" \
  "  return fract(sin(value * 91.3458 + 17.123) * 47453.5453);\n" \
  "}\n" \
  "vec2 ldk_drunk_target(float index)\n" \
  "{\n" \
  "  vec2 value = vec2(\n" \
  "      ldk_drunk_hash(index * 1.371 + 11.0),\n" \
  "      ldk_drunk_hash(index * 2.173 + 37.0));\n" \
  "  return value * 2.0 - vec2(1.0);\n" \
  "}\n" \
  "vec2 ldk_drunk_path(float time, out vec2 direction, out float motion)\n" \
  "{\n" \
  "  float segment = floor(time);\n" \
  "  float phase = fract(time);\n" \
  "  vec2 from = ldk_drunk_target(segment);\n" \
  "  vec2 to = ldk_drunk_target(segment + 1.0);\n" \
  "  vec2 delta = to - from;\n" \
  "  if (dot(delta, delta) < 0.16)\n" \
  "  {\n" \
  "    to = -to;\n" \
  "    delta = to - from;\n" \
  "  }\n" \
  "  float eased = phase * phase * (3.0 - 2.0 * phase);\n" \
  "  direction = delta;\n" \
  "  motion = 4.0 * phase * (1.0 - phase);\n" \
  "  return mix(from, to, eased);\n" \
  "}\n" \
  "vec2 ldk_drunk_uv(vec2 uv)\n" \
  "{\n" \
  "  if (u_drunk_primary.w <= 0.5 || u_drunk_primary.x <= 0.0 ||\n" \
  "      u_drunk_primary.y <= 0.0 || u_drunk_primary.z <= 0.0)\n" \
  "    return uv;\n" \
  "  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n" \
  "  float movement = clamp(u_drunk_primary.z, 0.0, 3.0) * strength;\n" \
  "  float time = u_animation.x * max(u_drunk_primary.y, 0.0) * 0.75;\n" \
  "  vec2 path_direction;\n" \
  "  float path_motion;\n" \
  "  vec2 path = ldk_drunk_path(time, path_direction, path_motion);\n" \
  "  float aspect = max(u_blur_options.z, 1e-6);\n" \
  "  vec2 drift = path * vec2(0.028 / aspect, 0.028) * movement;\n" \
  "  float roll = clamp(path_direction.x, -1.0, 1.0) *\n" \
  "      0.008 * path_motion * movement;\n" \
  "  vec2 delta = uv - vec2(0.5);\n" \
  "  float zoom = 1.0 + 0.12 * movement;\n" \
  "  delta /= zoom;\n" \
  "  float s = sin(roll);\n" \
  "  float c = cos(roll);\n" \
  "  delta = mat2(c, -s, s, c) * delta;\n" \
  "  return vec2(0.5) + delta + drift;\n" \
  "}\n" \
  "vec2 ldk_drunk_offset(vec2 uv)\n" \
  "{\n" \
  "  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n" \
  "  float ghosting = clamp(u_drunk_secondary.x, 0.0, 1.0) * strength;\n" \
  "  float time = u_animation.x * max(u_drunk_primary.y, 0.0) * 0.85;\n" \
  "  vec2 direction = vec2(\n" \
  "      sin(time * 0.91 + uv.y * 3.0),\n" \
  "      cos(time * 0.73 + uv.x * 2.0 + 0.4));\n" \
  "  float len = max(length(direction), 1e-5);\n" \
  "  direction /= len;\n" \
  "  return direction * (0.004 + 0.018 * ghosting);\n" \
  "}\n" \
  "vec4 ldk_sample_drunk_source(sampler2D source, vec2 uv)\n" \
  "{\n" \
  "  if (u_drunk_primary.w <= 0.5 || u_drunk_primary.x <= 0.0)\n" \
  "    return ldk_sample_post_source(source, uv);\n" \
  "  vec2 drunk_uv = ldk_drunk_uv(uv);\n" \
  "  vec4 color = ldk_sample_post_source(source, drunk_uv);\n" \
  "  float strength = clamp(u_drunk_primary.x, 0.0, 1.0);\n" \
  "  float ghosting = clamp(u_drunk_secondary.x, 0.0, 1.0) * strength;\n" \
  "  vec2 offset = ldk_drunk_offset(drunk_uv);\n" \
  "  if (ghosting > 0.0)\n" \
  "  {\n" \
  "    vec4 ghost_a = ldk_sample_post_source(source, drunk_uv + offset);\n" \
  "    vec4 ghost_b = ldk_sample_post_source(source, drunk_uv - offset);\n" \
  "    float ghost_weight = ghosting * 0.45;\n" \
  "    color = (color + (ghost_a + ghost_b) * ghost_weight) /\n" \
  "        (1.0 + ghost_weight * 2.0);\n" \
  "  }\n" \
  "  float chromatic = clamp(u_drunk_secondary.y, 0.0, 1.0) * strength;\n" \
  "  if (chromatic > 0.0)\n" \
  "  {\n" \
  "    float aspect = max(u_blur_options.z, 1e-6);\n" \
  "    vec2 radial = drunk_uv - vec2(0.5);\n" \
  "    radial.x *= aspect;\n" \
  "    float radius = length(radial);\n" \
  "    vec2 ca_offset = offset * (0.35 + radius * 0.9) * chromatic;\n" \
  "    float red = ldk_sample_post_source(source, drunk_uv + ca_offset).r;\n" \
  "    float blue = ldk_sample_post_source(source, drunk_uv - ca_offset).b;\n" \
  "    color.rgb = mix(color.rgb, vec3(red, color.g, blue), chromatic);\n" \
  "  }\n" \
  "  return color;\n" \
  "}\n" \
  "float ldk_blur_mask(vec2 uv)\n" \
  "{\n" \
  "  vec2 delta = uv - u_blur_focus.xy;\n" \
  "  float aspect = max(u_blur_options.z, 1e-6);\n" \
  "  delta.x *= aspect;\n" \
  "  float half_diagonal = 0.5 * length(vec2(aspect, 1.0));\n" \
  "  float distance_to_focus = length(delta) / max(half_diagonal, 1e-6);\n" \
  "  float size = clamp(u_blur_focus.z, 0.0, 1.0);\n" \
  "  float feather = max(clamp(u_blur_focus.w, 0.0, 1.0), 1e-5);\n" \
  "  float mask = smoothstep(size, size + feather, distance_to_focus);\n" \
  "  if (u_blur_options.y > 0.5)\n" \
  "    mask = 1.0 - mask;\n" \
  "  return mask;\n" \
  "}\n" \
  "float ldk_retro_threshold()\n" \
  "{\n" \
  "  float pattern_scale = max(floor(u_retro.w + 0.5), 1.0);\n" \
  "  vec2 cell = floor(gl_FragCoord.xy / pattern_scale);\n" \
  "  ivec2 pattern_coord = ivec2(mod(cell, 4.0));\n" \
  "  int index = pattern_coord.y * 4 + pattern_coord.x;\n" \
  "  return (LDK_RETRO_PATTERN[index] + 0.5) / 16.0;\n" \
  "}\n" \
  "vec3 ldk_retro_dither(vec3 color)\n" \
  "{\n" \
  "  float levels = max(floor(u_retro.y + 0.5), 2.0);\n" \
  "  float max_level = levels - 1.0;\n" \
  "  vec3 scaled = clamp(color, 0.0, 1.0) * max_level;\n" \
  "  vec3 lower = floor(scaled);\n" \
  "  vec3 fraction = scaled - lower;\n" \
  "  float threshold = mix(0.5, ldk_retro_threshold(),\n" \
  "      clamp(u_retro.z, 0.0, 1.0));\n" \
  "  vec3 quantized = lower + step(vec3(threshold), fraction);\n" \
  "  return clamp(quantized / max_level, 0.0, 1.0);\n" \
  "}\n" \
  "void main()\n" \
  "{\n" \
  "  vec2 sample_uv = ldk_distort_uv(v_uv);\n" \
  "  vec4 original = ldk_sample_drunk_source(LDK_TEXTURE_1, sample_uv);\n" \
  "  vec4 color = original;\n" \
  "  if (u_blur_options.x > 0.5)\n" \
  "  {\n" \
  "    vec4 blurred = ldk_sample_drunk_source(LDK_TEXTURE_2, sample_uv);\n" \
  "    color = mix(original, blurred, ldk_blur_mask(v_uv));\n" \
  "  }\n" \
  "  if (u_bloom_options.x > 0.0)\n" \
  "    color.rgb += max(texture(LDK_TEXTURE_3, sample_uv).rgb,\n" \
  "        vec3(0.0)) * u_bloom_options.x;\n" \
  "  if (u_tonemapping.y > 0.5)\n" \
  "  {\n" \
  "    color.rgb *= exp2(u_tonemapping.x);\n" \
  "    color.rgb = ldk_aces_tonemap(color.rgb);\n" \
  "  }\n" \
  "  if (u_color_adjustments.w > 0.5)\n" \
  "  {\n" \
  "    color.rgb += vec3(u_color_adjustments.x);\n" \
  "    color.rgb = (color.rgb - vec3(0.5)) * u_color_adjustments.y +\n" \
  "        vec3(0.5);\n" \
  "    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));\n" \
  "    color.rgb = mix(vec3(luminance), color.rgb, u_color_adjustments.z);\n" \
  "  }\n" \
  "  if (u_stylization_options.z > 0.5)\n" \
  "  {\n" \
  "    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));\n" \
  "    color.rgb = vec3(luminance);\n" \
  "  }\n" \
  "  if (u_stylization_options.y > 0.5)\n" \
  "    color.rgb = vec3(1.0) - clamp(color.rgb, 0.0, 1.0);\n" \
  "  if (u_vignette.w > 0.5)\n" \
  "  {\n" \
  "    float aspect = max(u_blur_options.z, 1e-6);\n" \
  "    vec2 delta = v_uv - vec2(0.5);\n" \
  "    delta.x *= aspect;\n" \
  "    float half_diagonal = 0.5 * length(vec2(aspect, 1.0));\n" \
  "    float distance_to_center =\n" \
  "        length(delta) / max(half_diagonal, 1e-6);\n" \
  "    float radius = clamp(u_vignette.y, 0.0, 1.0);\n" \
  "    float softness = max(clamp(u_vignette.z, 0.0, 1.0), 1e-5);\n" \
  "    float amount = smoothstep(\n" \
  "        radius, radius + softness, distance_to_center);\n" \
  "    color.rgb *= 1.0 - amount * clamp(u_vignette.x, 0.0, 1.0);\n" \
  "  }\n" \
  "  if (u_stylization_options.w > 0.5)\n" \
  "    color.rgb = ldk_retro_dither(color.rgb);\n" \
  "  vec3 linear_color = max(color.rgb, vec3(0.0));\n" \
  "  vec3 lower = linear_color * 12.92;\n" \
  "  vec3 upper = 1.055 * pow(linear_color, vec3(1.0 / 2.4)) - 0.055;\n" \
  "  color.rgb = mix(lower, upper,\n" \
  "      step(vec3(0.0031308), linear_color));\n" \
  "  out_color = color;\n" \
  "}\n"

#define LDK_RHI_GL33_SKYBOX_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(location = 0) in vec3 a_position;\n" \
  "out vec3 v_direction;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_fog_color;\n" \
  "  vec4 u_fog_params;\n" \
  "};\n" \
  "void main()\n" \
  "{\n" \
  "  v_direction = a_position;\n" \
  "  vec4 position = u_projection * mat4(mat3(u_view)) *\n" \
  "      vec4(a_position, 1.0);\n" \
  "  gl_Position = position.xyww;\n" \
  "}\n"

#define LDK_RHI_GL33_SKYBOX_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec3 v_direction;\n" \
  "out vec4 out_color;\n" \
  "uniform samplerCube LDK_TEXTURE_1;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view;\n" \
  "  mat4 u_projection;\n" \
  "  vec4 u_fog_color;\n" \
  "  vec4 u_fog_params;\n" \
  "};\n" \
  "void main()\n" \
  "{\n" \
  "  vec3 direction = normalize(v_direction);\n" \
  "  out_color = texture(LDK_TEXTURE_1, direction);\n" \
  "  float horizon = 1.0 - smoothstep(0.0, 0.16, abs(direction.y));\n" \
  "  float fog_enabled = step(0.000001, u_fog_params.y);\n" \
  "  out_color.rgb = mix(out_color.rgb, u_fog_color.rgb,\n" \
  "      horizon * fog_enabled);\n" \
  "}\n"

#define LDK_RHI_GL33_BLOOM_EXTRACT_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "out vec4 out_color;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  vec4 u_direction;\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_1;\n" \
  "vec3 ldk_bloom_extract(vec3 color)\n" \
  "{\n" \
  "  float brightness = max(max(color.r, color.g), color.b);\n" \
  "  float threshold = u_direction.x;\n" \
  "  float knee = max(threshold * u_direction.y, 0.00001);\n" \
  "  float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);\n" \
  "  soft = soft * soft / (4.0 * knee);\n" \
  "  float contribution = max(brightness - threshold, soft) /\n" \
  "      max(brightness, 0.00001);\n" \
  "  return max(color, vec3(0.0)) * contribution;\n" \
  "}\n" \
  "void main()\n" \
  "{\n" \
  "  vec2 texel = 1.0 / vec2(textureSize(LDK_TEXTURE_1, 0));\n" \
  "  vec3 color = vec3(0.0);\n" \
  "  for (int y = 0; y < 4; y++)\n" \
  "  {\n" \
  "    for (int x = 0; x < 4; x++)\n" \
  "    {\n" \
  "      vec2 offset = (vec2(float(x), float(y)) - 1.5) * texel;\n" \
  "      color += ldk_bloom_extract(texture(LDK_TEXTURE_1,\n" \
  "          v_uv + offset).rgb);\n" \
  "    }\n" \
  "  }\n" \
  "  out_color = vec4(color * 0.0625, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_BLUR_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "in vec2 v_uv;\n" \
  "out vec4 out_color;\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  vec4 u_direction;\n" \
  "};\n" \
  "uniform sampler2D LDK_TEXTURE_1;\n" \
  "void main()\n" \
  "{\n" \
  "  vec2 direction = u_direction.xy;\n" \
  "  vec4 color = texture(LDK_TEXTURE_1, v_uv) * 0.2270270270;\n" \
  "  color += texture(LDK_TEXTURE_1, v_uv + direction * 1.3846153846) *\n" \
  "      0.3162162162;\n" \
  "  color += texture(LDK_TEXTURE_1, v_uv - direction * 1.3846153846) *\n" \
  "      0.3162162162;\n" \
  "  color += texture(LDK_TEXTURE_1, v_uv + direction * 3.2307692308) *\n" \
  "      0.0702702703;\n" \
  "  color += texture(LDK_TEXTURE_1, v_uv - direction * 3.2307692308) *\n" \
  "      0.0702702703;\n" \
  "  out_color = color;\n" \
  "}\n"

/* Builtin water shaders. */

#define LDK_RHI_GL33_WATER_PASS_VERTEX_SHADER \
  "#version 330 core\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view_projection;\n" \
  "  mat4 u_inverse_view_projection;\n" \
  "  vec4 u_camera_time;\n" \
  "  vec4 u_viewport;\n" \
  "  vec4 u_shallow_color;\n" \
  "  vec4 u_deep_color;\n" \
  "  vec4 u_foam_color;\n" \
  "  vec4 u_waves[3];\n" \
  "  vec4 u_speeds;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_foam;\n" \
  "  vec4 u_shore;\n" \
  "  vec4 u_sun_direction;\n" \
  "  vec4 u_sun_color;\n" \
  "  vec4 u_ambient;\n" \
  "  vec4 u_texture_flags;\n" \
  "  vec4 u_noise;\n" \
  "  vec4 u_foam_detail;\n" \
  "  vec4 u_caustics;\n" \
  "  vec4 u_fog_color;\n" \
  "  vec4 u_fog_params;\n" \
  "  vec4 u_wave_fade;\n" \
  "  vec4 u_lakes;\n" \
  "  vec4 u_lake_uv;\n" \
  "};\n" \
  "const float LDK_WATER_TAU = 6.283185307179586;\n" \
  "float ldk_water_height(vec2 p)\n" \
  "{\n" \
  "  float h = u_viewport.z;\n" \
  "  for (int i = 0; i < 3; ++i)\n" \
  "  {\n" \
  "    float k = LDK_WATER_TAU / u_waves[i].w;\n" \
  "    float phase = k * (dot(p, u_waves[i].xy) -\n" \
  "        u_speeds[i] * u_camera_time.w);\n" \
  "    h += u_waves[i].z * sin(phase);\n" \
  "  }\n" \
  "  return h;\n" \
  "}\n" \
  "vec2 ldk_water_gradient(vec2 p)\n" \
  "{\n" \
  "  vec2 gradient = vec2(0.0);\n" \
  "  for (int i = 0; i < 3; ++i)\n" \
  "  {\n" \
  "    float k = LDK_WATER_TAU / u_waves[i].w;\n" \
  "    float phase = k * (dot(p, u_waves[i].xy) -\n" \
  "        u_speeds[i] * u_camera_time.w);\n" \
  "    gradient += u_waves[i].xy * (u_waves[i].z * k * cos(phase));\n" \
  "  }\n" \
  "  return gradient;\n" \
  "}\n" \
  "layout(location = 0) in vec3 a_position;\n" \
  "out vec3 v_world_position;\n" \
  "uniform sampler2D LDK_TEXTURE_12; // Filled island silhouette.\n" \
  "float ldk_water_silhouette(vec2 p)\n" \
  "{\n" \
  "  if (u_lakes.x < 0.5) return 0.0;\n" \
  "  vec2 uv = (p - u_lake_uv.xy) * u_lake_uv.zw;\n" \
  "  if (any(lessThan(uv, vec2(0.0))) ||\n" \
  "      any(greaterThanEqual(uv, vec2(1.0)))) return 0.0;\n" \
  "  ivec2 size_px = textureSize(LDK_TEXTURE_12, 0);\n" \
  "  ivec2 texel = clamp(ivec2(uv * vec2(size_px)),\n" \
  "      ivec2(0), size_px - ivec2(1));\n" \
  "  return texelFetch(LDK_TEXTURE_12, texel, 0).r;\n" \
  "}\n" \
  "float ldk_water_wave_fade(vec2 p)\n" \
  "{\n" \
  "  return 1.0 - ldk_water_silhouette(p);\n" \
  "}\n" \
  "void main()\n" \
  "{\n" \
  "  float fade = ldk_water_wave_fade(a_position.xz);\n" \
  "  float height = u_viewport.z + fade *\n" \
  "      (ldk_water_height(a_position.xz) - u_viewport.z);\n" \
  "  v_world_position = vec3(a_position.x, height, a_position.z);\n" \
  "  gl_Position = u_view_projection * vec4(v_world_position, 1.0);\n" \
  "}\n"

#define LDK_RHI_GL33_WATER_PASS_FRAGMENT_SHADER \
  "#version 330 core\n" \
  "layout(std140) uniform LDK_UBO_0\n" \
  "{\n" \
  "  mat4 u_view_projection;\n" \
  "  mat4 u_inverse_view_projection;\n" \
  "  vec4 u_camera_time;\n" \
  "  vec4 u_viewport;\n" \
  "  vec4 u_shallow_color;\n" \
  "  vec4 u_deep_color;\n" \
  "  vec4 u_foam_color;\n" \
  "  vec4 u_waves[3];\n" \
  "  vec4 u_speeds;\n" \
  "  vec4 u_surface;\n" \
  "  vec4 u_foam;\n" \
  "  vec4 u_shore;\n" \
  "  vec4 u_sun_direction;\n" \
  "  vec4 u_sun_color;\n" \
  "  vec4 u_ambient;\n" \
  "  vec4 u_texture_flags;\n" \
  "  vec4 u_noise;\n" \
  "  vec4 u_foam_detail;\n" \
  "  vec4 u_caustics;\n" \
  "  vec4 u_fog_color;\n" \
  "  vec4 u_fog_params;\n" \
  "  vec4 u_wave_fade;\n" \
  "  vec4 u_lakes;\n" \
  "  vec4 u_lake_uv;\n" \
  "};\n" LDK_GL33_FOG_FACTOR_GLSL \
  "const float LDK_WATER_TAU = 6.283185307179586;\n" \
  "uniform sampler2D LDK_TEXTURE_1;  // Opaque depth, nearest/clamp.\n" \
  "uniform sampler2D LDK_TEXTURE_6;  // Tangent-space normal, RGB.\n" \
  "uniform sampler2D LDK_TEXTURE_9;  // Foam mask, R.\n" \
  "uniform sampler2D LDK_TEXTURE_10; // Noise, R.\n" \
  "uniform sampler2D LDK_TEXTURE_11; // Caustics mask, R.\n" \
  "in vec3 v_world_position;\n" \
  "out vec4 out_color;\n" \
  "uniform sampler2D LDK_TEXTURE_12; // Filled island silhouette.\n" \
  "float ldk_water_silhouette(vec2 p)\n" \
  "{\n" \
  "  if (u_lakes.x < 0.5) return 0.0;\n" \
  "  vec2 uv = (p - u_lake_uv.xy) * u_lake_uv.zw;\n" \
  "  if (any(lessThan(uv, vec2(0.0))) ||\n" \
  "      any(greaterThanEqual(uv, vec2(1.0)))) return 0.0;\n" \
  "  ivec2 size_px = textureSize(LDK_TEXTURE_12, 0);\n" \
  "  ivec2 texel = clamp(ivec2(uv * vec2(size_px)),\n" \
  "      ivec2(0), size_px - ivec2(1));\n" \
  "  return texelFetch(LDK_TEXTURE_12, texel, 0).r;\n" \
  "}\n" \
  "float ldk_water_wave_fade(vec2 p)\n" \
  "{\n" \
  "  return 1.0 - ldk_water_silhouette(p);\n" \
  "}\n" \
  "\n" \
  "vec2 ldk_water_gradient(vec2 p)\n" \
  "{\n" \
  "  vec2 gradient = vec2(0.0);\n" \
  "  for (int i = 0; i < 3; ++i)\n" \
  "  {\n" \
  "    float k = LDK_WATER_TAU / u_waves[i].w;\n" \
  "    float phase = k * (dot(p, u_waves[i].xy) -\n" \
  "        u_speeds[i] * u_camera_time.w);\n" \
  "    gradient += u_waves[i].xy * (u_waves[i].z * k * cos(phase));\n" \
  "  }\n" \
  "  return gradient;\n" \
  "}\n" \
  "\n" \
  "vec2 ldk_water_noise(vec2 world_xz)\n" \
  "{\n" \
  "  if (u_texture_flags.z < 0.5)\n" \
  "    return vec2(0.0);\n" \
  "  vec2 uv = (world_xz + u_camera_time.w * u_noise.y * vec2(0.8, -0.6)) /\n" \
  "      u_noise.x;\n" \
  "  return vec2(texture(LDK_TEXTURE_10, uv).r,\n" \
  "      texture(LDK_TEXTURE_10, uv + vec2(0.37, 0.71)).r) - 0.5;\n" \
  "}\n" \
  "\n" \
  "vec2 ldk_water_detail(vec2 world_xz, vec2 distortion)\n" \
  "{\n" \
  "  float time = u_camera_time.w * u_speeds.w / u_surface.x;\n" \
  "  if (u_texture_flags.x > 0.5)\n" \
  "  {\n" \
  "    vec2 uv = world_xz / u_surface.x;\n" \
  "    vec3 a = texture(LDK_TEXTURE_6,\n" \
  "        uv + time * vec2(0.8, 0.6) + distortion).xyz * 2.0 - 1.0;\n" \
  "    vec3 b = texture(LDK_TEXTURE_6,\n" \
  "        uv * 1.37 - time * vec2(0.6, 0.8) - distortion).xyz * 2.0 - 1.0;\n" \
  "    // Map XY is the XZ tangent plane; map Z points out of the water.\n" \
  "    return -0.5 * u_surface.y *\n" \
  "        (a.xy / max(a.z, 0.2) + b.xy / max(b.z, 0.2));\n" \
  "  }\n" \
  "  vec2 p = world_xz * (LDK_WATER_TAU / u_surface.x);\n" \
  "  time *= LDK_WATER_TAU;\n" \
  "  vec2 a = vec2(cos(p.x + time) * cos(p.y * 0.77 - time),\n" \
  "      -0.77 * sin(p.x + time) * sin(p.y * 0.77 - time));\n" \
  "  vec2 b = vec2(0.63 * cos(p.x * 0.63 - time * 0.8),\n" \
  "      0.91 * cos(p.y * 0.91 + time * 0.6));\n" \
  "  return u_surface.y * (a + b * 0.5);\n" \
  "}\n" \
  "\n" \
  "vec3 ldk_water_scene_position(vec2 screen_uv, float depth)\n" \
  "{\n" \
  "  if (depth >= 1.0)\n" \
  "    return v_world_position - vec3(0.0, u_viewport.w, 0.0);\n" \
  "  screen_uv = clamp(screen_uv, u_viewport.xy * 0.5,\n" \
  "      vec2(1.0) - u_viewport.xy * 0.5);\n" \
  "  vec4 h = u_inverse_view_projection *\n" \
  "      vec4(screen_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);\n" \
  "  return h.xyz / h.w;\n" \
  "}\n" \
  "\n" \
  "// Probe the opaque depth along the water plane, including beyond the\n" \
  "// submerged wall's screen footprint. All distances remain in world XZ.\n" \
  "vec4 ldk_water_contact_sample(vec3 surface)\n" \
  "{\n" \
  "  vec4 clip = u_view_projection * vec4(surface, 1.0);\n" \
  "  if (clip.w <= 0.0)\n" \
  "    return vec4(0.0, 0.0, 0.0, -1.0);\n" \
  "  vec3 ndc = clip.xyz / clip.w;\n" \
  "  vec2 uv = ndc.xy * 0.5 + 0.5;\n" \
  "  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) ||\n" \
  "      ndc.z < -1.0 || ndc.z > 1.0)\n" \
  "    return vec4(0.0, 0.0, 0.0, -1.0);\n" \
  "  uv = (floor(uv / u_viewport.xy) + 0.5) * u_viewport.xy;\n" \
  "  float depth = textureLod(LDK_TEXTURE_1, uv, 0.0).r;\n" \
  "  vec3 position = ldk_water_scene_position(uv, depth);\n" \
  "  float side = u_camera_time.y >= surface.y ? 1.0 : -1.0;\n" \
  "  // Reconstruct at the sampled texel centre before testing the water " \
  "plane.\n" \
  "  // Comparing snapped scene depth with an unsnapped probe causes false " \
  "foam.\n" \
  "  bool blocked = depth < 1.0 && (position.y - surface.y) * side > 0.0;\n" \
  "  return vec4(position, blocked ? 1.0 : 0.0);\n" \
  "}\n" \
  "\n" \
  "float ldk_water_contact_distance(float range, float height_tolerance)\n" \
  "{\n" \
  "  float distance = range;\n" \
  "  if (range <= 0.0)\n" \
  "    return distance;\n" \
  "  const vec2 directions[8] = vec2[8](\n" \
  "      vec2(1.0, 0.0), vec2(0.707107, 0.707107),\n" \
  "      vec2(0.0, 1.0), vec2(-0.707107, 0.707107),\n" \
  "      vec2(-1.0, 0.0), vec2(-0.707107, -0.707107),\n" \
  "      vec2(0.0, -1.0), vec2(0.707107, -0.707107));\n" \
  "  for (int i = 0; i < 8; ++i)\n" \
  "  {\n" \
  "    vec3 direction = vec3(directions[i].x, 0.0, directions[i].y);\n" \
  "    float high = range;\n" \
  "    vec4 hit = ldk_water_contact_sample(v_world_position + direction * " \
  "high);\n" \
  "    if (hit.w < 0.5)\n" \
  "    {\n" \
  "      high = range * 0.5;\n" \
  "      hit = ldk_water_contact_sample(v_world_position + direction * " \
  "high);\n" \
  "      if (hit.w < 0.5)\n" \
  "        continue;\n" \
  "    }\n" \
  "    float low = 0.0;\n" \
  "    for (int j = 0; j < 6; ++j)\n" \
  "    {\n" \
  "      float middle = (low + high) * 0.5;\n" \
  "      vec4 probe = ldk_water_contact_sample(v_world_position + direction " \
  "* middle);\n" \
  "      if (probe.w > 0.5)\n" \
  "      {\n" \
  "        high = middle;\n" \
  "        hit = probe;\n" \
  "      }\n" \
  "      else\n" \
  "        low = middle;\n" \
  "    }\n" \
  "    // An occluder silhouette above the surface is not a water contact.\n" \
  "    if (abs(hit.y - v_world_position.y) <= height_tolerance)\n" \
  "      distance = min(distance, length(hit.xz - v_world_position.xz));\n" \
  "  }\n" \
  "  return distance;\n" \
  "}\n" \
  "\n" \
  "void main()\n" \
  "{\n" \
  "  vec2 screen_uv = gl_FragCoord.xy * u_viewport.xy;\n" \
  "  float scene_depth = texture(LDK_TEXTURE_1, screen_uv).r;\n" \
  "  float left_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(-1, " \
  "0)).r;\n" \
  "  float right_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(1, " \
  "0)).r;\n" \
  "  float down_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(0, " \
  "-1)).r;\n" \
  "  float up_depth = textureOffset(LDK_TEXTURE_1, screen_uv, ivec2(0, " \
  "1)).r;\n" \
  "  float neighbour_depth = max(max(left_depth, right_depth),\n" \
  "      max(down_depth, up_depth));\n" \
  "  bool has_bottom = scene_depth < 1.0;\n" \
  "  vec3 bottom = ldk_water_scene_position(screen_uv, scene_depth);\n" \
  "  // Central differences use the same neighbours for every pixel, " \
  "avoiding\n" \
  "  // the 2x2 normal blocks produced by quad derivatives of scene depth.\n" \
  "  vec3 dx = ldk_water_scene_position(screen_uv + vec2(u_viewport.x, " \
  "0.0),\n" \
  "      right_depth) - ldk_water_scene_position(\n" \
  "      screen_uv - vec2(u_viewport.x, 0.0), left_depth);\n" \
  "  vec3 dy = ldk_water_scene_position(screen_uv + vec2(0.0, " \
  "u_viewport.y),\n" \
  "      up_depth) - ldk_water_scene_position(\n" \
  "      screen_uv - vec2(0.0, u_viewport.y), down_depth);\n" \
  "  vec3 bottom_cross = cross(dx, dy);\n" \
  "  vec3 bottom_normal = bottom_cross /\n" \
  "      max(length(bottom_cross), 1e-6);\n" \
  "  if (bottom_normal.y < 0.0)\n" \
  "    bottom_normal = -bottom_normal;\n" \
  "  bool valid_contact = has_bottom && neighbour_depth < 1.0;\n" \
  "  float water_depth = has_bottom\n" \
  "      ? max(v_world_position.y - bottom.y, 0.0) : u_viewport.w;\n" \
  "  float depth_mix = smoothstep(0.0, u_viewport.w, water_depth);\n" \
  "  vec4 color = mix(u_shallow_color, u_deep_color, depth_mix);\n" \
  "\n" \
  "  vec2 noise = ldk_water_noise(v_world_position.xz);\n" \
  "  vec2 distortion = noise * u_noise.z;\n" \
  "  float wave_fade = ldk_water_wave_fade(v_world_position.xz);\n" \
  "  vec2 gradient = wave_fade *\n" \
  "      (ldk_water_gradient(v_world_position.xz) +\n" \
  "      ldk_water_detail(v_world_position.xz, distortion));\n" \
  "  vec3 normal = normalize(vec3(-gradient.x, 1.0, -gradient.y));\n" \
  "  vec3 view_direction = normalize(u_camera_time.xyz - v_world_position);\n" \
  "  vec3 sun_direction = u_sun_color.a > 0.0\n" \
  "      ? normalize(-u_sun_direction.xyz) : vec3(0.0, 1.0, 0.0);\n" \
  "  vec3 sun = u_sun_color.rgb * u_sun_color.a;\n" \
  "  vec3 light = u_ambient.rgb * u_ambient.a +\n" \
  "      sun * (0.35 + 0.65 * max(dot(normal, sun_direction), 0.0));\n" \
  "  vec3 half_vector = sun_direction + view_direction;\n" \
  "  vec3 half_direction = half_vector / max(length(half_vector), 1e-6);\n" \
  "  float highlight = pow(max(dot(normal, half_direction), 0.0),\n" \
  "      u_surface.w) * u_surface.z;\n" \
  "  float fresnel = pow(1.0 - max(dot(normal, view_direction), 0.0), 5.0);\n" \
  "  color.rgb = mix(color.rgb, u_shallow_color.rgb, fresnel * 0.35);\n" \
  "  color.rgb = color.rgb * light + sun * highlight;\n" \
  "  if (has_bottom && u_foam.w > 0.0)\n" \
  "    color.a *= smoothstep(0.0, u_foam.w, water_depth);\n" \
  "\n" \
  "  // Project on the reconstructed seabed, not on the moving water plane.\n" \
  "  // This is an additive transmitted-light approximation, not ray " \
  "tracing.\n" \
  "  vec2 bottom_distortion = ldk_water_noise(bottom.xz) * u_noise.z;\n" \
  "  vec2 caustics_uv = bottom.xz / u_caustics.x;\n" \
  "  float caustics_time = u_camera_time.w * u_caustics.z / u_caustics.x;\n" \
  "  float caustics_a = texture(LDK_TEXTURE_11,\n" \
  "      caustics_uv + caustics_time * vec2(0.8, 0.6) + " \
  "bottom_distortion).r;\n" \
  "  float caustics_b = texture(LDK_TEXTURE_11,\n" \
  "      caustics_uv * 1.17 - caustics_time * vec2(0.6, 0.8) -\n" \
  "      bottom_distortion).r;\n" \
  "  float caustics_fade = 1.0 - smoothstep(0.0, u_caustics.w, " \
  "water_depth);\n" \
  "  float caustics = valid_contact && u_texture_flags.w > 0.5\n" \
  "      ? min(caustics_a, caustics_b) * u_caustics.y * caustics_fade *\n" \
  "          smoothstep(0.0, 0.08, water_depth) * max(bottom_normal.y, 0.0)\n" \
  "      : 0.0;\n" \
  "  vec3 transmitted_light = sun * caustics * (1.0 - color.a);\n" \
  "\n" \
  "  // Shore waves use vertical depth; contact foam follows the " \
  "intersection\n" \
  "  // contour on the water surface, with world-space width and texture " \
  "UVs.\n" \
  "  float shore_slope = smoothstep(0.02, 0.10, length(bottom_normal.xz));\n" \
  "  vec2 foam_uv = (v_world_position.xz +\n" \
  "      u_camera_time.w * u_noise.w * vec2(0.8, -0.6)) / u_foam.z + " \
  "distortion;\n" \
  "  float foam_sample = u_texture_flags.y > 0.5\n" \
  "      ? texture(LDK_TEXTURE_9, foam_uv).r : 1.0;\n" \
  "  float foam_aa = clamp(fwidth(foam_sample), 0.006, 0.10);\n" \
  "  float contact_range = max(u_foam.x, 1e-4) * (1.0 + noise.x * 0.5);\n" \
  "  float surface_pattern = smoothstep(u_foam_detail.x - foam_aa,\n" \
  "      u_foam_detail.x + foam_aa, foam_sample);\n" \
  "  float phase = LDK_WATER_TAU *\n" \
  "      (water_depth + u_camera_time.w * u_shore.z) / u_shore.y + noise.x;\n" \
  "  float crest = 0.5 + 0.5 * cos(phase);\n" \
  "  float crest_aa = clamp(fwidth(crest), 0.01, 0.15);\n" \
  "  float band = smoothstep(0.78 - crest_aa, 0.96 + crest_aa, crest);\n" \
  "  float pixel_size = max(length(dFdx(v_world_position)),\n" \
  "      length(dFdy(v_world_position)));\n" \
  "  float view_slope = abs(view_direction.y) /\n" \
  "      max(length(view_direction.xz), 0.125);\n" \
  "  float search_range = max(u_foam.x, 0.0) * 1.25;\n" \
  "  float height_tolerance = max(0.01,\n" \
  "      search_range * 0.05 + pixel_size * view_slope * 0.5);\n" \
  "  // All derivatives/implicit texture reads precede this varying " \
  "discard.\n" \
  "  // The contour probes use explicit LOD and cannot affect opaque depth.\n" \
  "  if (gl_FragCoord.z > scene_depth)\n" \
  "    discard;\n" \
  "  float contact_distance = search_range;\n" \
  "  if (u_foam.y > 0.0)\n" \
  "    contact_distance = ldk_water_contact_distance(search_range, " \
  "height_tolerance);\n" \
  "  float cutoff = max(0.05, u_foam_detail.x *\n" \
  "      mix(0.25, 1.0, clamp(contact_distance / contact_range, 0.0, " \
  "1.0)));\n" \
  "  float contact_pattern = smoothstep(cutoff - foam_aa, cutoff + foam_aa,\n" \
  "      foam_sample);\n" \
  "  float contact = u_foam.x > 0.0\n" \
  "      ? (1.0 - smoothstep(contact_range * 0.55, contact_range, " \
  "contact_distance)) *\n" \
  "          contact_pattern * u_foam.y : 0.0;\n" \
  "  if (u_lakes.x > 0.5 && u_lakes.y > 0.0)\n" \
  "  {\n" \
  "    float shallow = smoothstep(0.0, u_lakes.y, water_depth);\n" \
  "    contact *= mix(1.0, shallow,\n" \
  "        ldk_water_silhouette(v_world_position.xz));\n" \
  "  }\n" \
  "  float surf = u_shore.x > 0.0\n" \
  "      ? band *\n" \
  "          (1.0 - smoothstep(0.0, u_shore.x, water_depth)) *\n" \
  "          surface_pattern * u_shore.w * shore_slope *\n" \
  "          smoothstep(0.2, 0.6, bottom_normal.y) : 0.0;\n" \
  "  float surface_foam = u_texture_flags.y > 0.5\n" \
  "      ? surface_pattern * u_foam_detail.y : 0.0;\n" \
  "  float foam = clamp(max(surface_foam, max(contact,\n" \
  "      valid_contact ? surf : 0.0)), 0.0, 1.0);\n" \
  "  vec3 foam_light = u_ambient.rgb * u_ambient.a + sun;\n" \
  "\n" \
  "  // Premultiplied blending keeps transmitted light visible in clear " \
  "water.\n" \
  "  float foam_alpha = foam * u_foam_color.a;\n" \
  "  float alpha = foam_alpha + color.a * (1.0 - foam_alpha);\n" \
  "  color.rgb = ((color.rgb * color.a + transmitted_light) *\n" \
  "      (1.0 - foam_alpha) + u_foam_color.rgb * foam_light * foam_alpha);\n" \
  "  color.a = alpha;\n" \
  "  float fog = ldk_fog_factor(v_world_position,\n" \
  "      u_camera_time.xyz, u_fog_params);\n" \
  "  color.rgb = mix(color.rgb, u_fog_color.rgb * color.a, fog);\n" \
  "  out_color = color;\n" \
  "  if (u_wave_fade.w > 0.5)\n" \
  "    out_color = vec4(1.0 - wave_fade, 0.0, wave_fade, 1.0);\n" \
  "}\n"

#endif // LDK_RHI_GL33_SHADERS_H
