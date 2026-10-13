/**
 * # Debug drawing
 *
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

  /** @group core.debug_draw.drawing */
  typedef struct LDKDebugDraw
  {
    LDKRenderer *renderer;
    LDKRendererViewId view_id;
    float thickness;
    u32 color;
    bool depth_test;
  } LDKDebugDraw;

  /* White, 0.02 world-unit thickness, depth test/write enabled. */
  /**
   * @brief Create a debug drawing context for a renderer view.
   * @param renderer Renderer receiving this request.
   * @param view_id Renderer view receiving this operation.
   * @return The requested value.
   * @group core.debug_draw.drawing
   */
  LDK_API LDKDebugDraw ldk_debug_draw_make(
      LDKRenderer *renderer, LDKRendererViewId view_id);
  /**
   * @brief Draw a line between two 3D positions.
   * @param draw Debug drawing context.
   * @param start Starting timestamp or location for this operation.
   * @param end Ending timestamp or location for this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.lines
   */
  LDK_API bool ldk_debug_draw_line(
      const LDKDebugDraw *draw, Vec3 start, Vec3 end);
  /* Four line segments form the head, whose length is 20% of the arrow. */
  /**
   * @brief Draw a directional arrow between two 3D positions.
   * @param draw Debug drawing context.
   * @param start Starting timestamp or location for this operation.
   * @param end Ending timestamp or location for this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.drawing
   */
  LDK_API bool ldk_debug_draw_arrow(
      const LDKDebugDraw *draw, Vec3 start, Vec3 end);
  /* World orientation with normalized axes; length ignores transform scale.
   * Axis colors override context color: X red, Y green, Z blue (+Z).
   */
  /**
   * @brief Draw coordinate axes using a world transform.
   * @param draw Debug drawing context.
   * @param world Object-to-world transformation matrix.
   * @param length Length used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.drawing
   */
  LDK_API bool ldk_debug_draw_axes(
      const LDKDebugDraw *draw, Mat4 world, float length);
  /**
   * @brief Draw an axis-aligned 3D bounding box.
   * @param draw Debug drawing context.
   * @param minimum Minimum used by this operation.
   * @param maximum Maximum used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.shapes
   */
  LDK_API bool ldk_debug_draw_box(
      const LDKDebugDraw *draw, Vec3 minimum, Vec3 maximum);
  /**
   * @brief Draw a circle in 3D space.
   * @param draw Debug drawing context.
   * @param center Center of the geometry.
   * @param normal Surface normal vector.
   * @param radius Interaction or geometry radius.
   * @param segments Segments used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.drawing
   */
  LDK_API bool ldk_debug_draw_circle(const LDKDebugDraw *draw,
      Vec3 center, Vec3 normal, float radius, u32 segments);
  /* Three orthogonal great circles. */
  /**
   * @brief Draw a wireframe sphere in 3D space.
   * @param draw Debug drawing context.
   * @param center Center of the geometry.
   * @param radius Interaction or geometry radius.
   * @param segments Segments used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.shapes
   */
  LDK_API bool ldk_debug_draw_sphere(
      const LDKDebugDraw *draw, Vec3 center, float radius, u32 segments);
  /* Flat base: height is axial distance, radius is base radius.
   * Direction need not be normalized. Four generators connect apex to rim.
   */
  /**
   * @brief Draw a wireframe cone in 3D space.
   * @param draw Debug drawing context.
   * @param apex Apex used by this operation.
   * @param direction Direction used by this operation.
   * @param height Height in pixels or the units expected by the API.
   * @param radius Interaction or geometry radius.
   * @param segments Segments used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.drawing
   */
  LDK_API bool ldk_debug_draw_cone(const LDKDebugDraw *draw,
      Vec3 apex, Vec3 direction, float height, float radius, u32 segments);
  /* Inverse(projection * view), NDC depth [-1,+1]. Supports finite
   * perspective and orthographic frustums; rejects corners at infinity.
   */
  /**
   * @brief Draw a camera or projection frustum.
   * @param draw Debug drawing context.
   * @param inverse_view_projection Inverse view projection used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group core.debug_draw.drawing
   */
  LDK_API bool ldk_debug_draw_frustum(
      const LDKDebugDraw *draw, Mat4 inverse_view_projection);

#ifdef __cplusplus
}
#endif
#endif // LDK_DEBUG_DRAW_H
