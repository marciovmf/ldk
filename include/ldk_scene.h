#ifndef LDK_SCENE_H
#define LDK_SCENE_H

#include <ldk_common.h>
#include <editor/ldk_component_metadata.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LDKGame LDKGame;

#define LDK_SCENE_TML_VERSION 1
#define LDK_SCENE_NULL_ENTITY_ID (-1)

/*
 * Entity references are always handled internally by scene_to_tml/from_tml:
 *   -1 = null entity reference
 *    0+ = temporary scene-local entity id
 */

typedef struct LDKSceneResult
{
  bool ok;
  char error[256];
} LDKSceneResult;

LDK_API void ldk_scene_result_clear(LDKSceneResult* result);
LDK_API void ldk_scene_result_set_error(LDKSceneResult* result, const char* error);

LDK_API u32 ldk_scene_component_meta_runtime_type(const LDKComponentMeta* meta);

LDK_API const LDKComponentMeta* ldk_scene_component_meta_find_by_type(
    LDKGame* game,
    u32 component_type);

LDK_API const LDKComponentFieldMeta* ldk_scene_component_field_find(
    const LDKComponentMeta* meta,
    const char* field_name);

LDK_API bool ldk_scene_component_field_is_serializable(
    const LDKComponentMeta* meta,
    const LDKComponentFieldMeta* field);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_H
