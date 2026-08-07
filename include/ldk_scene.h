/**
 * @file   ldk_scene.h
 * @brief  Scene data and TML serialization.
 */

#ifndef LDK_SCENE_H
#define LDK_SCENE_H

#include <ldk_common.h>
#include <editor/ldk_component_metadata.h>

#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_string.h>

#ifdef LDK_EDITOR
#include <stdx/stdx_strbuilder.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LDKGame LDKGame;

#define LDK_SCENE_TML_VERSION 1
#define LDK_SCENE_NULL_ENTITY_ID (-1)

/**
 * Entry in the Scene Manager catalog.
 *
 * Scene pointers returned by the Scene Manager refer to its internal catalog
 * and remain valid until the catalog is overridden or the manager is
 * terminated.
 */
typedef struct LDKScene
{
  XFSPath path;
  XSmallstr name;
  u32 index;
} LDKScene;

typedef struct LDKSceneResult
{
  bool ok;
  char error[256];
} LDKSceneResult;

LDK_API void ldk_scene_result_clear(LDKSceneResult *result);
LDK_API void ldk_scene_result_set_error(
    LDKSceneResult *result, const char *error);

/**
 * Loads entities and components from a TML string into the current ECS.
 *
 * This function does not clear existing entities and does not roll back
 * entities created before an error. A caller performing a full scene replace
 * is responsible for clearing the ECS before the call and after a failed call.
 */
LDK_API bool ldk_scene_from_tml(
    const char *source, LDKSceneResult *result);

/**
 * Reads a TML file and loads it into the current ECS.
 *
 * This function has the same ECS ownership semantics as
 * ldk_scene_from_tml().
 */
LDK_API bool ldk_scene_load_tml_file(
    const char *path, LDKSceneResult *result);

#ifdef LDK_EDITOR
/** Serializes the current ECS contents to TML. */
LDK_API bool ldk_scene_to_tml(
    XStrBuilder *out, LDKSceneResult *result);

/** Serializes the current ECS contents and writes them to a TML file. */
LDK_API bool ldk_scene_save_tml_file(
    const char *path, LDKSceneResult *result);
#endif

LDK_API u32 ldk_scene_component_meta_runtime_type(
    const LDKComponentMeta *meta);

LDK_API const LDKComponentMeta *ldk_scene_component_meta_find_by_type(
    LDKGame *game, u32 component_type);

LDK_API const LDKComponentFieldMeta *ldk_scene_component_field_find(
    const LDKComponentMeta *meta, const char *field_name);

LDK_API bool ldk_scene_component_field_is_serializable(
    const LDKComponentMeta *meta, const LDKComponentFieldMeta *field);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_H
