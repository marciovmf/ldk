/**
 * @file   ldk_scene_systems.h
 * @brief  System associations stored in a scene file.
 */

#ifndef LDK_SCENE_SYSTEMS_H
#define LDK_SCENE_SYSTEMS_H

#include <ldk_scene.h>
#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * System ids belonging to the open scene, not to the scene catalog.
 * Initialize with {0}. The list owns its storage and must be cleared when
 * the scene is discarded. Unknown ids are retained.
 */
typedef struct LDKSceneSystems
{
  u64 *ids;
  u32 count;
  u32 capacity;
} LDKSceneSystems;

LDK_API void ldk_scene_systems_clear(LDKSceneSystems *systems);
LDK_API bool ldk_scene_systems_contains(const LDKSceneSystems *systems, u64 id);
LDK_API bool ldk_scene_systems_add(LDKSceneSystems *systems, u64 id);
LDK_API bool ldk_scene_systems_remove(LDKSceneSystems *systems, u64 id);

/**
 * Reads the optional scene.systems node without creating ECS entities.
 * An absent node produces an empty list. On failure, out is unchanged.
 * Accepted ids are positive signed integers or strings representing u64s.
 * The writer uses quoted hexadecimal strings to support the full u64 range.
 *
 *   scene:
 *     systems:
 *       - id: "0x0000000000000100"
 */
LDK_API bool ldk_scene_systems_from_tml(
    const char *source, LDKSceneSystems *out, LDKSceneResult *result);
LDK_API bool ldk_scene_systems_load_tml_file(
    const char *path, LDKSceneSystems *out, LDKSceneResult *result);

/**
 * Reconcile the active gameplay systems with a scene's requested ids.
 * stop_missing is called before destroying the old scene's entities;
 * start is called after the new scene's entities have been loaded.
 * Start callbacks follow registry registration order; the scene list itself
 * is an association set, not an initialization-order list.
 */
LDK_API bool ldk_scene_systems_stop_missing(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);
LDK_API bool ldk_scene_systems_start(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);

#ifdef LDK_EDITOR
/**
 * Serializes the current ECS and the supplied scene associations. The
 * existing entity serializer remains responsible for entities/components.
 */
LDK_API bool ldk_scene_systems_to_tml(XStrBuilder *out,
    const LDKSceneSystems *systems, LDKSceneResult *result);
LDK_API bool ldk_scene_systems_save_tml_file(const char *path,
    const LDKSceneSystems *systems, LDKSceneResult *result);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_SYSTEMS_H
