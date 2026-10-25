/**
 * # Line component
 *
 * Dynamically rendered, ordered line segments backed by an in-memory mesh.
 * Segment positions and widths are expressed in entity-local world units.
 * The segment list is stored explicitly in scenes; its mesh is transient.
 */

#ifndef LDK_LINE_H
#define LDK_LINE_H

#include <component/ldk_mesh_source.h>
#include <stdx/stdx_array.h>

#ifdef __cplusplus
extern "C" {
#endif

  /** @group ldk.component.line */
  typedef struct LDKLineSegment
  {
    Vec3 pos_start;
    Vec3 pos_end;
    /* RGBA colors in 0xRRGGBBAA format, multiplied by the alpha fields. */
    //@inspect widget=COLOR
    u32 initial_color;
    //@inspect widget=COLOR
    u32 final_color;
    //@inspect min=0 max=1000
    float initial_width;
    //@inspect min=0 max=1000
    float final_width;
    //@inspect slider min=0 max=1
    float initial_alpha;
    //@inspect slider min=0 max=1
    float final_alpha;
  } LDKLineSegment;

  X_ARRAY_TYPE(LDKLineSegment)

  /** Initialize a newly added segment with useful visible defaults. */
  LDK_API void ldk_line_segment_initialize(void *segment);
  /** Notify generated mesh cache when an Inspector array element changes. */
  LDK_API void ldk_line_segments_changed(void *component);

  /** @group ldk.component.line */
  //@component
  typedef struct LDKLineComponent
  {
    /* Maximum number of active segments. Zero is allowed. */
    //@inspect min=0 max=4096
    u32 max_segments;
    //@inspect element_type=LDKLineSegment initializer=ldk_line_segment_initialize on_change=ldk_line_segments_changed limit_field=max_segments
    XArray_LDKLineSegment *segments;
    //@inspect hidden runtime
    u32 mesh_capacity;
    /* Embedded mesh source, not a separate ECS component. */
    //@inspect hidden runtime
    LDKMeshSource source;
    //@inspect hidden runtime
    bool dirty;
  } LDKLineComponent;

  /** Default segment: opaque white, width 0.1 at both ends. */
  LDK_API LDKLineSegment ldk_line_segment_default(void);

  /** Change the segment limit. Fails if smaller than the active count. */
  LDK_API bool ldk_line_max_segments_set(LDKLineComponent *line, u32 maximum);

  /** Append a segment; optionally return its index. No order is changed. */
  LDK_API bool ldk_line_segment_add(LDKLineComponent *line,
      const LDKLineSegment *segment, u32 *out_index);

  /** Replace an existing segment at index. */
  LDK_API bool ldk_line_segment_set(LDKLineComponent *line, u32 index,
      const LDKLineSegment *segment);

  /** Return a read-only segment; NULL when index is not active. */
  LDK_API const LDKLineSegment *ldk_line_segment_get(
      const LDKLineComponent *line, u32 index);

  /** Remove one segment, preserving the order of all remaining segments. */
  LDK_API bool ldk_line_segment_remove(LDKLineComponent *line, u32 index);

  /** Remove all segments but keep the allocated storage and generated mesh. */
  LDK_API void ldk_line_clear(LDKLineComponent *line);

  /** Count active segments. */
  LDK_API u32 ldk_line_segment_count(const LDKLineComponent *line);

#ifdef LDK_ENGINE
  /** Convert dirty segments into the private mesh asset before rendering. */
  LDK_API bool ldk_line_mesh_sync(
      LDKLineComponent *line, LDKAssetManager *assets);

  /** ECS registration descriptor. */
  LDK_API LDKComponentDesc ldk_line_component_desc(u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_LINE_H
