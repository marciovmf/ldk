/**
 * # Trail component
 *
 * Records a moving entity's world-space path and writes an age-faded trail
 * to its required Line component. Geometry and materials belong to Line.
 */

#ifndef LDK_TRAIL_H
#define LDK_TRAIL_H

#include <component/ldk_line.h>

#ifdef __cplusplus
extern "C" {
#endif

  /** @group ldk.component.trail */
  typedef struct LDKTrailPoint
  {
    Vec3 position;
    float age;
  } LDKTrailPoint;

  /** @group ldk.component.trail */
  //@component
  typedef struct LDKTrailComponent
  {
    /* Disabling clears the trail. The required Line stays attached. */
    bool enabled;
    /* Seconds each recorded point remains alive. */
    //@inspect min=0.01 max=30
    float lifetime;
    /* Minimum distance between emitted points, in world units. */
    //@inspect min=0 max=10
    float min_distance;
    /* The position sampled, relative to the entity's transform. */
    Vec3 local_offset;
    /* Endpoints of the age gradient: initial = old, final = new. */
    //@inspect widget=COLOR
    u32 initial_color;
    //@inspect widget=COLOR
    u32 final_color;
    float initial_width;
    float final_width;
    float initial_alpha;
    float final_alpha;

    /* Runtime positions are kept in world space, not serialized. */
    //@inspect hidden runtime
    LDKTrailPoint *points;
    //@inspect readonly runtime
    u32 point_count;
    //@inspect hidden runtime
    u32 point_capacity;
    //@inspect hidden runtime
    bool error_reported;
  } LDKTrailComponent;

  /** Initialize authored parameters with defaults. */
  LDK_API LDKTrailComponent ldk_trail_component_make_default(void);

  /** Reset the runtime path and clear the dependent Line. */
  LDK_API void ldk_trail_clear(
      LDKTrailComponent *trail, LDKLineComponent *line);

#ifdef LDK_ENGINE
  /** Advance one trail using the entity's current world transform. */
  LDK_API bool ldk_trail_update(LDKTrailComponent *trail,
      LDKLineComponent *line, Mat4 world, float dt);

  /** Implicit engine update, after gameplay and before mesh submission. */
  LDK_API void ldk_trail_update_all(float dt);

  /** Register the trail component with the ECS. */
  LDK_API LDKComponentDesc ldk_trail_component_desc(u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_TRAIL_H
