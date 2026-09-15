/** @file ldk_light.h
 * @brief Scene light components. Spot angles are half-cone angles in radians.
 * Position and direction come from Transform; forward is local -Z.
 */
#ifndef LDK_LIGHT_H
#define LDK_LIGHT_H

#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //@component
  typedef struct LDKPointLight
  {
    //@inspect widget=COLOR
    u32 color;
    float intensity;
    float range;
    bool enabled;
  } LDKPointLight;

  //@component
  typedef struct LDKSpotLight
  {
    //@inspect widget=COLOR
    u32 color;
    float intensity;
    float range;
    float inner_angle;
    float outer_angle;
    bool enabled;
  } LDKSpotLight;

  //@component
  typedef struct LDKDirectionalLight
  {
    //@inspect widget=COLOR
    u32 color;
    float intensity;
    bool enabled;
  } LDKDirectionalLight;

#ifdef LDK_ENGINE
  LDK_API LDKComponentDesc ldk_point_light_component_desc(u32 initial_capacity);
  LDK_API LDKComponentDesc ldk_spot_light_component_desc(u32 initial_capacity);
  LDK_API LDKComponentDesc ldk_directional_light_component_desc(
      u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif
#endif // LDK_LIGHT_H
