#include <ldk_debug_draw.h>

#include <math.h>

static bool s_debug_vec_valid(Vec3 value)
{
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool s_debug_draw_valid(const LDKDebugDraw *draw)
{
  return draw && draw->renderer && draw->renderer->is_initialized &&
         draw->view_id != LDK_RENDERER_VIEW_INVALID &&
         isfinite(draw->thickness) && draw->thickness > 0.0f;
}

static bool s_debug_basis(Vec3 direction, Vec3 *forward, Vec3 *right, Vec3 *up)
{
  float length = hypotf(hypotf(direction.x, direction.y), direction.z);
  if (!s_debug_vec_valid(direction) || !isfinite(length) || length == 0.0f)
  {
    return false;
  }
  *forward = vec3_make(direction.x / length, direction.y / length,
      direction.z / length);
  Vec3 reference = fabsf(forward->y) < 0.9f
      ? vec3_make(0, 1, 0) : vec3_make(1, 0, 0);
  *right = vec3_norm(vec3_cross(reference, *forward));
  *up = vec3_cross(*forward, *right);
  return true;
}

static Vec3 s_debug_ring_point(Vec3 center, Vec3 right, Vec3 up,
    float radius, float angle)
{
  return vec3_add(center, vec3_add(vec3_mul(right, radius * cosf(angle)),
      vec3_mul(up, radius * sinf(angle))));
}

static bool s_debug_edges(const LDKDebugDraw *draw, const Vec3 corners[8])
{
  static const u32 edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (u32 i = 0; i < 12; ++i)
  {
    if (!ldk_debug_draw_line(draw, corners[edges[i][0]], corners[edges[i][1]]))
    {
      return false;
    }
  }
  return true;
}

LDKDebugDraw ldk_debug_draw_make(
    LDKRenderer *renderer, LDKRendererViewId view_id)
{
  LDKDebugDraw draw = {0};
  draw.renderer = renderer;
  draw.view_id = view_id;
  draw.thickness = 0.02f;
  draw.color = 0xffffffffu;
  draw.depth_test = true;
  return draw;
}

bool ldk_debug_draw_line(const LDKDebugDraw *draw, Vec3 start, Vec3 end)
{
  return s_debug_draw_valid(draw) &&
         ldk_renderer_draw_line(draw->renderer, draw->view_id, start, end,
             draw->thickness, draw->color, draw->depth_test);
}

bool ldk_debug_draw_arrow(const LDKDebugDraw *draw, Vec3 start, Vec3 end)
{
  if (!s_debug_draw_valid(draw) || !s_debug_vec_valid(start) ||
      !s_debug_vec_valid(end))
  {
    return false;
  }
  Vec3 delta = vec3_sub(end, start);
  float length = hypotf(hypotf(delta.x, delta.y), delta.z);
  if (length == 0.0f)
  {
    return true;
  }
  Vec3 forward, right, up;
  if (!s_debug_basis(delta, &forward, &right, &up))
  {
    return false;
  }
  Vec3 base = vec3_sub(end, vec3_mul(forward, length * 0.2f));
  if (!ldk_debug_draw_line(draw, start, end))
  {
    return false;
  }
  for (u32 i = 0; i < 4; ++i)
  {
    Vec3 point = s_debug_ring_point(base, right, up, length * 0.08f,
        (float)i * STDXM_PI * 0.5f);
    if (!ldk_debug_draw_line(draw, end, point))
    {
      return false;
    }
  }
  return true;
}

bool ldk_debug_draw_axes(const LDKDebugDraw *draw, Mat4 world, float length)
{
  if (!s_debug_draw_valid(draw) || !isfinite(length) || length <= 0.0f)
  {
    return false;
  }
  Vec3 origin = vec3_make(world.m[12], world.m[13], world.m[14]);
  Vec3 axes[3];
  for (u32 i = 0; i < 3; ++i)
  {
    Vec3 right, up;
    Vec3 axis = vec3_make(world.m[i * 4], world.m[i * 4 + 1],
        world.m[i * 4 + 2]);
    if (!s_debug_basis(axis, &axes[i], &right, &up))
    {
      return false;
    }
  }
  const u32 colors[] = {0xff4040ffu, 0x40ff40ffu, 0x4080ffffu};
  LDKDebugDraw axis_draw = *draw;
  for (u32 i = 0; i < 3; ++i)
  {
    axis_draw.color = colors[i];
    if (!ldk_debug_draw_arrow(&axis_draw, origin,
            vec3_add(origin, vec3_mul(axes[i], length))))
    {
      return false;
    }
  }
  return true;
}

bool ldk_debug_draw_box(const LDKDebugDraw *draw, Vec3 minimum, Vec3 maximum)
{
  if (!s_debug_draw_valid(draw) || !s_debug_vec_valid(minimum) ||
      !s_debug_vec_valid(maximum) || minimum.x > maximum.x ||
      minimum.y > maximum.y || minimum.z > maximum.z)
  {
    return false;
  }
  Vec3 corners[8] = {
      {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
      {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
      {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
      {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z}};
  return s_debug_edges(draw, corners);
}

bool ldk_debug_draw_circle(const LDKDebugDraw *draw, Vec3 center, Vec3 normal,
    float radius, u32 segments)
{
  Vec3 forward, right, up;
  if (!s_debug_draw_valid(draw) || !s_debug_vec_valid(center) ||
      !isfinite(radius) || radius <= 0.0f || segments < 3 || segments > 4096 ||
      !s_debug_basis(normal, &forward, &right, &up))
  {
    return false;
  }
  Vec3 first = s_debug_ring_point(center, right, up, radius, 0.0f);
  Vec3 previous = first;
  for (u32 i = 1; i <= segments; ++i)
  {
    Vec3 next = i == segments ? first : s_debug_ring_point(center, right, up,
        radius, 2.0f * STDXM_PI * (float)i / (float)segments);
    if (!ldk_debug_draw_line(draw, previous, next))
    {
      return false;
    }
    previous = next;
  }
  return true;
}

bool ldk_debug_draw_sphere(const LDKDebugDraw *draw, Vec3 center,
    float radius, u32 segments)
{
  return ldk_debug_draw_circle(draw, center, vec3_make(1, 0, 0), radius, segments) &&
         ldk_debug_draw_circle(draw, center, vec3_make(0, 1, 0), radius, segments) &&
         ldk_debug_draw_circle(draw, center, vec3_make(0, 0, 1), radius, segments);
}

bool ldk_debug_draw_cone(const LDKDebugDraw *draw, Vec3 apex, Vec3 direction,
    float height, float radius, u32 segments)
{
  Vec3 forward, right, up;
  if (!s_debug_draw_valid(draw) || !s_debug_vec_valid(apex) ||
      !isfinite(height) || height < 0.0f || !isfinite(radius) || radius < 0.0f ||
      segments < 3 || segments > 4096 ||
      !s_debug_basis(direction, &forward, &right, &up))
  {
    return false;
  }
  Vec3 center = vec3_add(apex, vec3_mul(forward, height));
  if (radius == 0.0f)
  {
    return ldk_debug_draw_line(draw, apex, center);
  }
  if (!ldk_debug_draw_circle(draw, center, forward, radius, segments))
  {
    return false;
  }
  for (u32 i = 0; i < 4; ++i)
  {
    Vec3 point = s_debug_ring_point(center, right, up, radius,
        (float)i * STDXM_PI * 0.5f);
    if (!ldk_debug_draw_line(draw, apex, point))
    {
      return false;
    }
  }
  return true;
}

bool ldk_debug_draw_frustum(const LDKDebugDraw *draw, Mat4 inverse_view_projection)
{
  if (!s_debug_draw_valid(draw))
  {
    return false;
  }
  const float xy[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  Vec3 corners[8];
  const float *m = inverse_view_projection.m;
  for (u32 i = 0; i < 8; ++i)
  {
    float x = xy[i % 4][0];
    float y = xy[i % 4][1];
    float z = i < 4 ? -1.0f : 1.0f;
    float w = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (!isfinite(w) || w == 0.0f)
    {
      return false;
    }
    corners[i] = vec3_make((m[0] * x + m[4] * y + m[8] * z + m[12]) / w,
        (m[1] * x + m[5] * y + m[9] * z + m[13]) / w,
        (m[2] * x + m[6] * y + m[10] * z + m[14]) / w);
    if (!s_debug_vec_valid(corners[i]))
    {
      return false;
    }
  }
  return s_debug_edges(draw, corners);
}
