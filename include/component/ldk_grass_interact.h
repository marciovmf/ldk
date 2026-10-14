/**
 * # Grass interaction component
 *
 * Grass interaction component.
 */

#ifndef LDK_GRASS_INTERACT_H
#define LDK_GRASS_INTERACT_H

#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.component.grass_interact */
//@component
typedef struct LDKGrassInteractComponent
{
  bool enabled;
  //@inspect min=0
  float radius;
  //@inspect min=0
  float strength;
} LDKGrassInteractComponent;

/**
 * @brief Create default settings for grass interact component.
 * @return A value initialized with the engine defaults.
 * @group ldk.component.grass_interact
 */
LDK_API LDKGrassInteractComponent ldk_grass_interact_component_make_default(
    void);

#ifdef LDK_ENGINE
/**
 * @brief Build the ECS registration descriptor for the grass interact component.
 * @param initial_capacity Initial number of entries reserved in the registry.
 * @return A descriptor for the requested component or resource.
 * @group ldk.component.grass_interact
 */
LDK_API LDKComponentDesc ldk_grass_interact_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_INTERACT_H
