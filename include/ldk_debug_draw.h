/** @file ldk_debug_draw.h
 * Transient 3D debug geometry, available to editor and game builds.
 * All distances, including thickness, are in world units. Colors: 0xRRGGBBAA.
 * No global state or ownership: the context can be kept or copied by callers.
 * Call on the render thread before render_frame, and resubmit every frame.
 * Helpers return false on invalid arguments or submission failure. A failed
 * helper may have submitted part of its shape. Tessellation: 3..4096 segments.
 */
#ifndef LDK_DEBUG_DRAW_H
#define LDK_DEBUG_DRAW_H

#include <module/ldk_renderer.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKDebugDraw
  {
    LDKRenderer *renderer;
    LDKRendererViewId view_id;
    float thickness;
    u32 color;
    bool depth_test;
  } LDKDebugDraw;

  /* White, 0.02 world-unit thickness, depth test/write enabled. */
  LDK_API LDKDebugDraw ldk_debug_draw_make(
      LDKRenderer *renderer, LDKRendererViewId view_id);
  LDK_API bool ldk_debug_draw_line(
      const LDKDebugDraw *draw, Vec3 start, Vec3 end);
  /* Four line segments form the head, whose length is 20% of the arrow. */
  LDK_API bool ldk_debug_draw_arrow(
      const LDKDebugDraw *draw, Vec3 start, Vec3 end);
  /* World orientation with normalized axes; length ignores transform scale.
   * Axis colors override context color: X red, Y green, Z blue (+Z).
   */
  LDK_API bool ldk_debug_draw_axes(
      const LDKDebugDraw *draw, Mat4 world, float length);
  LDK_API bool ldk_debug_draw_box(
      const LDKDebugDraw *draw, Vec3 minimum, Vec3 maximum);
  LDK_API bool ldk_debug_draw_circle(const LDKDebugDraw *draw,
      Vec3 center, Vec3 normal, float radius, u32 segments);
  /* Three orthogonal great circles. */
  LDK_API bool ldk_debug_draw_sphere(
      const LDKDebugDraw *draw, Vec3 center, float radius, u32 segments);
  /* Flat base: height is axial distance, radius is base radius.
   * Direction need not be normalized. Four generators connect apex to rim.
   */
  LDK_API bool ldk_debug_draw_cone(const LDKDebugDraw *draw,
      Vec3 apex, Vec3 direction, float height, float radius, u32 segments);
  /* Inverse(projection * view), NDC depth [-1,+1]. Supports finite
   * perspective and orthographic frustums; rejects corners at infinity.
   */
  LDK_API bool ldk_debug_draw_frustum(
      const LDKDebugDraw *draw, Mat4 inverse_view_projection);

#ifdef __cplusplus
}
#endif
#endif // LDK_DEBUG_DRAW_H
