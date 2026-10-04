/**
 * @file ldk_grass_interact.h
 * @brief Grass interaction component.
 */
#ifndef LDK_GRASS_INTERACT_H
#define LDK_GRASS_INTERACT_H

#include <ldk_common.h>
#include <module/ldk_component.h>

#ifdef __cplusplus
extern "C" {
#endif

//@component
typedef struct LDKGrassInteractComponent
{
  bool enabled;
  //@inspect min=0
  float radius;
  //@inspect min=0
  float strength;
} LDKGrassInteractComponent;

LDK_API LDKGrassInteractComponent ldk_grass_interact_component_make_default(
    void);

#ifdef LDK_ENGINE
LDK_API LDKComponentDesc ldk_grass_interact_component_desc(
    u32 initial_capacity);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_INTERACT_H
